param([string]$Version = '6.8.3')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$qtOutput = Join-Path $projectRoot 'out/Qt'
$qtPrefix = Join-Path $qtOutput "$Version/msvc2022_64"
if ((Test-Path -LiteralPath (Join-Path $qtPrefix 'lib/cmake/Qt6QuickControls2/Qt6QuickControls2Config.cmake')) -and
    (Test-Path -LiteralPath (Join-Path $qtPrefix 'lib/cmake/Qt6ShaderTools/Qt6ShaderToolsConfig.cmake')) -and
    (Test-Path -LiteralPath (Join-Path $qtPrefix 'bin/lconvert.exe'))) {
    Write-Output "Qt SDK already present: $qtPrefix"
    return
}
# SDK 和下载工具均保存在 out 下，不修改全局 Python 环境或系统 PATH。
$venv = Join-Path $projectRoot 'out/qt-tools'
if (-not (Test-Path -LiteralPath (Join-Path $venv 'Scripts/python.exe'))) {
    & python -m venv $venv
    if ($LASTEXITCODE -ne 0) { throw 'Cannot create Qt download tool environment.' }
}
$toolPython = Join-Path $venv 'Scripts/python.exe'
& $toolPython -m pip install --disable-pip-version-check 'aqtinstall==3.3.0'
if ($LASTEXITCODE -ne 0) { throw 'Cannot install aqtinstall.' }
& $toolPython -m aqt install-qt windows desktop $Version win64_msvc2022_64 `
    -O $qtOutput --archives qtbase qtdeclarative qttools qtsvg -m qtshadertools --timeout 120
if ($LASTEXITCODE -ne 0) { throw 'Qt SDK download failed.' }
Write-Output "Qt SDK ready: $qtPrefix"
