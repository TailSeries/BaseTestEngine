param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$QtRoot = '',
    [switch]$Run,
    [switch]$Validate
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $QtRoot) { $QtRoot = Join-Path $projectRoot 'out/Qt/6.8.3/msvc2022_64' }
if (-not (Test-Path -LiteralPath (Join-Path $QtRoot 'lib/cmake/Qt6/Qt6Config.cmake'))) {
    throw "Qt SDK not found: $QtRoot. Run Source/Editor/SetupQt.ps1, or pass -QtRoot."
}
$buildDirectory = Join-Path $projectRoot 'out/build/qt-editor'
$configureArguments = @('-S', (Join-Path $projectRoot 'Source'), '-B', $buildDirectory,
    '-DBASETEST_BUILD_EDITOR=ON', "-DCMAKE_PREFIX_PATH=$QtRoot")
# 已有缓存沿用其生成器平台，避免与 Visual Studio 默认 x64 的空 platform 字段冲突。
if (-not (Test-Path -LiteralPath (Join-Path $buildDirectory 'CMakeCache.txt'))) {
    $configureArguments += @('-A', 'x64')
}
& cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'Editor CMake configuration failed.' }
& cmake --build $buildDirectory --config $Configuration --target BaseTestEditor RHITest -j 8
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed.' }
$editorExecutable = Join-Path $projectRoot "out/bin/$Configuration/BaseTestEditor.exe"
if ($Validate) {
    $outputDirectory = Join-Path $projectRoot "out/editor-smoke/$Configuration"
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
    $reportPath = Join-Path $outputDirectory 'editor-smoke.txt'
    # 报告先置为待执行状态，不能把旧 PASS 当作本次结果。
    'RUNNING' | Set-Content -Encoding utf8 -LiteralPath $reportPath
    $editorProcess = Start-Process -FilePath $editorExecutable `
        -ArgumentList @('--smoke-test', '--smoke-output', ('"' + $outputDirectory + '"')) `
        -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    if (-not $editorProcess.WaitForExit(45000)) {
        Stop-Process -Id $editorProcess.Id
        throw 'Editor integration validation timed out.'
    }
    Get-Content -LiteralPath $reportPath
    if ($editorProcess.ExitCode -ne 0 -or -not ((Get-Content -LiteralPath $reportPath -Raw).StartsWith('PASS:'))) {
        throw "Editor validation failed. See out/bin/$Configuration/Logs/Editor.log."
    }
}
if ($Run) { & $editorExecutable }
Write-Output "Editor executable: $editorExecutable"
