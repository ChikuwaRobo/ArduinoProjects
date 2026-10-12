# Build, flash over native USB, then check the running firmware.
param(
    [string]$Port = "",
    [switch]$Initial,
    [string]$Cli = "",
    [string]$Python = ""
)
$ErrorActionPreference = "Stop"
$projectDir = Split-Path $PSScriptRoot -Parent
if (-not $Cli) {
    $localCli = Join-Path $projectDir ".tools\arduino-cli.exe"
    $Cli = if (Test-Path -LiteralPath $localCli) { $localCli } else { "arduino-cli" }
}
if (-not $Python) {
    $Python = "python"
}
& (Join-Path $PSScriptRoot "build.ps1") -Cli $Cli
$flashArguments = @((Join-Path $PSScriptRoot "usb_update.py"))
if ($Port) { $flashArguments += @("--port", $Port) }
if ($Initial) { $flashArguments += "--initial" }
& $Python @flashArguments
if ($LASTEXITCODE -ne 0) { throw "USB firmware update failed: $LASTEXITCODE" }
