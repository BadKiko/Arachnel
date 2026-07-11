#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ROOT = $PSScriptRoot
$BUILD_DIR = Join-Path $ROOT "build-win"
$BUILD_TYPE = if ($env:BUILD_TYPE) { $env:BUILD_TYPE } else { "RelWithDebInfo" }
$env:QT_QML_MATERIAL_IMPORT_PATH = Join-Path $BUILD_DIR "qml_modules"
$env:QML2_IMPORT_PATH = Join-Path $BUILD_DIR "qml_modules"

function Initialize-DevPath {
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machinePath;$userPath"
}

function Resolve-Cmake {
    Initialize-DevPath
    $candidates = @(
        (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
        "D:\Qt\Tools\CMake_64\bin\cmake.exe",
        "C:\Program Files\CMake\bin\cmake.exe"
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
    if ($candidates) { return $candidates[0] }
    throw "cmake not found. Install CMake or Qt Tools (CMake_64)."
}

function Get-QtKitKind {
    param([string]$Prefix)
    if ($Prefix -match 'mingw') { return 'mingw_64' }
    if ($Prefix -match 'msvc') { return 'msvc2022_64' }
    return 'unknown'
}

function Find-QtKit {
    if ($env:CMAKE_PREFIX_PATH) {
        return @{ Prefix = $env:CMAKE_PREFIX_PATH; Kind = (Get-QtKitKind $env:CMAKE_PREFIX_PATH) }
    }

    foreach ($root in @("D:\Qt", "C:\Qt")) {
        if (-not (Test-Path -LiteralPath $root)) { continue }

        $versions = Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match "^\d+\.\d+" } |
            Sort-Object { [version]$_.Name } -Descending

        foreach ($versionDir in $versions) {
            foreach ($kitName in @("mingw_64", "msvc2022_64")) {
                $kit = Join-Path $versionDir.FullName $kitName
                $qt6Config = Join-Path $kit "lib\cmake\Qt6\Qt6Config.cmake"
                if (Test-Path -LiteralPath $qt6Config) {
                    return @{ Prefix = $kit; Kind = $kitName }
                }
            }
        }
    }

    return $null
}

function Get-BuildArgs {
    $qt = Find-QtKit
    if (-not $qt) {
        throw "Qt kit not found. Set CMAKE_PREFIX_PATH, e.g. D:\Qt\6.11.1\mingw_64"
    }

    $cmake = Resolve-Cmake
    $configureArgs = @(
        "-S", $ROOT,
        "-B", $BUILD_DIR,
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE",
        "-DCMAKE_PREFIX_PATH=$($qt.Prefix)",
        "-DARACHNEL_FAST_BUILD=$(if ($env:ARACHNEL_FAST_BUILD -eq '0') { 'OFF' } else { 'ON' })",
        "-DARACHNEL_LIBTORRENT_SHARED=$(if ($env:ARACHNEL_LIBTORRENT_SHARED -eq '0') { 'OFF' } else { 'ON' })"
    )

    if ($qt.Kind -eq "mingw_64") {
        $gcc = "D:/Qt/Tools/mingw1310_64/bin/gcc.exe"
        $gxx = "D:/Qt/Tools/mingw1310_64/bin/g++.exe"
        if (-not (Test-Path -LiteralPath ($gcc -replace '/', '\'))) {
            throw "MinGW compiler not found at D:\Qt\Tools\mingw1310_64"
        }
        $env:Path = "D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\Ninja;$env:Path"
        return @{
            Qt = $qt
            Cmake = $cmake
            Configure = $configureArgs + @("-G", "Ninja", "-DCMAKE_C_COMPILER=$gcc", "-DCMAKE_CXX_COMPILER=$gxx")
        }
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($vsPath) {
            return @{
                Qt = $qt
                Cmake = $cmake
                Configure = $configureArgs + @("-G", "Visual Studio 17 2022", "-A", "x64")
            }
        }
    }

    throw "MSVC kit found but Visual Studio Build Tools are missing."
}

function Get-AppPath {
    $candidates = @(
        (Join-Path $BUILD_DIR "arachnel_app.exe"),
        (Join-Path $BUILD_DIR "$BUILD_TYPE\arachnel_app.exe")
    )
    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    return (Join-Path $ROOT $candidates[0])
}

function Ensure-BuildDir {
    param([hashtable]$Qt)

    if (-not $Qt) { return }
    $expected = if ($Qt.Kind -eq "mingw_64") { "Ninja" } else { "Visual Studio 17 2022" }

    $cache = Join-Path $BUILD_DIR "CMakeCache.txt"
    $mismatch = $false
    if (Test-Path -LiteralPath $cache) {
        $line = Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=' | Select-Object -First 1
        if ($line -and $line.Line -notmatch [regex]::Escape($expected)) { $mismatch = $true }
    }

    if (-not $mismatch) {
        $deps = Join-Path $BUILD_DIR "_deps"
        if (Test-Path -LiteralPath $deps) {
            Get-ChildItem -Path $deps -Filter CMakeCache.txt -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
                $line = Select-String -Path $_.FullName -Pattern '^CMAKE_GENERATOR:INTERNAL=' | Select-Object -First 1
                if ($line -and $line.Line -notmatch [regex]::Escape($expected)) { $mismatch = $true }
            }
        }
    }

    if ($mismatch) {
        Write-Host "Cleaning build-win (generator mismatch)" -ForegroundColor Yellow
        Remove-Item -LiteralPath $BUILD_DIR -Recurse -Force
    }
}

function Initialize-QtRuntime {
    param([hashtable]$Qt)

    if (-not $Qt) { return }

    $qtBin = Join-Path $Qt.Prefix "bin"
    $runtimePaths = @($qtBin, "D:\Qt\Tools\mingw1310_64\bin", "D:\Qt\Tools\Ninja") |
        Where-Object { Test-Path -LiteralPath $_ }

    if ($env:VULKAN_SDK) {
        $vulkanBin = Join-Path $env:VULKAN_SDK "Bin"
        if (Test-Path -LiteralPath $vulkanBin) {
            $runtimePaths = @($vulkanBin) + $runtimePaths
        }
    } else {
        $vulkanRoots = Get-ChildItem -Path "C:\VulkanSDK" -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            Select-Object -First 1
        if ($vulkanRoots) {
            $vulkanBin = Join-Path $vulkanRoots.FullName "Bin"
            if (Test-Path -LiteralPath $vulkanBin) {
                $runtimePaths = @($vulkanBin) + $runtimePaths
            }
        }
    }

    if ($runtimePaths.Count -gt 0) {
        $env:Path = ($runtimePaths -join ";") + ";" + $env:Path
    }

    $qtPlugins = Join-Path $Qt.Prefix "plugins"
    if (Test-Path -LiteralPath $qtPlugins) {
        $env:QT_PLUGIN_PATH = $qtPlugins
    }
}

function Test-QtRuntimeDeployed {
    param([string]$AppDir)

    $required = @(
        (Join-Path $AppDir "Qt6Core.dll"),
        (Join-Path $AppDir "Qt6Gui.dll"),
        (Join-Path $AppDir "Qt6Qml.dll"),
        (Join-Path $AppDir "platforms\qwindows.dll")
    )
    foreach ($path in $required) {
        if (-not (Test-Path -LiteralPath $path)) { return $false }
    }
    return $true
}

function Deploy-QtRuntime {
    param(
        [hashtable]$Qt,
        [string]$AppPath
    )

    if (-not $Qt -or -not (Test-Path -LiteralPath $AppPath)) { return }

    $appDir = Split-Path -Parent $AppPath
    if (Test-QtRuntimeDeployed $appDir) { return }

    $windeployqt = Join-Path $Qt.Prefix "bin\windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt)) {
        Write-Host "windeployqt not found; relying on PATH for Qt DLLs" -ForegroundColor Yellow
        return
    }

    Write-Host "Deploying Qt runtime ..."
    $qmlDir = Join-Path $ROOT "qml"
    $qmlModules = Join-Path $BUILD_DIR "qml_modules"
    & $windeployqt --qmldir $qmlDir --qmldir $qmlModules --no-translations $AppPath
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE"
    }
}

function Test-NeedsCmakeConfigure {
    $cache = Join-Path $BUILD_DIR "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cache)) { return $true }

    $cacheText = Get-Content -LiteralPath $cache -Raw
    $wantShared = if ($env:ARACHNEL_LIBTORRENT_SHARED -eq '0') { 'OFF' } else { 'ON' }
    if ($cacheText -notmatch "ARACHNEL_LIBTORRENT_SHARED:BOOL=$wantShared") {
        Write-Host "Reconfigure: libtorrent shared/static setting changed" -ForegroundColor Yellow
        return $true
    }

    $cacheTime = (Get-Item -LiteralPath $cache).LastWriteTimeUtc
    $inputs = @((Join-Path $ROOT "CMakeLists.txt"))
    Get-ChildItem -LiteralPath (Join-Path $ROOT "cmake") -Filter "*.cmake" -File -ErrorAction SilentlyContinue |
        ForEach-Object { $inputs += $_.FullName }

    foreach ($input in $inputs) {
        if ((Get-Item -LiteralPath $input).LastWriteTimeUtc -gt $cacheTime) {
            return $true
        }
    }
    return $false
}

function Enable-CompileCache {
    foreach ($name in @("sccache", "ccache")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) {
            $env:CMAKE_C_COMPILER_LAUNCHER = $cmd.Source
            $env:CMAKE_CXX_COMPILER_LAUNCHER = $cmd.Source
            return
        }
    }
}

function Ensure-DevBuild {
    $plan = Get-BuildArgs
    Ensure-BuildDir $plan.Qt
    Initialize-QtRuntime $plan.Qt
    Enable-CompileCache

    $appPath = Get-AppPath
    if (Test-Path -LiteralPath $appPath) {
        Deploy-QtRuntime $plan.Qt $appPath
    }

    if (Test-NeedsCmakeConfigure) {
        Write-Host "CMake configure ..."
        $configureArgs = $plan.Configure
        & $plan.Cmake @configureArgs
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

        # Static -> shared switch leaves a stale libtorrent build tree.
        $ltBuild = Join-Path $BUILD_DIR "_deps\libtorrent-build"
        if ((Test-Path -LiteralPath $ltBuild) -and $env:ARACHNEL_LIBTORRENT_SHARED -ne '0') {
            $ltDll = Get-ChildItem -Path $ltBuild -Recurse -Filter "libtorrent-rasterbar*.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
            if (-not $ltDll) {
                Write-Host "Cleaning stale libtorrent build (switching to shared DLL) ..." -ForegroundColor Yellow
                Remove-Item -LiteralPath $ltBuild -Recurse -Force -ErrorAction SilentlyContinue
                & $plan.Cmake @configureArgs
                if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
            }
        }
    }

    Write-Host "Build ..."
    & $plan.Cmake --build $BUILD_DIR --target arachnel_app -j $env:NUMBER_OF_PROCESSORS
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $appPath = Get-AppPath
    if (Test-Path -LiteralPath $appPath) {
        Deploy-QtRuntime $plan.Qt $appPath
    }
}

$remainingArgs = [System.Collections.Generic.List[string]]::new()
$runOnly = $false
$argsList = @($args)
$i = 0
while ($i -lt $argsList.Count) {
    switch ($argsList[$i]) {
        { $_ -in "-r", "--rebuild" } {
            if (Test-Path -LiteralPath $BUILD_DIR) {
                Remove-Item -LiteralPath $BUILD_DIR -Recurse -Force
            }
            $i++
            continue
        }
        { $_ -in "--run" } {
            $runOnly = $true
            $i++
            continue
        }
        { $_ -in "-h", "--help" } {
            @"
Arachnel dev launcher

  .\run.ps1              configure (if needed) + build + run
  .\run.ps1 --rebuild    clean build-win, then build + run
  .\run.ps1 --run        run without build (exe must exist)

Pass app args after options, e.g. .\run.ps1 -- --some-flag

Env: BUILD_TYPE (default RelWithDebInfo), CMAKE_PREFIX_PATH, ARACHNEL_FAST_BUILD=0, ARACHNEL_LIBTORRENT_SHARED=0

First time after this update: .\run.ps1 --rebuild  (rebuilds libtorrent as DLL)
"@
            exit 0
        }
        default { $remainingArgs.Add($argsList[$i]) | Out-Null; $i++ }
    }
}

if (-not $runOnly) {
    Ensure-DevBuild
}

$APP = Get-AppPath
if (-not (Test-Path -LiteralPath $APP)) {
    throw "arachnel_app.exe not found. Run without --run first."
}

$plan = Get-BuildArgs
Initialize-QtRuntime $plan.Qt
Deploy-QtRuntime $plan.Qt $APP

& $APP @remainingArgs
exit $LASTEXITCODE
