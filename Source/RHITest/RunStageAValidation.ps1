param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)
# 兼容旧入口：阶段 A 的验收用例现在随阶段 B 完整管线一起执行。
& (Join-Path $PSScriptRoot 'RunStageBValidation.ps1') -Configuration $Configuration
