namespace {

QString platformLibraryName(const QString& base)
{
#if defined(Q_OS_WIN)
    return base + QStringLiteral(".dll");
#else
    return QStringLiteral("lib") + base + QStringLiteral(".so");
#endif
}

bool isZipArchive(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    char magic[4] = {};
    return file.read(magic, 4) == 4 && magic[0] == 'P' && magic[1] == 'K'
           && magic[2] == '\x03' && magic[3] == '\x04';
}

QString escapePowerShellSingleQuotedLiteral(const QString& value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return escaped;
}

QString g_lastPluginLoadError;

#if defined(Q_OS_WIN)
void prependWindowsPathDirectory(const QString& directory)
{
    if (directory.isEmpty())
        return;

    const QByteArray pathKey = "PATH";
    const QByteArray entry = QDir::toNativeSeparators(directory).toUtf8();
    const QByteArray current = qgetenv(pathKey);
    if (current.split(';').contains(entry))
        return;

    qputenv(pathKey, entry + ";" + current);
}

struct ScopedAddDllDirectory {
    using AddDllDirectoryFn = DLL_DIRECTORY_COOKIE(WINAPI*)(PCWSTR);
    using RemoveDllDirectoryFn = BOOL(WINAPI*)(DLL_DIRECTORY_COOKIE);

    explicit ScopedAddDllDirectory(const QStringList& directories)
    {
        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        if (!kernel)
            return;

        const auto addDllDirectory = reinterpret_cast<AddDllDirectoryFn>(
            GetProcAddress(kernel, "AddDllDirectory"));
        removeDllDirectory = reinterpret_cast<RemoveDllDirectoryFn>(
            GetProcAddress(kernel, "RemoveDllDirectory"));
        if (!addDllDirectory || !removeDllDirectory)
            return;

        for (const QString& directory : directories) {
            if (directory.isEmpty())
                continue;
            const DLL_DIRECTORY_COOKIE cookie =
                addDllDirectory(reinterpret_cast<LPCWSTR>(directory.utf16()));
            if (cookie)
                cookies.append(cookie);
        }
    }

    ~ScopedAddDllDirectory()
    {
        if (!removeDllDirectory)
            return;
        for (const DLL_DIRECTORY_COOKIE cookie : cookies)
            removeDllDirectory(cookie);
    }

    RemoveDllDirectoryFn removeDllDirectory = nullptr;
    QList<DLL_DIRECTORY_COOKIE> cookies;
};
#endif

} // namespace
