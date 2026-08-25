[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot 'build' }
$cli = & (Join-Path $PSScriptRoot 'bootstrap-arduino-cli.ps1') | Select-Object -Last 1
$config = Join-Path $repoRoot '.tools\arduino-cli.yaml'
$fqbn = 'esp8266:esp8266:generic:eesz=4M,FlashMode=dout,FlashFreq=40,CrystalFreq=26,wipe=none'

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
& $cli --config-file $config compile --fqbn $fqbn --output-dir $OutputDirectory $repoRoot
if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }

$image = Join-Path $OutputDirectory 'esp8266-serial-wifi-bridge.ino.bin'
$item = Get-Item -LiteralPath $image
$hash = (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash
Write-Host "Built: $($item.FullName)"
Write-Host "Length: $($item.Length) bytes"
Write-Host "SHA-256: $hash"
