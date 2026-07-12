#Requires -Version 5.1
# Pack freetp plugin as .arach (ZIP with plugin.json + dll + games-arachnel.json)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$BuildPlugins = Join-Path $Root "build-win\plugin-build\freetp"
$OutDir = Join-Path $Root "build-win\dist"
$ArachPath = Join-Path $OutDir "freetp.arach"

if (-not (Test-Path $BuildPlugins)) {
    Write-Error "Build plugins first: .\run.ps1 — expected $BuildPlugins"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (Test-Path $ArachPath) { Remove-Item $ArachPath -Force }

$staging = Join-Path $env:TEMP "arachnel-freetp-pack"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $staging "freetp") | Out-Null
Copy-Item "$BuildPlugins\*" (Join-Path $staging "freetp") -Recurse

Compress-Archive -Path (Join-Path $staging "freetp") -DestinationPath $ArachPath -Force
Remove-Item $staging -Recurse -Force

Write-Host "Created $ArachPath"
Write-Host "Install: Settings -> Plugins -> Install .arach"
