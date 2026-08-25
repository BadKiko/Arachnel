#pragma once

#include "library_model.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

enum class VrRuntimeKind {
    None,
    OpenVR, // openvr_api.dll, SteamVR
    OpenXR, // openxr_loader.dll
    Oculus, // LibOVRRT / OVRPlugin
};

enum class VrMode {
    NotVr,      // No VR support
    VrOnly,     // Strictly a VR-only title (e.g. Half-Life: Alyx, Beat Saber, VTOL VR)
    VrOptional, // Hybrid title with both Desktop/2D and VR modes (e.g. The Forest, Payday 2, Subnautica)
};

struct VrGameDetection {
    bool hasVrLibraries = false;
    VrMode vrMode = VrMode::NotVr;
    bool isCurrentLaunchVr = false;
    VrRuntimeKind runtime = VrRuntimeKind::None;
    bool isUnity = false;
    bool isUnreal = false;
    QString detectedLibrary;
    QStringList suggestedArgs;
};

/**
 * Scans game folder, executable path, options, and genres to classify VR support
 * and determine whether the specific launch should activate VR mode.
 */
VrGameDetection detectVrGame(const QString& installPath,
                             const QString& executablePath = QString(),
                             const QString& genres = QString(),
                             const QString& gameTitle = QString(),
                             const QVector<GameLaunchOption>& options = {},
                             const QString& selectedOptionId = QString(),
                             const QStringList& currentArgs = {});

/** Returns human-readable name of VR runtime (e.g. "OpenVR (SteamVR)", "OpenXR", "Oculus VR"). */
QString vrRuntimeName(VrRuntimeKind kind);

/** Checks if SteamVR process (vrserver / vrmonitor / vrcompositor) is currently running. */
bool isSteamVrRunning();

/** Locates SteamVR install directory via openvrpaths.vrpath, Steam installation, etc. */
QString findSteamVrInstallPath();

/** Attempts to start SteamVR if not already running. */
bool tryStartSteamVr();

} // namespace arachnel::core
