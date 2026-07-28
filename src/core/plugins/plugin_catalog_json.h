#pragma once

#include "catalog_types.h"

#include <QByteArray>
#include <QVector>

namespace arachnel::core {

/**
 * JSON catalog crossing the plugin DLL boundary (API v4).
 * Schema: {"schema":"arachnel.plugin.catalog.v1","entries":[...]}
 */
QByteArray serializePluginCatalogJson(const QVector<CatalogEntry>& entries);
QVector<CatalogEntry> parsePluginCatalogJson(const QByteArray& json,
                                             const QString& defaultSourceId);

} // namespace arachnel::core
