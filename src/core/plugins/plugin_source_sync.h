#pragma once

#include "catalog_types.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace arachnel::core {

class PluginHost;

struct PluginSourceSyncResult {
    enum class Status {
        Failed,    ///< nothing usable; caller should fall back to the plugin's catalog_json
        Unchanged, ///< same content as `knownKey`; entries left empty
        Loaded,    ///< entries are filled (parser output, before prepareEntry)
    };
    Status status = Status::Failed;
    QVector<CatalogEntry> entries;
    QByteArray payloadKey;
    QString error;
};

/**
 * Catalog-source extension (see plugin_api.h): the host downloads the plugin's raw catalog
 * (conditional GET, gzip, TTL, on-disk raw copy), scans its rows without building a document
 * tree and has the plugin normalize them in batches. Blocking; call from a worker thread.
 *
 * Never throws and never leaves the catalog cache half written: any failure returns
 * Status::Failed and the caller keeps using the plugin's catalog_json path.
 */
PluginSourceSyncResult syncPluginCatalogSource(const PluginHost& host, const QString& sourceId,
                                               const QByteArray& knownKey, bool force = false);

/**
 * Suffix ("|<plugin version>|x<ext>") that keys written by syncPluginCatalogSource end with;
 * empty when the plugin has no usable catalog source. A stored key that disagrees with it was
 * written by another plugin build (or by the catalog_json path) and must not be trusted.
 */
QByteArray pluginCatalogSourceKeySuffix(const PluginHost& host, const QString& sourceId);

/** True unless ARACHNEL_DISABLE_PLUGIN_SOURCE is set (diagnostics / escape hatch). */
bool pluginCatalogSourceEnabled();

} // namespace arachnel::core
