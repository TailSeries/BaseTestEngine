param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$report = Join-Path $projectRoot "out/stage-b/$Configuration/stage-c-compiler-checks.txt"
$parameterReport = Join-Path $projectRoot "out/stage-b/$Configuration/stage-c-parameter-checks.txt"
if (Test-Path -LiteralPath $parameterReport) { Remove-Item -LiteralPath $parameterReport }
# 清理本节旧报告，防止把上一次成功当成本次通过；A/B 脚本负责进程超时及退出检查。
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report }
& (Join-Path $PSScriptRoot 'RunStageBValidation.ps1') -Configuration $Configuration
if (-not (Test-Path -LiteralPath $report)) { throw 'Stage C compiler report missing.' }
Get-Content -LiteralPath $report

if (-not (Test-Path -LiteralPath $parameterReport)) { throw "Stage C parameter report missing." }
Get-Content -LiteralPath $parameterReport
