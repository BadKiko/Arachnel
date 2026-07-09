#include "core_controller.h"

#include <QJSEngine>
#include <QQmlEngine>
#include <QtQml/qqml.h>
#include <QUuid>

namespace arachnel::core {

namespace {

QString steamCover(const char* appId)
{
    return QStringLiteral(
               "https://shared.fastly.steamstatic.com/store_item_assets/steam/apps/%1/"
               "library_600x900.jpg")
        .arg(QString::fromLatin1(appId));
}

} // namespace

CoreController* CoreController::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    return &instance();
}

CoreController& CoreController::instance()
{
    static CoreController controller;
    return controller;
}

CoreController::CoreController(QObject* parent)
    : QObject(parent)
{
    loadMockData();
}

void CoreController::loadMockData()
{
    m_sources.setPlugins({
        {QStringLiteral("online-fix"),
         QStringLiteral("Online-Fix"),
         QStringLiteral("Portable-архивы, распаковка в библиотеку"),
         {QStringLiteral("search"), QStringLiteral("install"), QStringLiteral("update")}},
        {QStringLiteral("freetp"),
         QStringLiteral("FreeTP"),
         QStringLiteral("Installer, portable, встроенный или отдельный фикс"),
         {QStringLiteral("search"), QStringLiteral("install"), QStringLiteral("update")}},
    });

    m_library.setGames({
        {QStringLiteral("elden-ring"),
         QStringLiteral("Elden Ring"),
         steamCover("1245620"),
         QStringLiteral("online-fix"),
         QStringLiteral("Online-Fix"),
         QStringLiteral("1.12.0"),
         QStringLiteral("~/Games/Arachnel/elden-ring"),
         QStringLiteral(
             "Открытый мир от FromSoftware: исследуйте Междуземье, сражайтесь с боссами "
             "и собирайте билды под свой стиль. Portable-сборка с Online-Fix."),
         QStringLiteral("Action RPG, Open World"),
         QStringLiteral("60 GB"),
         InstallKind::PortableArchive,
         true},
        {QStringLiteral("hogwarts-legacy"),
         QStringLiteral("Hogwarts Legacy"),
         steamCover("990080"),
         QStringLiteral("freetp"),
         QStringLiteral("FreeTP"),
         QStringLiteral("2.0.1"),
         QStringLiteral("~/Games/Arachnel/hogwarts-legacy"),
         QStringLiteral(
             "Приключенческая RPG во вселенной Гарри Поттера: учёба в Хогвартсе, "
             "заклинания, исследование замка и окрестностей. Установка через installer."),
         QStringLiteral("Action RPG, Adventure"),
         QStringLiteral("85 GB"),
         InstallKind::Installer,
         false},
        {QStringLiteral("cyberpunk-2077"),
         QStringLiteral("Cyberpunk 2077"),
         steamCover("1091500"),
         QStringLiteral("online-fix"),
         QStringLiteral("Online-Fix"),
         QStringLiteral("2.1"),
         QStringLiteral("~/Games/Arachnel/cyberpunk-2077"),
         QStringLiteral(
             "Открытый мир Night City: история Ви, кибернетика, фракции и сюжетные ветки. "
             "Сборка с bundled fix для запуска."),
         QStringLiteral("RPG, Open World, Sci-Fi"),
         QStringLiteral("70 GB"),
         InstallKind::BundledFix,
         false},
    });

    m_jobs.setJobs({
        {QStringLiteral("job-1"),
         QStringLiteral("Обновление Elden Ring"),
         QStringLiteral("queued"),
         0},
    });
}

void CoreController::setLastAction(const QString& action)
{
    if (m_lastAction == action)
        return;
    m_lastAction = action;
    emit lastActionChanged();
}

void CoreController::launchGame(const QString& gameId)
{
    const LibraryGame* game = m_library.gameById(gameId);
    if (!game) {
        setLastAction(QStringLiteral("Игра не найдена: %1").arg(gameId));
        return;
    }
    setLastAction(QStringLiteral("Запуск (mock): %1 из %2").arg(game->title, game->installPath));
}

QVector<CatalogEntry> CoreController::mockCatalogFor(const QString& sourceId,
                                                     const QString& query) const
{
    const QString needle = query.trimmed().toLower();

    auto matches = [&](const QString& title) {
        return needle.isEmpty() || title.toLower().contains(needle);
    };

    if (sourceId == QStringLiteral("online-fix")) {
        QVector<CatalogEntry> entries;
        const auto add = [&](const QString& id, const QString& title, const char* steamAppId,
                             const QString& version, const QString& size, InstallKind kind,
                             const QString& description, const QString& genres) {
            if (matches(title))
                entries.append({id, title, steamCover(steamAppId), sourceId, version, size,
                                description, genres, kind});
        };
        add(QStringLiteral("of-baldurs-gate-3"), QStringLiteral("Baldur's Gate 3"), "1086940",
            QStringLiteral("4.1.1"), QStringLiteral("62 GB"), InstallKind::PortableArchive,
            QStringLiteral(
                "Партийнная RPG по D&D 5e: кооператив, ветвящийся сюжет и тактические бои."),
            QStringLiteral("CRPG, Party-based"));
        add(QStringLiteral("of-red-dead-2"), QStringLiteral("Red Dead Redemption 2"), "1174180",
            QStringLiteral("1.0.1436"), QStringLiteral("118 GB"), InstallKind::PortableArchive,
            QStringLiteral(
                "Эпический вестерн Rockstar: банда Ван дер Линде, охота и открытый мир."),
            QStringLiteral("Action Adventure, Open World"));
        add(QStringLiteral("of-sekiro"), QStringLiteral("Sekiro: Shadows Die Twice"), "814380",
            QStringLiteral("1.06"), QStringLiteral("24 GB"), InstallKind::PortableArchive,
            QStringLiteral(
                "Жёсткий экшен FromSoftware: парирования, скрытность и боссы в Сэнгоку."),
            QStringLiteral("Action, Souls-like"));
        return entries;
    }

    if (sourceId == QStringLiteral("freetp")) {
        QVector<CatalogEntry> entries;
        const auto add = [&](const QString& id, const QString& title, const char* steamAppId,
                             const QString& version, const QString& size, InstallKind kind,
                             const QString& description, const QString& genres) {
            if (matches(title))
                entries.append({id, title, steamCover(steamAppId), sourceId, version, size,
                                description, genres, kind});
        };
        add(QStringLiteral("ft-starfield"), QStringLiteral("Starfield"), "1716740",
            QStringLiteral("1.7.33"), QStringLiteral("installer · 95 GB"), InstallKind::Installer,
            QStringLiteral(
                "Космическая RPG Bethesda: исследование систем, корабли и создание персонажа."),
            QStringLiteral("RPG, Space, Exploration"));
        add(QStringLiteral("ft-lies-of-p"), QStringLiteral("Lies of P"), "1627720",
            QStringLiteral("1.2.0"), QStringLiteral("portable · 31 GB"), InstallKind::BundledFix,
            QStringLiteral(
                "Souls-like по мотивам Пиноккио: комбо оружия, ложь и мрачный Belle Époque."),
            QStringLiteral("Action RPG, Souls-like"));
        add(QStringLiteral("ft-dying-light-2"), QStringLiteral("Dying Light 2"), "534380",
            QStringLiteral("1.9.0"), QStringLiteral("fix download · 48 GB"),
            InstallKind::FixDownload,
            QStringLiteral(
                "Паркур и зомби в постапокалипсисе: день/ночь, выборы фракций и кооп."),
            QStringLiteral("Action, Survival, Parkour"));
        return entries;
    }

    return {};
}

void CoreController::searchCatalog(const QString& sourceId, const QString& query)
{
    if (!m_sources.pluginById(sourceId)) {
        m_catalog.clear();
        setLastAction(QStringLiteral("Неизвестный источник: %1").arg(sourceId));
        return;
    }

    m_catalog.setEntries(mockCatalogFor(sourceId, query));
    setLastAction(QStringLiteral("Каталог %1: найдено %2").arg(sourceId).arg(m_catalog.rowCount()));
}

void CoreController::installCatalogEntry(const QString& entryId)
{
    const CatalogEntry* entry = m_catalog.entryById(entryId);
    if (!entry) {
        setLastAction(QStringLiteral("Запись каталога не найдена: %1").arg(entryId));
        return;
    }

    const QString jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_jobs.addJob({jobId,
                   QStringLiteral("Установка %1").arg(entry->title),
                   QStringLiteral("download"),
                   15});
    setLastAction(QStringLiteral("Установка (mock): %1 [%2]")
                      .arg(entry->title, installKindLabel(entry->installKind)));
}

void CoreController::checkUpdates()
{
    QVector<LibraryGame> games;
    games.reserve(m_library.rowCount());
    for (int i = 0; i < m_library.rowCount(); ++i) {
        const QModelIndex idx = m_library.index(i, 0);
        LibraryGame game;
        game.id = m_library.data(idx, LibraryModel::GameIdRole).toString();
        game.title = m_library.data(idx, LibraryModel::TitleRole).toString();
        game.coverUrl = m_library.data(idx, LibraryModel::CoverUrlRole).toString();
        game.sourceId = m_library.data(idx, LibraryModel::SourceIdRole).toString();
        game.sourceName = m_library.data(idx, LibraryModel::SourceNameRole).toString();
        game.version = m_library.data(idx, LibraryModel::VersionRole).toString();
        game.installPath = m_library.data(idx, LibraryModel::InstallPathRole).toString();
        game.description = m_library.data(idx, LibraryModel::DescriptionRole).toString();
        game.genres = m_library.data(idx, LibraryModel::GenresRole).toString();
        game.sizeLabel = m_library.data(idx, LibraryModel::SizeLabelRole).toString();
        game.installKind =
            static_cast<InstallKind>(m_library.data(idx, LibraryModel::InstallKindRole).toInt());
        game.hasUpdate = (i % 2 == 0);
        games.append(game);
    }
    m_library.setGames(std::move(games));
    setLastAction(QStringLiteral("Проверка обновлений (mock) завершена"));
}

void registerCoreTypes()
{
    qmlRegisterSingletonType<CoreController>("Arachnel.Core", 1, 0, "Core", &CoreController::create);
    qmlRegisterUncreatableType<LibraryModel>("Arachnel.Core", 1, 0, "LibraryModel",
                                             QStringLiteral("Use Core.library"));
    qmlRegisterUncreatableType<SourcePluginModel>("Arachnel.Core", 1, 0, "SourcePluginModel",
                                                  QStringLiteral("Use Core.sources"));
    qmlRegisterUncreatableType<CatalogModel>("Arachnel.Core", 1, 0, "CatalogModel",
                                             QStringLiteral("Use Core.catalog"));
    qmlRegisterUncreatableType<JobModel>("Arachnel.Core", 1, 0, "JobModel",
                                         QStringLiteral("Use Core.jobs"));
}

} // namespace arachnel::core
