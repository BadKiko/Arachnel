#pragma once

#include <QString>
#include <QStringList>

namespace arachnel::core {

/** Compare Arachnel app versions like 0.1.30b / v0.1.34a / 0.1.33. */
int compareAppVersions(const QString& left, const QString& right);

/** True when version is within [minVersion, maxVersion]. Empty max = no upper bound. */
bool appVersionInRange(const QString& version, const QString& minVersion,
                       const QString& maxVersion);

/** Prefer the highest plugin version; empty strings sort lowest. */
int comparePluginVersions(const QString& left, const QString& right);

} // namespace arachnel::core
