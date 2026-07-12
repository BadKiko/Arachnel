#pragma once

#include "catalog_types.h"

#include <QByteArray>
#include <QVector>

namespace arachnel::core {

QVector<CatalogEntry> parseCatalogFeed(const QByteArray& payload, const QString& sourceId);
QString catalogFeedValidationError(const QByteArray& payload);
void deduplicateCatalogEntries(QVector<CatalogEntry>& entries);

} // namespace arachnel::core
