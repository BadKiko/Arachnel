#include "job_display.h"

#include <QCoreApplication>

namespace arachnel::core {

namespace {

// Extracted by lupdate; used at runtime via translate()/trExact().
void jobDisplayTranslationSeed()
{
    QT_TRANSLATE_NOOP("Core", "Installed");
    QT_TRANSLATE_NOOP("Core", "Download complete");
    QT_TRANSLATE_NOOP("Core", "Installation required");
    QT_TRANSLATE_NOOP("Core", "Downloading…");
    QT_TRANSLATE_NOOP("Core", "Connecting…");
    QT_TRANSLATE_NOOP("Core", "Installing…");
    QT_TRANSLATE_NOOP("Core", "Installing add-on…");
    QT_TRANSLATE_NOOP("Core", "Preparing…");
    QT_TRANSLATE_NOOP("Core", "Preparing Steam…");
    QT_TRANSLATE_NOOP("Core", "Downloading from Steam CDN…");
    QT_TRANSLATE_NOOP("Core", "Getting game info…");
    QT_TRANSLATE_NOOP("Core", "Finishing…");
    QT_TRANSLATE_NOOP("Core", "This game is not available for download right now. Try another title.");
    QT_TRANSLATE_NOOP("Core",
                      "Could not prepare this game for download. Try again later or pick another title.");
    QT_TRANSLATE_NOOP("Core", "Download failed. Try again or pick another game.");
    QT_TRANSLATE_NOOP(
        "Core",
        "Steam blocked downloading game files (need a packaged manifest). "
        "Try another title, or set hubcapApiKey in plugin settings.");
    QT_TRANSLATE_NOOP("Core", "Resuming…");
    QT_TRANSLATE_NOOP("Core", "Paused");
    QT_TRANSLATE_NOOP("Core", "Cancelled");
    QT_TRANSLATE_NOOP("Core", "Install failed");
    QT_TRANSLATE_NOOP("Core", "Failed to start torrent");
    QT_TRANSLATE_NOOP("Core", "Failed to start HTTP download");
    QT_TRANSLATE_NOOP("Core", "Completed");
    QT_TRANSLATE_NOOP("Core", "Downloading %1");
    QT_TRANSLATE_NOOP("Core", "Installing %1");
    QT_TRANSLATE_NOOP("Core", "Updating %1");
    QT_TRANSLATE_NOOP("Core", "Installing (%1/%2)");
    QT_TRANSLATE_NOOP("Core", "Install failed: %1");
    QT_TRANSLATE_NOOP("Core", "Error: %1");
    QT_TRANSLATE_NOOP("Core", "Add-on %1 — %2");
    (void)&jobDisplayTranslationSeed;
}

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

    if (QString translated = fromPrefixed(title, QStringLiteral("Установка "), "Installing %1");
        !translated.isEmpty())
        return translated;
    if (QString translated = fromPrefixed(title, QStringLiteral("Installing "), "Installing %1");
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
        {"Подготовка…", "Preparing…"},
        {"Preparing…", "Preparing…"},
        {"Подготовка Steam…", "Preparing Steam…"},
        {"Preparing Steam…", "Preparing Steam…"},
        {"Загрузка с Steam CDN…", "Downloading from Steam CDN…"},
        {"Downloading from Steam CDN…", "Downloading from Steam CDN…"},
        {"Получение данных об игре…", "Getting game info…"},
        {"Getting game info…", "Getting game info…"},
        {"Завершение…", "Finishing…"},
        {"Finishing…", "Finishing…"},
        {"Эта игра сейчас недоступна для загрузки. Попробуйте другую.",
         "This game is not available for download right now. Try another title."},
        {"This game is not available for download right now. Try another title.",
         "This game is not available for download right now. Try another title."},
        {"Не удалось подготовить игру к загрузке. Попробуйте позже или выберите другую.",
         "Could not prepare this game for download. Try again later or pick another title."},
        {"Could not prepare this game for download. Try again later or pick another title.",
         "Could not prepare this game for download. Try again later or pick another title."},
        {"Ошибка загрузки. Попробуйте снова или выберите другую игру.",
         "Download failed. Try again or pick another game."},
        {"Download failed. Try again or pick another game.",
         "Download failed. Try again or pick another game."},
        {"Steam заблокировал загрузку файлов (нужен готовый манифест). Попробуйте другую игру или укажите hubcapApiKey в настройках плагина.",
         "Steam blocked downloading game files (need a packaged manifest). Try another title, or set hubcapApiKey in plugin settings."},
        {"Steam blocked downloading game files (need a packaged manifest). Try another title, or set hubcapApiKey in plugin settings.",
         "Steam blocked downloading game files (need a packaged manifest). Try another title, or set hubcapApiKey in plugin settings."},
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
        {"Завершено", "Completed"},
        {"Completed", "Completed"},
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
