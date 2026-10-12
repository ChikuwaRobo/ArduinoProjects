# Compile the sketch with Arduino CLI; do not upload firmware.
param([string]$Cli = "arduino-cli")
$ErrorActionPreference = "Stop"
$projectDir = Split-Path $PSScriptRoot -Parent
& $Cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc" --build-path "$projectDir\build" $projectDir
if ($LASTEXITCODE -ne 0) { throw "Arduino build failed: $LASTEXITCODE" }
# Match Arduino core's upload recipe without depending on the user's install path.
$buildOptions = Get-Content -Raw -LiteralPath "$projectDir\build\build.options.json" | ConvertFrom-Json
$platformDir = $buildOptions.hardwareFolders -split ',' | Where-Object { Test-Path -LiteralPath (Join-Path $_ "tools\partitions\boot_app0.bin") } | Select-Object -First 1
if (-not $platformDir) { throw "ESP32 core 3.2.0 partition image was not found" }
Copy-Item -LiteralPath (Join-Path $platformDir "tools\partitions\boot_app0.bin") -Destination "$projectDir\build\boot_app0.bin"
