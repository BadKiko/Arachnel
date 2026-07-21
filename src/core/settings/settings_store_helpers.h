namespace {

QString settingsFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/settings.json");
}

SourcePluginInfo sourceFromJson(const QJsonObject& obj)
{
    SourcePluginInfo info;
    info.id = obj.value(QStringLiteral("id")).toString();
    info.name = obj.value(QStringLiteral("name")).toString();
    info.description = obj.value(QStringLiteral("description")).toString();
    info.catalogUrl = obj.value(QStringLiteral("catalogUrl")).toString();
    info.iconName = obj.value(QStringLiteral("iconName")).toString(QStringLiteral("storefront"));
    info.enabled = obj.value(QStringLiteral("enabled")).toBool(true);

    const QJsonArray caps = obj.value(QStringLiteral("capabilities")).toArray();
    for (const QJsonValue& cap : caps)
        info.capabilities.append(cap.toString());
    if (info.capabilities.isEmpty()) {
        info.capabilities = {QStringLiteral("search"), QStringLiteral("install"),
                             QStringLiteral("update")};
    }
    return info;
}

QJsonObject sourceToJson(const SourcePluginInfo& info)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), info.id);
    obj.insert(QStringLiteral("name"), info.name);
    obj.insert(QStringLiteral("description"), info.description);
    obj.insert(QStringLiteral("catalogUrl"), info.catalogUrl);
    obj.insert(QStringLiteral("iconName"), info.iconName);
    obj.insert(QStringLiteral("enabled"), info.enabled);

    QJsonArray caps;
    for (const QString& cap : info.capabilities)
        caps.append(cap);
    obj.insert(QStringLiteral("capabilities"), caps);
    return obj;
}

} // namespace
