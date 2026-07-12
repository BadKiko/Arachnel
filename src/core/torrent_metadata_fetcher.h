#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace arachnel::core {

QString magnetInfoHashKey(const QString& magnetUri);

std::optional<QStringList> fetchMagnetFileNames(const QString& magnetUri, int timeoutMs = 12000);

} // namespace arachnel::core
