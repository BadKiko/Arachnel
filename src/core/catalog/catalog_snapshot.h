#pragma once

#include "catalog_types.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace arachnel::core {

/**
 * Binary snapshot of a source's parsed catalog entries. Relaunching with an unchanged feed
 * reads this instead of re-parsing ~70 MB of JSON (the dominant cost of a warm start).
 *
 * A snapshot is only used when it was written for the same payload (`payloadKey`), by the same
 * build (struct size + executable identity) and is complete. Anything else is ignored and the
 * JSON path runs as before, so a stale or damaged file can never produce wrong entries.
 *
 * Entries are stored exactly as the parser produced them, before any per-entry preparation
 * (metadata overlay, derived fields), so those still reflect the current caches.
 */
namespace CatalogSnapshot {

bool save(const QString& sourceId, const QByteArray& payloadKey,
          const QVector<CatalogEntry>& entries);
bool load(const QString& sourceId, const QByteArray& payloadKey, QVector<CatalogEntry>* out);
void remove(const QString& sourceId);

} // namespace CatalogSnapshot

} // namespace arachnel::core
