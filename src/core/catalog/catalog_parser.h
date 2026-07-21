#pragma once

#include "catalog_types.h"

#include <QByteArray>
#include <QVector>

namespace arachnel::core {

QVector<CatalogEntry> parseCatalogFeed(const QByteArray& payload, const QString& sourceId);
QString catalogFeedValidationError(const QByteArray& payload);
void deduplicateCatalogEntries(QVector<CatalogEntry>& entries);
QString catalogMagnetInfoHash(const QString& magnetUri);
QString catalogMagnetInfoHash(const QStringList& magnetUris);

} // namespace arachnel::core
