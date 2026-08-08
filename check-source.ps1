param(
    [string]$RepoPath = "F:\workspace\BaseTestEngine",
    [string]$SourceDir = "Source",
    [string]$LogFile = "$env:TEMP\codex-source-watch.log",
    [string]$StateFile = "$env:TEMP\codex-source-watch-state.txt",
    [int]$CheckIntervalSeconds = 300,
    [int]$StaleWarningMinutes = 60
)

function Write-Log {
    param([string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    "$timestamp - $Message" | Out-File -FilePath $LogFile -Append -Encoding UTF8
    Write-Host "$timestamp - $Message"
}

function Get-LastCommitTime {
    Push-Location $RepoPath
    $lastCommit = git log -1 --format="%ct" -- "$SourceDir/"
    Pop-Location
    if ($lastCommit) {
        return [DateTimeOffset]::FromUnixTimeSeconds([long]$lastCommit).LocalDateTime
    }
    return $null
}

function Get-StateTimestamp {
    if (Test-Path $StateFile) {
        $content = Get-Content $StateFile -Raw -ErrorAction SilentlyContinue
        if ($content -match "(\d+)") {
            return [DateTimeOffset]::FromUnixTimeSeconds([long]$Matches[1]).LocalDateTime
        }
    }
    return $null
}

function Set-StateTimestamp {
    param([DateTime]$Time)
    $unix = [DateTimeOffset]::new($Time).ToUnixTimeSeconds()
    $unix.ToString() | Out-File -FilePath $StateFile -Force
}

function Invoke-Commit {
    Push-Location $RepoPath
    $message = "Auto-commit: Source changes at " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
    git add "$SourceDir/"
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        return @{Success=$false; Message="git add failed with exit code $LASTEXITCODE"}
    }
    git commit -m $message
    $commitExit = $LASTEXITCODE
    if ($commitExit -eq 0) {
        $hash = git rev-parse HEAD
        Pop-Location
        Set-StateTimestamp (Get-Date)
        return @{Success=$true; Message="Commit succeeded: $hash`n  $message"}
    } else {
        $output = git status --porcelain
        Pop-Location
        if ([string]::IsNullOrWhiteSpace($output)) {
            return @{Success=$true; Message="No changes to commit"}
        }
        return @{Success=$false; Message="git commit failed with exit code $commitExit"}
    }
}

# === Main ===
$lastCommitTime = Get-LastCommitTime
$lastStateTime = Get-StateTimestamp
if ($lastStateTime -and $lastCommitTime) {
    $ticks = [Math]::Max($lastStateTime.Ticks, $lastCommitTime.Ticks)
    $effectiveLastTime = [DateTime]::new($ticks)
} elseif ($lastStateTime) {
    $effectiveLastTime = $lastStateTime
} elseif ($lastCommitTime) {
    $effectiveLastTime = $lastCommitTime
} else {
    $effectiveLastTime = (Get-Date).AddHours(-2)
}

$result = Invoke-Commit
if ($result.Success) {
    Write-Log $result.Message
} else {
    Write-Log "COMMIT FAILED: $($result.Message)"
}


$elapsed = [DateTime]::Now - $effectiveLastTime
if ($elapsed.TotalMinutes -ge $StaleWarningMinutes) {
    Write-Log "WARNING: No commit for " + [math]::Round($elapsed.TotalMinutes) + " minutes (threshold: " + $StaleWarningMinutes + "m). Source directory may be idle."
}

Write-Log "Check completed."