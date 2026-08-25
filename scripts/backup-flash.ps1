[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [string]$Output
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (-not $Output) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $Output = Join-Path $repoRoot "backup-$stamp-4mb.bin"
}
$outputPath = [System.IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $outputPath) {
    throw "Refusing to overwrite existing backup: $outputPath"
}
$python = & (Join-Path $PSScriptRoot 'install-python-tools.ps1') | Select-Object -Last 1

& $python -m esptool --chip esp8266 --port $Port flash-id
if ($LASTEXITCODE -ne 0) { throw 'Could not identify the ESP8266.' }
& $python -m esptool --chip esp8266 --port $Port read-flash 0x000000 0x400000 $outputPath
if ($LASTEXITCODE -ne 0) { throw 'Full-flash backup failed.' }

$item = Get-Item -LiteralPath $outputPath
$hash = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
Write-Host "BACKUP COMPLETE: $($item.FullName)"
Write-Host "Length: $($item.Length) bytes"
Write-Host "SHA-256: $hash"
