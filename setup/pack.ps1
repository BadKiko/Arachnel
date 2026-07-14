#Requires -Version 5.1

param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\build-win"),
    [string]$DistDir = (Join-Path $PSScriptRoot "..\dist-win"),
    [string]$OutputPath = (Join-Path $PSScriptRoot "..\Arachnel-Setup.exe"),
    [string]$QtPrefix = $env:CMAKE_PREFIX_PATH
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ROOT = Resolve-Path (Join-Path $PSScriptRoot "..")
$SETUP_QML = Join-Path $PSScriptRoot "qml"
$STAGING = Join-Path $PSScriptRoot "staging"
$RUNTIME_DIR = Join-Path $STAGING "runtime"

function Resolve-UninstallExe {
    param([string]$Dir)
    $candidates = @(
        (Join-Path $Dir "uninstall.exe"),
        (Join-Path $Dir "RelWithDebInfo\uninstall.exe"),
        (Join-Path $Dir "Release\uninstall.exe")
    )
    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    throw "uninstall.exe not found in $Dir"
}

function Resolve-LauncherExe {
    param([string]$Dir)
    $candidates = @(
        (Join-Path $Dir "arachnel_setup_launcher.exe"),
        (Join-Path $Dir "RelWithDebInfo\arachnel_setup_launcher.exe"),
        (Join-Path $Dir "Release\arachnel_setup_launcher.exe")
    )
    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    throw "arachnel_setup_launcher.exe not found in $Dir"
}

function Resolve-SetupExe {
    param([string]$Dir)
    $candidates = @(
        (Join-Path $Dir "arachnel_setup.exe"),
        (Join-Path $Dir "RelWithDebInfo\arachnel_setup.exe"),
        (Join-Path $Dir "Release\arachnel_setup.exe")
    )
    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    throw "arachnel_setup.exe not found in $Dir"
}

function Write-U64Le {
    param([byte[]]$Buffer, [int]$Offset, [uint64]$Value)
    for ($i = 0; $i -lt 8; $i++) {
        $Buffer[$Offset + $i] = [byte]($Value -band 0xFF)
        $Value = $Value -shr 8
    }
}

function Append-PayloadFooter {
    param(
        [string]$ExePath,
        [uint64]$RuntimeOffset,
        [uint64]$RuntimeSize,
        [uint64]$AppOffset,
        [uint64]$AppSize
    )

    $footer = New-Object byte[] 40
    [Text.Encoding]::ASCII.GetBytes("ARACHPK1").CopyTo($footer, 0)
    Write-U64Le $footer 8 $RuntimeOffset
    Write-U64Le $footer 16 $RuntimeSize
    Write-U64Le $footer 24 $AppOffset
    Write-U64Le $footer 32 $AppSize

    $stream = [IO.File]::Open($ExePath, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $stream.Write($footer, 0, $footer.Length)
    }
    finally {
        $stream.Close()
    }
}

if (-not (Test-Path -LiteralPath $DistDir)) {
    throw "dist-win not found at $DistDir. Run .\run.ps1 --package first."
}

$distApp = Join-Path $DistDir "arachnel_app.exe"
if (-not (Test-Path -LiteralPath $distApp)) {
    throw "dist-win is missing arachnel_app.exe. Run .\run.ps1 --package first."
}

$setupExe = Resolve-SetupExe $BuildDir
$launcherExe = Resolve-LauncherExe $BuildDir
$uninstallExe = Resolve-UninstallExe $BuildDir

if (-not $QtPrefix) {
    throw "Qt prefix not set. Pass -QtPrefix or set CMAKE_PREFIX_PATH."
}

$windeployqt = Join-Path $QtPrefix "bin\windeployqt.exe"
if (-not (Test-Path -LiteralPath $windeployqt)) {
    throw "windeployqt not found at $windeployqt"
}

if (Test-Path -LiteralPath $STAGING) {
    Remove-Item -LiteralPath $STAGING -Recurse -Force
}
New-Item -ItemType Directory -Path $RUNTIME_DIR | Out-Null

Copy-Item -LiteralPath $setupExe -Destination (Join-Path $RUNTIME_DIR "arachnel_setup.exe") -Force
Copy-Item -LiteralPath $uninstallExe -Destination (Join-Path $RUNTIME_DIR "uninstall.exe") -Force

@"
[Paths]
Prefix=.
Plugins=.
Qml2Imports=qml
Imports=qml
"@ | Set-Content -LiteralPath (Join-Path $RUNTIME_DIR "qt.conf") -Encoding ASCII

$qmlModulesBuilt = Join-Path $BuildDir "qml_modules"
if (Test-Path -LiteralPath $qmlModulesBuilt) {
    Copy-Item -LiteralPath $qmlModulesBuilt -Destination (Join-Path $RUNTIME_DIR "qml_modules") -Recurse -Force
}

$runtimeExe = Join-Path $RUNTIME_DIR "arachnel_setup.exe"
Write-Host "Deploying Qt runtime for installer ..."
& $windeployqt --qmldir $SETUP_QML --qmldir $qmlModulesBuilt --no-translations $runtimeExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed for installer runtime (exit $LASTEXITCODE)"
}

$qmlMaterialDll = Join-Path $BuildDir "qml_material.dll"
if (Test-Path -LiteralPath $qmlMaterialDll) {
    Copy-Item -LiteralPath $qmlMaterialDll -Destination (Join-Path $RUNTIME_DIR "qml_material.dll") -Force
}

$runtimeZip = Join-Path $STAGING "runtime.zip"
$appZip = Join-Path $STAGING "app.zip"
if (Test-Path -LiteralPath $runtimeZip) { Remove-Item -LiteralPath $runtimeZip -Force }
if (Test-Path -LiteralPath $appZip) { Remove-Item -LiteralPath $appZip -Force }

Write-Host "Compressing installer runtime ..."
Compress-Archive -Path (Join-Path $RUNTIME_DIR "*") -DestinationPath $runtimeZip -CompressionLevel Optimal

Write-Host "Compressing app payload from dist-win ..."
Compress-Archive -Path (Join-Path $DistDir "*") -DestinationPath $appZip -CompressionLevel Optimal

if (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath -Force
}
Copy-Item -LiteralPath $launcherExe -Destination $OutputPath -Force

$baseSize = (Get-Item -LiteralPath $OutputPath).Length
$runtimeSize = (Get-Item -LiteralPath $runtimeZip).Length
$appSize = (Get-Item -LiteralPath $appZip).Length

$runtimeOffset = [uint64]$baseSize
$appOffset = [uint64]($baseSize + $runtimeSize)

Write-Host "Appending embedded payloads ..."
foreach ($zipPath in @($runtimeZip, $appZip)) {
    $bytes = [IO.File]::ReadAllBytes($zipPath)
    $stream = [IO.File]::Open($OutputPath, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $stream.Write($bytes, 0, $bytes.Length)
    }
    finally {
        $stream.Close()
    }
}

Append-PayloadFooter -ExePath $OutputPath `
    -RuntimeOffset $runtimeOffset `
    -RuntimeSize ([uint64]$runtimeSize) `
    -AppOffset $appOffset `
    -AppSize ([uint64]$appSize)

Write-Host ""
Write-Host "Done: $OutputPath" -ForegroundColor Green
Write-Host ("  base={0:N0} B  runtime={1:N0} B  app={2:N0} B" -f $baseSize, $runtimeSize, $appSize)
