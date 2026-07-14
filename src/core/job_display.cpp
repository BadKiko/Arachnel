#include "job_display.h"

#include <QCoreApplication>

namespace arachnel::core {

namespace {

QString trExact(const char* english)
{
    return QCoreApplication::translate("Core", english);
}

QString fromPrefixed(const QString& value, const QString& legacyPrefix, const char* englishPrefix)
{
    if (value.startsWith(legacyPrefix))
        return QCoreApplication::translate("Core", englishPrefix).arg(value.mid(legacyPrefix.size()));
    return {};
}

} // namespace

QString displayJobTitle(const QString& title)
{
    if (title.isEmpty())
        return title;

    if (QString translated = fromPrefixed(title, QStringLiteral("Загрузка "), "Downloading %1");
        !translated.isEmpty())
        return translated;
    if (QString translated = fromPrefixed(title, QStringLiteral("Downloading "), "Downloading %1");
        !translated.isEmpty())
        return translated;

    if (QString translated = fromPrefixed(title, QStringLiteral("Обновление "), "Updating %1");
        !translated.isEmpty())
        return translated;
    if (QString translated = fromPrefixed(title, QStringLiteral("Updating "), "Updating %1");
        !translated.isEmpty())
        return translated;

    if (title.startsWith(QStringLiteral("Дополнение "))) {
        const int dash = title.indexOf(QStringLiteral(" — "));
        if (dash > 11)
            return QCoreApplication::translate("Core", "Add-on %1 — %2").arg(title.mid(11, dash - 11), title.mid(dash + 3));
    }
    if (title.startsWith(QStringLiteral("Add-on "))) {
        const int dash = title.indexOf(QStringLiteral(" — "));
        if (dash > 7)
            return QCoreApplication::translate("Core", "Add-on %1 — %2").arg(title.mid(7, dash - 7), title.mid(dash + 3));
    }

    return title;
}

QString displayJobDetail(const QString& detail)
{
    if (detail.isEmpty())
        return detail;

    static const struct {
        const char* legacyRu;
        const char* english;
    } kExact[] = {
        {"Установлено", "Installed"},
        {"Installed", "Installed"},
        {"Загрузка завершена", "Download complete"},
        {"Download complete", "Download complete"},
        {"Требуется установка", "Installation required"},
        {"Installation required", "Installation required"},
        {"Загрузка…", "Downloading…"},
        {"Downloading…", "Downloading…"},
        {"Подключение…", "Connecting…"},
        {"Connecting…", "Connecting…"},
        {"Установка…", "Installing…"},
        {"Installing…", "Installing…"},
        {"Установка дополнения…", "Installing add-on…"},
        {"Installing add-on…", "Installing add-on…"},
        {"Возобновление…", "Resuming…"},
        {"Resuming…", "Resuming…"},
        {"Пауза", "Paused"},
        {"Paused", "Paused"},
        {"Отменено", "Cancelled"},
        {"Cancelled", "Cancelled"},
        {"Ошибка установки", "Install failed"},
        {"Install failed", "Install failed"},
        {"Не удалось начать торрент", "Failed to start torrent"},
        {"Failed to start torrent", "Failed to start torrent"},
        {"Не удалось начать HTTP-загрузку", "Failed to start HTTP download"},
        {"Failed to start HTTP download", "Failed to start HTTP download"},
    };

    for (const auto& entry : kExact) {
        if (detail == QLatin1String(entry.legacyRu) || detail == QLatin1String(entry.english))
            return trExact(entry.english);
    }

    if (detail.startsWith(QStringLiteral("Ошибка установки: ")))
        return QCoreApplication::translate("Core", "Install failed: %1").arg(detail.mid(18));
    if (detail.startsWith(QStringLiteral("Install failed: ")))
        return QCoreApplication::translate("Core", "Install failed: %1").arg(detail.mid(16));

    if (detail.startsWith(QStringLiteral("Установка (")))
        return QCoreApplication::translate("Core", "Installing (%1/%2)")
            .arg(detail.section(QLatin1Char('('), 1, 1).section(QLatin1Char('/'), 0, 0),
                 detail.section(QLatin1Char('/'), 1, 1).chopped(1));
    if (detail.startsWith(QStringLiteral("Installing (")))
        return QCoreApplication::translate("Core", "Installing (%1/%2)")
            .arg(detail.section(QLatin1Char('('), 1, 1).section(QLatin1Char('/'), 0, 0),
                 detail.section(QLatin1Char('/'), 1, 1).chopped(1));

    if (detail.startsWith(QStringLiteral("Ошибка:")))
        return QCoreApplication::translate("Core", "Error: %1").arg(detail.mid(7));
    if (detail.startsWith(QStringLiteral("Error:")))
        return QCoreApplication::translate("Core", "Error: %1").arg(detail.mid(6));

    return detail;
}

} // namespace arachnel::core
