#pragma once

#include "catalog_types.h"

#include <QByteArray>
#include <QVector>

namespace arachnel::core {

QVector<CatalogEntry> parseCatalogFeed(const QByteArray& payload, const QString& sourceId);
/** Structural check only - does not build CatalogEntry objects. */
QString catalogFeedValidationError(const QByteArray& payload);
/** Count games without constructing CatalogEntry (for source count prefetch). */
int catalogFeedQuickCount(const QByteArray& payload);
void deduplicateCatalogEntries(QVector<CatalogEntry>& entries);
QString catalogMagnetInfoHash(const QString& magnetUri);
QString catalogMagnetInfoHash(const QStringList& magnetUris);

} // namespace arachnel::core
