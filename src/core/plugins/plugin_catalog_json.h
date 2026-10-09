#pragma once

#include "catalog_types.h"

#include <QByteArray>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

/**
 * JSON catalog crossing the plugin DLL boundary (API v4).
 * Schema: {"schema":"arachnel.plugin.catalog.v1","entries":[...]}
 */
QByteArray serializePluginCatalogJson(const QVector<CatalogEntry>& entries);
QVector<CatalogEntry> parsePluginCatalogJson(const QByteArray& json,
                                             const QString& defaultSourceId);

/**
 * Same as parsePluginCatalogJson; also reads the optional top-level "supersedes" id list
 * (catalog source extension, see plugin_api.h) into `supersedes` when given.
 */
QVector<CatalogEntry> parsePluginCatalogJsonEx(const QByteArray& json,
                                               const QString& defaultSourceId,
                                               QStringList* supersedes);

} // namespace arachnel::core
