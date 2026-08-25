[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$venv = Join-Path $repoRoot '.tools\python-venv'
$python = Join-Path $venv 'Scripts\python.exe'
if (-not (Test-Path -LiteralPath $python)) {
    $launcher = Get-Command py -ErrorAction SilentlyContinue
    if ($launcher) {
        & $launcher.Source -3 -m venv $venv
    } else {
        & python -m venv $venv
    }
    if ($LASTEXITCODE -ne 0) { throw 'Unable to create Python virtual environment.' }
}
& $python -m pip install --disable-pip-version-check -r (Join-Path $repoRoot 'requirements-tools.txt')
if ($LASTEXITCODE -ne 0) { throw 'Python tool installation failed.' }
Write-Output $python
