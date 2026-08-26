[CmdletBinding()]
param(
    [string]$Binary = 'release\esp8266-serial-wifi-bridge-v0.3.0-rc1.bin'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$binaryPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Binary))
$releaseRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'release'))
if (-not $binaryPath.StartsWith($releaseRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Release verification only accepts a file under release/.'
}
if (-not (Test-Path -LiteralPath $binaryPath)) { throw "Binary not found: $binaryPath" }

$manifest = Join-Path $releaseRoot 'SHA256SUMS.txt'
$name = Split-Path $binaryPath -Leaf
$line = Get-Content -LiteralPath $manifest | Where-Object { $_ -match "  $([regex]::Escape($name))$" }
if (-not $line) { throw "No checksum entry for $name" }
$expected = ($line -split '\s+')[0].ToUpperInvariant()
$actual = (Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash
if ($actual -ne $expected) { throw "Hash mismatch. Expected $expected, got $actual" }

$item = Get-Item -LiteralPath $binaryPath
Write-Host "VERIFIED: $($item.FullName)"
Write-Host "Length: $($item.Length) bytes"
Write-Host "SHA-256: $actual"
