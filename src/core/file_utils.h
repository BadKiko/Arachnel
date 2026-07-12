#pragma once

#include <QString>

namespace arachnel::core {

bool removePathRecursive(const QString& path, QString* errorOut = nullptr);
bool copyPathRecursive(const QString& src, const QString& dst, QString* errorOut = nullptr);
bool movePathRecursive(const QString& src, const QString& dst, QString* errorOut = nullptr);
QString relocatePathPrefix(const QString& path, const QString& oldRoot, const QString& newRoot);

} // namespace arachnel::core
