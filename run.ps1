#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ROOT = $PSScriptRoot
$BUILD_DIR = Join-Path $ROOT "build-win"
$BUILD_TYPE = if ($env:BUILD_TYPE) { $env:BUILD_TYPE } else { "RelWithDebInfo" }
$DIST_DIR = Join-Path $ROOT "dist-win"
$env:QT_QML_MATERIAL_IMPORT_PATH = Join-Path $BUILD_DIR "qml_modules"
$env:QML2_IMPORT_PATH = Join-Path $BUILD_DIR "qml_modules"

function Initialize-DevPath {
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machinePath;$userPath"
}

function Add-QtInstallRoot {
    param(
        [System.Collections.Generic.List[string]]$Roots,
        [string]$Path
    )
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { return }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    # Kit path …/6.x.y/mingw_64 → install root
    if (Test-Path -LiteralPath (Join-Path $resolved "lib\cmake\Qt6\Qt6Config.cmake")) {
        $resolved = Split-Path -Parent (Split-Path -Parent $resolved)
    }
    if (-not $Roots.Contains($resolved)) {
        [void]$Roots.Add($resolved)
    }
}

function Get-QtInstallRoots {
    $roots = [System.Collections.Generic.List[string]]::new()
    Add-QtInstallRoot $roots $env:QT_INSTALL_DIR
    Add-QtInstallRoot $roots $env:QTDIR
    if ($env:CMAKE_PREFIX_PATH) {
        $p = ($env:CMAKE_PREFIX_PATH -split ';' | Select-Object -First 1)
        Add-QtInstallRoot $roots $p
    }

    foreach ($drive in @([IO.DriveInfo]::GetDrives() | Where-Object { $_.DriveType -eq 'Fixed' } | ForEach-Object { $_.Name.TrimEnd('\') })) {
        foreach ($name in @("Qt", "Qt6")) {
            $p = Join-Path $drive $name
            if ((Test-Path -LiteralPath $p) -and -not $roots.Contains($p)) {
                [void]$roots.Add($p)
            }
        }
    }

    foreach ($p in @(
            (Join-Path ${env:ProgramFiles} "Qt"),
            $(if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} "Qt" }),
            (Join-Path $env:LOCALAPPDATA "Qt"),
            (Join-Path $env:USERPROFILE "Qt")
        )) {
        if ($p -and (Test-Path -LiteralPath $p) -and -not $roots.Contains($p)) {
            [void]$roots.Add($p)
        }
    }
    return @($roots)
}

function Get-QtToolsRoot {
    param([string]$KitPrefix)
    if (-not $KitPrefix) { return $null }
    # <root>/<version>/<kit> → <root>/Tools
    $versionRoot = Split-Path -Parent $KitPrefix
    $installRoot = Split-Path -Parent $versionRoot
    foreach ($tools in @(
            (Join-Path $installRoot "Tools"),
            (Join-Path $versionRoot "Tools")
        )) {
        if (Test-Path -LiteralPath $tools) { return $tools }
    }
    foreach ($root in (Get-QtInstallRoots)) {
        $tools = Join-Path $root "Tools"
        if (Test-Path -LiteralPath $tools) { return $tools }
    }
    return $null
}

function Find-MingwBinDir {
    param([string]$KitPrefix)

    $tools = Get-QtToolsRoot $KitPrefix
    if ($tools) {
        $mingwDirs = Get-ChildItem -LiteralPath $tools -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^mingw' } |
            Sort-Object Name -Descending
        foreach ($dir in $mingwDirs) {
            $bin = Join-Path $dir.FullName "bin"
            if (Test-Path -LiteralPath (Join-Path $bin "g++.exe")) {
                return $bin
            }
        }
    }

    $fromPath = Get-Command gcc.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
    if ($fromPath) {
        return (Split-Path -Parent $fromPath)
    }
    return $null
}

function Find-NinjaDir {
    param([string]$KitPrefix)

    $tools = Get-QtToolsRoot $KitPrefix
    if ($tools) {
        foreach ($name in @("Ninja", "Ninja_64")) {
            $bin = Join-Path $tools $name
            if (Test-Path -LiteralPath (Join-Path $bin "ninja.exe")) { return $bin }
            if (Test-Path -LiteralPath (Join-Path $bin "bin\ninja.exe")) { return (Join-Path $bin "bin") }
        }
    }

    $fromPath = Get-Command ninja.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
    if ($fromPath) {
        return (Split-Path -Parent $fromPath)
    }
    return $null
}

function Resolve-Cmake {
    Initialize-DevPath
    $candidates = [System.Collections.Generic.List[string]]::new()
    $fromPath = Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
    if ($fromPath) { [void]$candidates.Add($fromPath) }

    foreach ($root in (Get-QtInstallRoots)) {
        $tools = Join-Path $root "Tools"
        if (-not (Test-Path -LiteralPath $tools)) { continue }
        Get-ChildItem -LiteralPath $tools -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^CMake' } |
            ForEach-Object {
                $exe = Join-Path $_.FullName "bin\cmake.exe"
                if (Test-Path -LiteralPath $exe) { [void]$candidates.Add($exe) }
            }
    }

    foreach ($pf in @(${env:ProgramFiles}, ${env:ProgramFiles(x86)})) {
        if (-not $pf) { continue }
        $exe = Join-Path $pf "CMake\bin\cmake.exe"
        if (Test-Path -LiteralPath $exe) { [void]$candidates.Add($exe) }
    }

    $unique = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique
    if ($unique) { return @($unique)[0] }
    throw "cmake not found. Install CMake, or set PATH / install Qt Online Installer Tools → CMake."
}

function Get-QtKitKind {
    param([string]$Prefix)
    if ($Prefix -match 'mingw') { return 'mingw_64' }
    if ($Prefix -match 'msvc') { return 'msvc2022_64' }
    return 'unknown'
}

function Find-QtKit {
    foreach ($raw in @($env:CMAKE_PREFIX_PATH, $env:QTDIR)) {
        if (-not $raw) { continue }
        $prefix = ($raw -split ';' | Select-Object -First 1)
        if (-not $prefix) { continue }
        $qt6Config = Join-Path $prefix "lib\cmake\Qt6\Qt6Config.cmake"
        if (Test-Path -LiteralPath $qt6Config) {
            return @{ Prefix = (Resolve-Path -LiteralPath $prefix).Path; Kind = (Get-QtKitKind $prefix) }
        }
    }

    $kitPreference = @("mingw_64", "msvc2022_64", "msvc2019_64")
    if ($env:ARACHNEL_QT_KIT) {
        $kitPreference = @($env:ARACHNEL_QT_KIT) + $kitPreference
    }

    foreach ($root in (Get-QtInstallRoots)) {
        $versions = Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match "^\d+\.\d+" } |
            Sort-Object {
                try { [version]$_.Name } catch { [version]"0.0" }
            } -Descending

        foreach ($versionDir in $versions) {
            foreach ($kitName in ($kitPreference | Select-Object -Unique)) {
                $kit = Join-Path $versionDir.FullName $kitName
                $qt6Config = Join-Path $kit "lib\cmake\Qt6\Qt6Config.cmake"
                if (Test-Path -LiteralPath $qt6Config) {
                    return @{ Prefix = $kit; Kind = (Get-QtKitKind $kit) }
                }
            }
        }
    }

    return $null
}

function Get-BuildArgs {
    $qt = Find-QtKit
    if (-not $qt) {
        throw @"
Qt kit not found.
Set CMAKE_PREFIX_PATH to a Qt6 kit (…/6.x.y/mingw_64 or …/msvc2022_64),
or QT_INSTALL_DIR to your Qt root (the folder that contains version dirs / Tools).
"@
    }

    $cmake = Resolve-Cmake
    $configureArgs = @(
        "-S", $ROOT,
        "-B", $BUILD_DIR,
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE",
        "-DCMAKE_PREFIX_PATH=$($qt.Prefix)",
        "-DARACHNEL_FAST_BUILD=$(if ($env:ARACHNEL_FAST_BUILD -eq '0') { 'OFF' } else { 'ON' })",
        "-DARACHNEL_LIBTORRENT_SHARED=$(if ($env:ARACHNEL_LIBTORRENT_SHARED -eq '0') { 'OFF' } else { 'ON' })",
        "-DARACHNEL_VERSION=$(if ($env:ARACHNEL_VERSION) { $env:ARACHNEL_VERSION } else { 'dev' })"
    )

    if ($env:CMAKE_C_COMPILER_LAUNCHER) {
        $configureArgs += "-DCMAKE_C_COMPILER_LAUNCHER=$($env:CMAKE_C_COMPILER_LAUNCHER)"
    }
    if ($env:CMAKE_CXX_COMPILER_LAUNCHER) {
        $configureArgs += "-DCMAKE_CXX_COMPILER_LAUNCHER=$($env:CMAKE_CXX_COMPILER_LAUNCHER)"
    }
    if ($env:FETCHCONTENT_BASE_DIR) {
        New-Item -ItemType Directory -Force -Path $env:FETCHCONTENT_BASE_DIR | Out-Null
        $fetchDir = ($env:FETCHCONTENT_BASE_DIR -replace '\\', '/')
        $configureArgs += "-DFETCHCONTENT_BASE_DIR=$fetchDir"
    }

    if ($qt.Kind -eq "mingw_64") {
        $mingwBin = Find-MingwBinDir $qt.Prefix
        if (-not $mingwBin) {
            throw "MinGW not found next to Qt (Tools/mingw*_64) and gcc.exe is not on PATH."
        }
        $gcc = (Join-Path $mingwBin "gcc.exe") -replace '\\', '/'
        $gxx = (Join-Path $mingwBin "g++.exe") -replace '\\', '/'
        $pathParts = @($mingwBin)
        $ninjaDir = Find-NinjaDir $qt.Prefix
        if ($ninjaDir) { $pathParts += $ninjaDir }
        $env:Path = ($pathParts -join ";") + ";" + $env:Path
        return @{
            Qt = $qt
            Cmake = $cmake
            Configure = $configureArgs + @("-G", "Ninja", "-DCMAKE_C_COMPILER=$gcc", "-DCMAKE_CXX_COMPILER=$gxx")
        }
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if ($env:GITHUB_ACTIONS -eq 'true') {
        $ninja = Get-Command ninja.exe -ErrorAction SilentlyContinue
        $cl = $null
        if ($env:VCToolsInstallDir) {
            $clCandidate = Join-Path $env:VCToolsInstallDir "bin\Hostx64\x64\cl.exe"
            if (Test-Path -LiteralPath $clCandidate) { $cl = $clCandidate }
        }
        if (-not $cl) {
            $cl = Get-Command cl.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
        }
        if ($ninja -and $cl) {
            $clForCmake = ($cl -replace '\\', '/')
            $configureExtras = @(
                "-G", "Ninja",
                "-DCMAKE_C_COMPILER=$clForCmake",
                "-DCMAKE_CXX_COMPILER=$clForCmake"
            )
            if ($env:WindowsSdkDir -and $env:WindowsSDKVersion) {
                $sdkVer = $env:WindowsSDKVersion.TrimEnd('\')
                $sdkBin = Join-Path $env:WindowsSdkDir "bin\$sdkVer\x64"
                foreach ($pair in @(
                        @{ Flag = "CMAKE_RC_COMPILER"; Name = "rc.exe" },
                        @{ Flag = "CMAKE_MT"; Name = "mt.exe" }
                    )) {
                    $tool = Join-Path $sdkBin $pair.Name
                    if (Test-Path -LiteralPath $tool) {
                        $toolForCmake = ($tool -replace '\\', '/')
                        $configureExtras += "-D$($pair.Flag)=$toolForCmake"
                    }
                }
            }
            if ($env:VCToolsInstallDir) {
                $linkCandidate = Join-Path $env:VCToolsInstallDir "bin\Hostx64\x64\link.exe"
                if (Test-Path -LiteralPath $linkCandidate) {
                    $linkForCmake = ($linkCandidate -replace '\\', '/')
                    $configureExtras += "-DCMAKE_LINKER=$linkForCmake"
                }
            }
            return @{
                Qt = $qt
                Cmake = $cmake
                Configure = $configureArgs + $configureExtras
            }
        }
    }
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

function Get-SetupPath {
    $candidates = @(
        (Join-Path $BUILD_DIR "arachnel_setup.exe"),
        (Join-Path $BUILD_DIR "$BUILD_TYPE\arachnel_setup.exe"),
        (Join-Path $BUILD_DIR "Release\arachnel_setup.exe")
    )
    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    return (Join-Path $BUILD_DIR $candidates[0])
}

function New-InstallerPackage {
    if (-not (Test-Path -LiteralPath (Join-Path $DIST_DIR "arachnel_app.exe"))) {
        Write-Host "dist-win missing — building release package first ..."
        New-ReleasePackage
    }

    Enable-CompileCache
    $plan = Get-BuildArgs
    Ensure-BuildDir $plan.Qt
    Initialize-QtRuntime $plan.Qt

    if (Test-NeedsCmakeConfigure) {
        Write-Host "CMake configure ..."
        & $plan.Cmake @($plan.Configure)
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    Write-Host "Build arachnel_setup + launcher + uninstall ..."
    & $plan.Cmake --build $BUILD_DIR --target arachnel_setup arachnel_setup_launcher arachnel_uninstall -j $env:NUMBER_OF_PROCESSORS
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $setupPath = Get-SetupPath
    if (-not (Test-Path -LiteralPath $setupPath)) {
        throw "arachnel_setup.exe not found after build."
    }

    $launcherPath = Join-Path $BUILD_DIR "arachnel_setup_launcher.exe"
    if (-not (Test-Path -LiteralPath $launcherPath)) {
        throw "arachnel_setup_launcher.exe not found after build."
    }

    $uninstallPath = Join-Path $BUILD_DIR "uninstall.exe"
    if (-not (Test-Path -LiteralPath $uninstallPath)) {
        throw "uninstall.exe not found after build."
    }

    $packScript = Join-Path $ROOT "setup\pack.ps1"
    & $packScript -BuildDir $BUILD_DIR -DistDir $DIST_DIR -QtPrefix $plan.Qt.Prefix
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
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
    $expected = if ($Qt.Kind -eq "mingw_64") {
        "Ninja"
    } elseif ($env:GITHUB_ACTIONS -eq 'true') {
        "Ninja"
    } else {
        "Visual Studio 17 2022"
    }

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

function Find-VulkanBinDir {
    if ($env:VULKAN_SDK) {
        $bin = Join-Path $env:VULKAN_SDK "Bin"
        if (Test-Path -LiteralPath $bin) { return $bin }
    }
    foreach ($root in @(
            "C:\VulkanSDK",
            (Join-Path ${env:ProgramFiles} "VulkanSDK"),
            (Join-Path $env:LOCALAPPDATA "VulkanSDK")
        )) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $latest = Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            Select-Object -First 1
        if (-not $latest) { continue }
        $bin = Join-Path $latest.FullName "Bin"
        if (Test-Path -LiteralPath $bin) { return $bin }
    }
    return $null
}

function Initialize-QtRuntime {
    param([hashtable]$Qt)

    if (-not $Qt) { return }

    $runtimePaths = [System.Collections.Generic.List[string]]::new()
    $qtBin = Join-Path $Qt.Prefix "bin"
    if (Test-Path -LiteralPath $qtBin) { [void]$runtimePaths.Add($qtBin) }

    if ($Qt.Kind -eq "mingw_64") {
        $mingwBin = Find-MingwBinDir $Qt.Prefix
        if ($mingwBin) { [void]$runtimePaths.Add($mingwBin) }
        $ninjaDir = Find-NinjaDir $Qt.Prefix
        if ($ninjaDir) { [void]$runtimePaths.Add($ninjaDir) }
    }

    $vulkanBin = Find-VulkanBinDir
    if ($vulkanBin) {
        $runtimePaths.Insert(0, $vulkanBin)
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
        (Join-Path $AppDir "Qt6Multimedia.dll"),
        (Join-Path $AppDir "platforms\qwindows.dll"),
        (Join-Path $AppDir "qml\QtMultimedia\qmldir")
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

    Resolve-WindowsTlsRuntime -DestDir $appDir -QtPrefix $Qt.Prefix
}

function New-ReleasePackage {
    Enable-CompileCache
    $plan = Get-BuildArgs
    Ensure-BuildDir $plan.Qt
    Initialize-QtRuntime $plan.Qt

    if (Test-NeedsCmakeConfigure) {
        Write-Host "CMake configure ..."
        $configureArgs = $plan.Configure
        & $plan.Cmake @configureArgs
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

        if (Test-LibtorrentNeedsSharedMigration $BUILD_DIR) {
            $ltBuild = Join-Path $BUILD_DIR "_deps\libtorrent-build"
            Write-Host "Cleaning stale static libtorrent build (one-time shared DLL migration) ..." -ForegroundColor Yellow
            Remove-Item -LiteralPath $ltBuild -Recurse -Force -ErrorAction SilentlyContinue
            & $plan.Cmake @configureArgs
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
    }

    Write-Host "Build ..."
    & $plan.Cmake --build $BUILD_DIR --target arachnel_app -j $env:NUMBER_OF_PROCESSORS
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $appPath = Get-AppPath
    if (-not (Test-Path -LiteralPath $appPath)) {
        throw "arachnel_app.exe not found after build."
    }

    if (Test-Path -LiteralPath $DIST_DIR) {
        Remove-Item -LiteralPath $DIST_DIR -Recurse -Force
    }
    New-Item -ItemType Directory -Path $DIST_DIR | Out-Null

    $distExe = Join-Path $DIST_DIR "arachnel_app.exe"
    Copy-Item -LiteralPath $appPath -Destination $distExe -Force

    @"
[Paths]
Prefix=.
Plugins=.
Qml2Imports=qml
Imports=qml
"@ | Set-Content -LiteralPath (Join-Path $DIST_DIR "qt.conf") -Encoding ASCII

    $qmlModulesBuilt = Join-Path $BUILD_DIR "qml_modules"
    if (Test-Path -LiteralPath $qmlModulesBuilt) {
        Copy-Item -LiteralPath $qmlModulesBuilt -Destination (Join-Path $DIST_DIR "qml_modules") -Recurse -Force
    }

    $windeployqt = Join-Path $plan.Qt.Prefix "bin\windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt)) {
        throw "windeployqt not found at $windeployqt"
    }

    Write-Host "Deploying Qt runtime to dist-win ..."
    $qmlDir = Join-Path $ROOT "qml"
    $qmlModules = Join-Path $BUILD_DIR "qml_modules"
    & $windeployqt --qmldir $qmlDir --qmldir $qmlModules --no-translations $distExe
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE"
    }

    $qtNetworkAccess = Join-Path $plan.Qt.Prefix "plugins\\networkaccess"
    if (Test-Path -LiteralPath $qtNetworkAccess) {
        Copy-Item -LiteralPath $qtNetworkAccess -Destination (Join-Path $DIST_DIR "networkaccess") -Recurse -Force
    }

    Resolve-WindowsTlsRuntime -DestDir $DIST_DIR -QtPrefix $plan.Qt.Prefix

    $libtorrentDll = Resolve-LibtorrentSharedDll $BUILD_DIR
    if ($libtorrentDll) {
        $dllName = Split-Path -Leaf $libtorrentDll
        Copy-Item -LiteralPath $libtorrentDll -Destination (Join-Path $DIST_DIR $dllName) -Force
        Write-Host "Bundled libtorrent: $dllName"
    } elseif ($env:ARACHNEL_LIBTORRENT_SHARED -ne '0') {
        throw "libtorrent shared DLL not found (expected torrent-rasterbar.dll or libtorrent-rasterbar.dll). Rebuild with --rebuild."
    }

    $qmlMaterialDll = Join-Path $BUILD_DIR "qml_material.dll"
    if (Test-Path -LiteralPath $qmlMaterialDll) {
        Copy-Item -LiteralPath $qmlMaterialDll -Destination (Join-Path $DIST_DIR "qml_material.dll") -Force
    }

    $zipName = "Arachnel-win64-$BUILD_TYPE.zip"
    $zipPath = Join-Path $ROOT $zipName
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Write-Host "Packaging $zipName ..."
    Compress-Archive -Path (Join-Path $DIST_DIR "*") -DestinationPath $zipPath -Force

    Write-Host ""
    Write-Host "Done: $zipPath" -ForegroundColor Green
}

function Resolve-WindowsTlsRuntime {
    param(
        [string]$DestDir,
        [string]$QtPrefix
    )

    # windeployqt copies tls/qopensslbackend.dll, but not OpenSSL itself. Loading that
    # plugin without libssl/libcrypto aborts the process with a Windows system error.
    $opensslDlls = @('libssl-3-x64.dll', 'libcrypto-3-x64.dll')
    $searchRoots = New-Object System.Collections.Generic.List[string]

    if ($env:IQTA_TOOLS) { [void]$searchRoots.Add($env:IQTA_TOOLS) }
    if ($QtPrefix) {
        [void]$searchRoots.Add((Join-Path $QtPrefix "bin"))
        $qtVersionRoot = Split-Path -Parent $QtPrefix
        $qtInstallRoot = Split-Path -Parent $qtVersionRoot
        [void]$searchRoots.Add((Join-Path $qtInstallRoot "Tools"))
        [void]$searchRoots.Add((Join-Path $qtVersionRoot "Tools"))
    }
    foreach ($pathEntry in ($env:Path -split ';')) {
        if (-not $pathEntry) { continue }
        if ($pathEntry -match '(?i)openssl') { [void]$searchRoots.Add($pathEntry) }
    }

    $missing = @()
    foreach ($dllName in $opensslDlls) {
        $dest = Join-Path $DestDir $dllName
        if (Test-Path -LiteralPath $dest) { continue }

        $found = $null
        foreach ($root in ($searchRoots | Select-Object -Unique)) {
            if (-not (Test-Path -LiteralPath $root)) { continue }
            $match = Get-ChildItem -LiteralPath $root -Filter $dllName -Recurse -File -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($match) {
                $found = $match.FullName
                break
            }
        }

        if ($found) {
            Copy-Item -LiteralPath $found -Destination $dest -Force
            Write-Host "Bundled OpenSSL: $dllName"
        } else {
            $missing += $dllName
        }
    }

    if ($missing.Count -eq 0) { return }

    $opensslBackend = Join-Path $DestDir "tls\qopensslbackend.dll"
    if (Test-Path -LiteralPath $opensslBackend) {
        Remove-Item -LiteralPath $opensslBackend -Force
        Write-Host "Removed tls\qopensslbackend.dll (OpenSSL DLLs missing: $($missing -join ', '); using Schannel)" -ForegroundColor Yellow
    }
}

function Resolve-LibtorrentSharedDll {
    param([string]$BuildDir)

    $searchDirs = @(
        (Split-Path -Parent (Get-AppPath)),
        $BuildDir,
        (Join-Path $BuildDir "_deps\libtorrent-build"),
        (Join-Path $BuildDir "thirdparty\libtorrent")
    ) | Select-Object -Unique

    foreach ($dir in $searchDirs) {
        if (-not (Test-Path -LiteralPath $dir)) { continue }
        $match = Get-ChildItem -LiteralPath $dir -Filter "*torrent-rasterbar*.dll" -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($match) { return $match.FullName }

        $match = Get-ChildItem -Path $dir -Recurse -Filter "*torrent-rasterbar*.dll" -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($match) { return $match.FullName }
    }

    return $null
}

function Test-LibtorrentSharedDllPresent {
    param([string]$BuildDir)

    return [bool](Resolve-LibtorrentSharedDll $BuildDir)
}

function Test-LibtorrentNeedsSharedMigration {
    param([string]$BuildDir)

    if ($env:ARACHNEL_LIBTORRENT_SHARED -eq '0') { return $false }
    if (Test-LibtorrentSharedDllPresent $BuildDir) { return $false }

    $ltBuild = Join-Path $BuildDir "_deps\libtorrent-build"
    if (-not (Test-Path -LiteralPath $ltBuild)) { return $false }

    # Only wipe when an old static libtorrent tree is still on disk.
    $staticLib = Get-ChildItem -Path $ltBuild -Recurse -Filter "libtorrent-rasterbar.a" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    return [bool]$staticLib
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
    # Honor launchers already set by CI (sccache-action / env).
    if ($env:CMAKE_CXX_COMPILER_LAUNCHER -and $env:CMAKE_C_COMPILER_LAUNCHER) {
        return
    }
    foreach ($name in @("sccache", "ccache")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) {
            $env:CMAKE_C_COMPILER_LAUNCHER = $cmd.Source
            $env:CMAKE_CXX_COMPILER_LAUNCHER = $cmd.Source
            Write-Host "Compile cache: $name ($($cmd.Source))"
            return
        }
    }
}

function Ensure-DevBuild {
    Enable-CompileCache
    $plan = Get-BuildArgs
    Ensure-BuildDir $plan.Qt
    Initialize-QtRuntime $plan.Qt

    $appPath = Get-AppPath
    if (Test-Path -LiteralPath $appPath) {
        Deploy-QtRuntime $plan.Qt $appPath
    }

    if (Test-NeedsCmakeConfigure) {
        Write-Host "CMake configure ..."
        $configureArgs = $plan.Configure
        & $plan.Cmake @configureArgs
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

        if (Test-LibtorrentNeedsSharedMigration $BUILD_DIR) {
            $ltBuild = Join-Path $BUILD_DIR "_deps\libtorrent-build"
            Write-Host "Cleaning stale static libtorrent build (one-time shared DLL migration) ..." -ForegroundColor Yellow
            Remove-Item -LiteralPath $ltBuild -Recurse -Force -ErrorAction SilentlyContinue
            & $plan.Cmake @configureArgs
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
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

function Get-ArachnelDataDir {
    Join-Path $env:APPDATA "Arachnel"
}

function Format-ExitCode {
    param([int]$Code)
    if ($Code -eq 0) { return "0" }
    if ($Code -lt 0) {
        $unsigned = [uint32]$Code
        return "0x{0:X8} ({1})" -f $unsigned, $Code
    }
    return "$Code"
}

function Get-LogTail {
    param(
        [string]$Path,
        [int]$Lines = 40
    )
    if (-not (Test-Path -LiteralPath $Path)) { return @() }
    return @(Get-Content -LiteralPath $Path -Tail $Lines -ErrorAction SilentlyContinue)
}

function Show-CrashReport {
    param(
        [int]$ExitCode,
        [string]$AppPath
    )

    $dataDir = Get-ArachnelDataDir
    $crashLog = Join-Path $dataDir "crash.log"
    $runLog = Join-Path $dataDir "run.log"
    $issueUrl = "https://github.com/BadKiko/Arachnel/issues/new"
    $exitLabel = Format-ExitCode $ExitCode

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host " Arachnel stopped unexpectedly" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "Exit code: $exitLabel"
    Write-Host "Exe:       $AppPath"
    Write-Host "Logs:      $dataDir"
    Write-Host ""

    $crashTail = Get-LogTail $crashLog 20
    if ($crashTail.Count -gt 0) {
        Write-Host "--- crash.log (last lines) ---" -ForegroundColor Yellow
        $crashTail | ForEach-Object { Write-Host $_ }
        Write-Host ""
    }

    $runTail = Get-LogTail $runLog 30
    if ($runTail.Count -gt 0) {
        Write-Host "--- run.log (last lines) ---" -ForegroundColor Yellow
        $runTail | ForEach-Object { Write-Host $_ }
        Write-Host ""
    }

    Write-Host "Please report this on GitHub (attach the log tail above):" -ForegroundColor Cyan
    Write-Host $issueUrl -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Press Enter to close this window ..."
    [void][System.Console]::ReadLine()
}

function Invoke-ArachnelApp {
    param(
        [string]$AppPath,
        [string[]]$AppArgs
    )

    $env:ARACHNEL_DEV_RUN = "1"
    Write-Host "Run $AppPath"
    Write-Host ""

    & $AppPath @AppArgs
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
        Show-CrashReport -ExitCode $exitCode -AppPath $AppPath
    }

    exit $exitCode
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
        { $_ -in "--release" } {
            $script:BUILD_TYPE = "Release"
            $i++
            continue
        }
        { $_ -in "--package" } {
            if ($script:BUILD_TYPE -eq "RelWithDebInfo" -and -not $env:BUILD_TYPE) {
                $script:BUILD_TYPE = "Release"
            }
            New-ReleasePackage
            exit 0
        }
        { $_ -in "--installer" } {
            if ($script:BUILD_TYPE -eq "RelWithDebInfo" -and -not $env:BUILD_TYPE) {
                $script:BUILD_TYPE = "Release"
            }
            New-InstallerPackage
            exit 0
        }
        { $_ -in "-h", "--help" } {
            @"
Arachnel dev launcher

  .\run.ps1              configure (if needed) + build + run
  .\run.ps1 --rebuild    clean build-win, then build + run
  .\run.ps1 --run        run without build (exe must exist)
  .\run.ps1 --package    build Release + create dist-win ZIP (Arachnel-win64-Release.zip)
  .\run.ps1 --installer  build single Arachnel-Setup.exe (embedded app + runtime payload)
  .\run.ps1 --release    use Release build type (still runs app unless combined with --package)
  BUILD_TYPE=RelWithDebInfo .\run.ps1 --package   debug symbols in the package (larger ZIP)

Pass app args after options, e.g. .\run.ps1 -- --some-flag

Env:
  BUILD_TYPE (default RelWithDebInfo)
  CMAKE_PREFIX_PATH / QTDIR     Qt6 kit path (…/6.x.y/mingw_64 or msvc2022_64)
  QT_INSTALL_DIR                Qt root (contains version dirs + Tools)
  ARACHNEL_QT_KIT               prefer kit name, e.g. mingw_64 or msvc2022_64
  ARACHNEL_FAST_BUILD=0, ARACHNEL_LIBTORRENT_SHARED=0

Shared libtorrent migration (static -> DLL) is automatic once; use --rebuild for a full clean.
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

Invoke-ArachnelApp -AppPath $APP -AppArgs $remainingArgs
