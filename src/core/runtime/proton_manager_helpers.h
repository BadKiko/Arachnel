namespace {

QString appDataDir()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

QString normalizePath(const QString& path)
{
    return QFileInfo(path).canonicalFilePath().isEmpty() ? QFileInfo(path).absoluteFilePath()
                                                           : QFileInfo(path).canonicalFilePath();
}

} // namespace
