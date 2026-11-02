param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$executable = Join-Path $projectRoot "out/bin/$Configuration/RHITest.exe"
$outputDirectory = Join-Path $projectRoot "out/stage-b/$Configuration"
if (-not (Test-Path -LiteralPath $executable)) { throw "Build RHITest $Configuration first." }
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$report = Join-Path $outputDirectory 'stage-b-validation.txt'
# 清除旧报告，避免启动失败时误读上一轮 PASS。
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report }
$validationProcess = Start-Process -FilePath $executable -ArgumentList '--validate' -WorkingDirectory $outputDirectory -WindowStyle Hidden -PassThru
if (-not $validationProcess.WaitForExit(60000)) {
    Stop-Process -Id $validationProcess.Id
    throw 'Stage B validation timed out after 60 seconds.'
}
if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report }
if ($validationProcess.ExitCode -ne 0) { throw "Stage B validation failed: exit $($validationProcess.ExitCode)" }
if (-not (Test-Path -LiteralPath $report)) { throw 'Validation report missing.' }
Get-Content -LiteralPath (Join-Path $outputDirectory 'stage-b-checks.txt')
Get-Content -LiteralPath (Join-Path $outputDirectory 'stage-a-checks.txt')
Write-Output "Readback image: $(Join-Path $outputDirectory 'stage-b-frame.ppm')"
