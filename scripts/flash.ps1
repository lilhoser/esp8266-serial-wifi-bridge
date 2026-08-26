[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [string]$Binary = 'release\esp8266-serial-wifi-bridge-v0.2.0-rc1.bin',
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$binaryPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Binary))
$releaseRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'release'))
if (-not $binaryPath.StartsWith($releaseRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'For safety, the flash script accepts only a checksummed file under release/.'
}

& (Join-Path $PSScriptRoot 'verify-release.ps1') -Binary $Binary
$cli = & (Join-Path $PSScriptRoot 'bootstrap-arduino-cli.ps1') | Select-Object -Last 1
$config = Join-Path $repoRoot '.tools\arduino-cli.yaml'
$fqbn = 'esp8266:esp8266:generic:eesz=4M,FlashMode=dout,FlashFreq=40,CrystalFreq=26,wipe=none'
$item = Get-Item -LiteralPath $binaryPath
$hash = (Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash

Write-Host ''
Write-Host 'FLASH PLAN'
Write-Host "  Port:   $Port"
Write-Host "  Image:  $($item.FullName)"
Write-Host "  Length: $($item.Length) bytes"
Write-Host "  SHA256: $hash"
Write-Host "  Target: ESP8266, 4 MB, DOUT, 40 MHz, 26 MHz crystal"
Write-Host '  Erase:  sketch/application sectors only; no full-chip erase requested'
Write-Host ''

if (-not $Yes) {
    $answer = Read-Host 'Type FLASH to continue'
    if ($answer -cne 'FLASH') { throw 'Cancelled; nothing was written.' }
}

& $cli --config-file $config upload --port $Port --fqbn $fqbn --input-file $binaryPath --verify
if ($LASTEXITCODE -ne 0) { throw 'Upload or verification failed.' }
Write-Host 'UPLOAD VERIFIED.'
