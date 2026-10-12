# Download the official pinned Windows esptool package and check its release digest.
param([string]$Python = "python")
$ErrorActionPreference = "Stop"
$projectDir = Split-Path $PSScriptRoot -Parent
$toolDir = Join-Path $projectDir ".tools"
New-Item -ItemType Directory -Force -Path $toolDir | Out-Null
$release = Invoke-RestMethod "https://api.github.com/repos/espressif/esptool/releases/tags/v5.5.0" -TimeoutSec 30
$asset = $release.assets | Where-Object name -eq "esptool-v5.5.0-windows-amd64.zip"
if (-not $asset -or $asset.digest -notmatch '^sha256:([0-9a-f]{64})$') { throw "Official release digest is missing" }
$expectedHash = $Matches[1]
$archive = Join-Path $toolDir $asset.name
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest $asset.browser_download_url -OutFile $archive -TimeoutSec 120
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) { throw "esptool download hash mismatch" }
Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $toolDir "esptool-v5.5.0") -Force
& $Python -c "import serial; print('pyserial', serial.__version__)"
if ($LASTEXITCODE -ne 0) {
    & $Python -m pip install --index-url https://pypi.org/simple -r (Join-Path $PSScriptRoot "requirements-flash.txt")
    if ($LASTEXITCODE -ne 0) { throw "pyserial installation failed" }
}
