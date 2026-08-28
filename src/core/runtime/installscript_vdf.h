#pragma once

#include "runtime_dependency_types.h"

#include <QString>
#include <QVector>

namespace arachnel::core {

struct InstallScriptRegistryValue {
    QString rootKey;
    QString subKey;
    QString valueName;
    QString stringValue;
    quint32 dwordValue = 0;
    bool isDword = false;
};

/**
 * Parse Steam installscript.vdf "Run Process" blocks.
 * process/command paths still contain %INSTALLDIR% until resolveInstallDirPlaceholders.
 */
QVector<RedistInstallStep> parseInstallScriptRunProcess(const QString& vdfText);

/** Replace %INSTALLDIR% (any slash style) with the Steamworks Shared absolute path. */
void resolveInstallDirPlaceholders(QVector<RedistInstallStep>* steps, const QString& installDir);

/** True when Wine/Proton prefix registry files mention HasRunKey as installed. */
bool prefixHasRunKey(const QString& prefixDir, const QString& hasRunKey);

/** Parse Steam installscript.vdf "Registry" blocks into a flat list of keys/values. */
QVector<InstallScriptRegistryValue> parseInstallScriptRegistry(const QString& vdfText);

/** Replace placeholders like %INSTALLDIR%, %PROGRAMDATA%, %USERPROFILE% in registry values. */
void resolveRegistryPlaceholders(QVector<InstallScriptRegistryValue>* entries, const QString& installDir);

/** Apply registry entries to Windows Registry or Linux Proton prefix. */
bool applyRegistryEntries(const QVector<InstallScriptRegistryValue>& entries, const QString& protonPrefixDir = {});

/** Scan game directory for installscript*.vdf / runasadmin.vdf and apply all registry keys. */
bool applyInstallScriptForGame(const QString& gameDir, const QString& protonPrefixDir = {});

/** Check if game directory contains Ubisoft loader libraries (uplay_r1/r2_loader). */
bool isUbisoftGame(const QString& gameDir);

/** Patch EA Frostbite Activation64.dll license check bypass. */
bool healEaActivation(const QString& gameDir);

} // namespace arachnel::core


