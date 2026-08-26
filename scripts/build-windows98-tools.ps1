[CmdletBinding()]
param(
    [string]$WatcomRoot = $env:WATCOM,
    [string]$OutputDirectory = 'build\windows98'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if ([string]::IsNullOrWhiteSpace($WatcomRoot)) {
    throw 'Set WATCOM to an Open Watcom v2 installation or pass -WatcomRoot.'
}
$compiler = Join-Path $WatcomRoot 'binnt64\wcl386.exe'
if (-not (Test-Path -LiteralPath $compiler)) {
    $compiler = Join-Path $WatcomRoot 'binnt\wcl386.exe'
}
if (-not (Test-Path -LiteralPath $compiler)) {
    throw "Open Watcom wcl386.exe not found under: $WatcomRoot"
}

$output = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
New-Item -ItemType Directory -Force -Path $output | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'tools\windows98\W98TERM.C') -Destination $output -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'tools\windows98\LINEPASS.C') -Destination $output -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'tools\windows98\W98AGENT.C') -Destination $output -Force

$savedWatcom = $env:WATCOM
$savedEdpath = $env:EDPATH
$savedInclude = $env:INCLUDE
$savedPath = $env:PATH
try {
    $env:WATCOM = $WatcomRoot
    $env:EDPATH = Join-Path $WatcomRoot 'eddat'
    $env:INCLUDE = (Join-Path $WatcomRoot 'h\nt') + ';' + (Join-Path $WatcomRoot 'h')
    $env:PATH = (Join-Path $WatcomRoot 'binnt64') + ';' +
                (Join-Path $WatcomRoot 'binnt') + ';' + $env:PATH
    Push-Location $output
    try {
        foreach ($source in @('W98TERM.C', 'LINEPASS.C', 'W98AGENT.C')) {
            & $compiler -q -bt=nt -l=nt -dWINVER=0x0400 -d_WIN32_WINNT=0x0400 -6r -ox -s $source
            if ($LASTEXITCODE -ne 0) {
                throw "Open Watcom failed while compiling $source."
            }
        }
    } finally {
        Pop-Location
    }
} finally {
    $env:WATCOM = $savedWatcom
    $env:EDPATH = $savedEdpath
    $env:INCLUDE = $savedInclude
    $env:PATH = $savedPath
}

Get-FileHash (Join-Path $output 'W98TERM.EXE'),
             (Join-Path $output 'LINEPASS.EXE'),
             (Join-Path $output 'W98AGENT.EXE') -Algorithm SHA256
