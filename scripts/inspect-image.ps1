[CmdletBinding()]
param(
    [string]$Binary = 'release\esp8266-serial-wifi-bridge-v0.1.0.bin'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$binaryPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Binary))
if (-not (Test-Path -LiteralPath $binaryPath)) { throw "Binary not found: $binaryPath" }
$python = & (Join-Path $PSScriptRoot 'install-python-tools.ps1') | Select-Object -Last 1
& $python -m esptool image-info $binaryPath
if ($LASTEXITCODE -ne 0) { throw 'Image inspection failed.' }
