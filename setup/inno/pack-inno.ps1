#Requires -Version 5.1
# Build shipping Inno installer from dist-win (Botva2 UI).

param(
    [string]$Version = "",
    [string]$DistDir = "",
    [string]$OutputPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ROOT = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if (-not $DistDir) {
    $DistDir = Join-Path $ROOT "dist-win"
}
if (-not $Version) {
    if ($env:ARACHNEL_VERSION) {
        $Version = $env:ARACHNEL_VERSION
    } elseif ($env:INPUT_VERSION) {
        $Version = $env:INPUT_VERSION
    }
}
if ($Version) {
    $Version = $Version.Trim()
    if ($Version.StartsWith('v') -or $Version.StartsWith('V')) {
        $Version = $Version.Substring(1)
    }
}
if (-not $Version -or $Version -eq 'dev' -or $Version -eq '0.0.0-dev') {
    throw "pack-inno: refuse version '$Version'. Pass -Version / ARACHNEL_VERSION (e.g. 0.1.39)."
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $ROOT "Arachnel-Setup.exe"
}

$appExe = Join-Path $DistDir "arachnel_app.exe"
if (-not (Test-Path -LiteralPath $appExe)) {
    throw "dist-win missing arachnel_app.exe at $DistDir. Run .\run.ps1 --package first."
}

$skinScript = Join-Path $PSScriptRoot "skin\gen-skin.ps1"
$bg = Join-Path $PSScriptRoot "skin\background.bmp"
if (-not (Test-Path -LiteralPath $bg)) {
    Write-Host "Generating Botva2 skin assets ..."
    & $skinScript
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$iscc = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $iscc) {
    throw "ISCC.exe not found. Install Inno Setup 6."
}

Write-Host "Building Inno installer (version=$Version, dist=$DistDir) ..."
& $iscc "/DMyAppVersion=$Version" (Join-Path $PSScriptRoot "Arachnel.iss")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$built = Join-Path $PSScriptRoot "output\Arachnel-$Version-Setup.exe"
if (-not (Test-Path -LiteralPath $built)) {
    throw "ISCC output missing: $built"
}

Copy-Item -LiteralPath $built -Destination $OutputPath -Force

$signScript = Join-Path $ROOT "scripts\ci\sign-windows.ps1"
if ($env:WINDOWS_SIGN_CERT_PFX_BASE64) {
    Write-Host "Signing $OutputPath ..."
    & $signScript -Files @($OutputPath)
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "Code signing skipped (WINDOWS_SIGN_CERT_PFX_BASE64 is not set)."
}

Write-Host "Done: $OutputPath" -ForegroundColor Green
Write-Host "Also: $built"
