#pragma once

#include <QJsonObject>
#include <QString>

namespace arachnel::core {

/** Read public source URL from plugins.json / plugin.json (no hardcoded repos). */
inline QString repositoryFromJsonObject(const QJsonObject& obj)
{
    for (const char* key : {"repository", "homepage", "sourceUrl", "git", "source"}) {
        const QString value = obj.value(QLatin1String(key)).toString().trimmed();
        if (!value.isEmpty())
            return value;
    }
    return {};
}

inline QString resolvePluginRepository(const QString& /*pluginId*/, const QJsonObject& obj = {})
{
    return repositoryFromJsonObject(obj);
}

} // namespace arachnel::core
