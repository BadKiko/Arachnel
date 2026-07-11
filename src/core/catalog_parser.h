#pragma once

#include "catalog_types.h"

#include <QByteArray>
#include <QVector>

namespace arachnel::core {

QVector<CatalogEntry> parseCatalogFeed(const QByteArray& payload, const QString& sourceId);

} // namespace arachnel::core
