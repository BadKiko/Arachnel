#Requires -Version 5.1
# Pack freetp plugin for manual install (zip with plugin.json + dll + games.json)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$BuildPlugins = Join-Path $Root "build-win\plugins\freetp"
$OutDir = Join-Path $Root "dist"
$ZipPath = Join-Path $OutDir "freetp-plugin-win.zip"

if (-not (Test-Path $BuildPlugins)) {
    Write-Error "Build plugins first: .\run.ps1 — expected $BuildPlugins"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }

$staging = Join-Path $env:TEMP "arachnel-freetp-pack"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $staging "freetp") | Out-Null
Copy-Item "$BuildPlugins\*" (Join-Path $staging "freetp") -Recurse

Compress-Archive -Path (Join-Path $staging "freetp") -DestinationPath $ZipPath -Force
Remove-Item $staging -Recurse -Force

Write-Host "Created $ZipPath"
Write-Host "Install: Settings -> Plugins -> Install from ZIP"
