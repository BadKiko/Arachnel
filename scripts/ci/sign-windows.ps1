#Requires -Version 5.1

param(
    [Parameter(Mandatory = $true)]
    [string[]]$Files
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-SignTool {
    $fromPath = Get-Command signtool.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
    if ($fromPath) { return $fromPath }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $kitsRoot = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -find "Windows Kits\10\bin\*\x64\signtool.exe" 2>$null |
            Sort-Object -Descending | Select-Object -First 1
        if ($kitsRoot) { return $kitsRoot }
    }

    throw "signtool.exe not found. Install Windows SDK / Visual Studio Build Tools."
}

if (-not $env:WINDOWS_SIGN_CERT_PFX_BASE64) {
    Write-Host "Code signing skipped (WINDOWS_SIGN_CERT_PFX_BASE64 is not set)."
    exit 0
}

if (-not $env:WINDOWS_SIGN_CERT_PASSWORD) {
    throw "WINDOWS_SIGN_CERT_PASSWORD is required when WINDOWS_SIGN_CERT_PFX_BASE64 is set."
}

$pfxPath = Join-Path $env:RUNNER_TEMP "arachnel-sign.pfx"
$pfxBytes = [Convert]::FromBase64String($env:WINDOWS_SIGN_CERT_PFX_BASE64)
[IO.File]::WriteAllBytes($pfxPath, $pfxBytes)

try {
    $signtool = Resolve-SignTool
    $timestamp = if ($env:WINDOWS_SIGN_TIMESTAMP_URL) {
        $env:WINDOWS_SIGN_TIMESTAMP_URL
    } else {
        "http://timestamp.digicert.com"
    }

    foreach ($file in $Files) {
        if (-not (Test-Path -LiteralPath $file)) {
            throw "File to sign not found: $file"
        }
        Write-Host "Signing $file ..."
        & $signtool sign `
            /f $pfxPath `
            /p $env:WINDOWS_SIGN_CERT_PASSWORD `
            /tr $timestamp `
            /td sha256 `
            /fd sha256 `
            /a `
            $file
        if ($LASTEXITCODE -ne 0) {
            throw "signtool failed for $file (exit $LASTEXITCODE)"
        }
    }
}
finally {
    Remove-Item -LiteralPath $pfxPath -Force -ErrorAction SilentlyContinue
}

Write-Host "Signing complete."
