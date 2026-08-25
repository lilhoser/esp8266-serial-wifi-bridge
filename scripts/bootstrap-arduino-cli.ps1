[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$version = '1.5.1'
$archiveName = "arduino-cli_${version}_Windows_64bit.zip"
$expectedHash = 'FABE42E0EB04D00E776A66178299FF95A46C623DBC260F997E58FD514853DD40'
$repoRoot = Split-Path $PSScriptRoot -Parent
$toolsRoot = Join-Path $repoRoot '.tools'
$cliRoot = Join-Path $toolsRoot "arduino-cli-$version"
$cli = Join-Path $cliRoot 'arduino-cli.exe'
$archive = Join-Path $toolsRoot $archiveName
$config = Join-Path $toolsRoot 'arduino-cli.yaml'
$packageUrl = 'https://arduino.esp8266.com/stable/package_esp8266com_index.json'

New-Item -ItemType Directory -Force -Path $toolsRoot | Out-Null
if (-not (Test-Path -LiteralPath $cli)) {
    $url = "https://github.com/arduino/arduino-cli/releases/download/v$version/$archiveName"
    Write-Host "Downloading $url"
    Invoke-WebRequest -Uri $url -OutFile $archive
    $actualHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    if ($actualHash -ne $expectedHash) {
        throw "Arduino CLI archive hash mismatch. Expected $expectedHash, got $actualHash"
    }
    New-Item -ItemType Directory -Force -Path $cliRoot | Out-Null
    Expand-Archive -LiteralPath $archive -DestinationPath $cliRoot -Force
}

$data = (Join-Path $toolsRoot 'arduino-data').Replace('\', '/')
$downloads = (Join-Path $toolsRoot 'arduino-downloads').Replace('\', '/')
$user = (Join-Path $toolsRoot 'arduino-user').Replace('\', '/')
@"
board_manager:
  additional_urls:
    - $packageUrl
directories:
  data: $data
  downloads: $downloads
  user: $user
"@ | Set-Content -LiteralPath $config -Encoding ascii

$installed = & $cli --config-file $config core list --format json | ConvertFrom-Json
$hasCore = $false
foreach ($platform in $installed.platforms) {
    if ($platform.id -eq 'esp8266:esp8266' -and $platform.installed_version -eq '3.1.2') {
        $hasCore = $true
    }
}
if (-not $hasCore) {
    & $cli --config-file $config core update-index
    if ($LASTEXITCODE -ne 0) { throw 'Arduino core index update failed.' }
    & $cli --config-file $config core install 'esp8266:esp8266@3.1.2'
    if ($LASTEXITCODE -ne 0) { throw 'ESP8266 core installation failed.' }
}

Write-Host "Arduino CLI ready: $cli"
Write-Output $cli
