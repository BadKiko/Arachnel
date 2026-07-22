#pragma once

#include <QString>

namespace arachnel::core {

/** Hidden stamp in an install folder: only Arachnel-managed games get this. */
inline constexpr auto kInstallMarkerFileName = ".arachnel";

QString installMarkerPath(const QString& installPath);
bool hasInstallMarker(const QString& installPath);
bool writeInstallMarker(const QString& installPath, const QString& gameId = {},
                        const QString& sourceId = {});

} // namespace arachnel::core
