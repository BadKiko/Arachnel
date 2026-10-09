#pragma once

#include "catalog_types.h"

#include <QString>
#include <QVector>

namespace arachnel::core {

/**
 * Helpers for adding a game that is already on disk (downloaded elsewhere, e.g. from a
 * torrent) to the library. Pure functions so they stay easy to reason about.
 */

/** "Hollow.Knight.v1.5.4-GOG [RePack]" -> "Hollow Knight". */
QString cleanGameTitleFromFolderName(const QString& folderName);

/**
 * Steam app id the files were made for: OnlineFix.ini RealAppId, steam_appid.txt,
 * steam_settings/steam_appid.txt, steam_emu.ini / ColdClientLoader.ini. Looks in the exe
 * folder first, then the game root. Spacewar (480) is ignored. Empty when unknown.
 */
QString detectSteamAppIdForImport(const QString& gameRoot, const QString& executablePath);

/**
 * Executable to start for a user-picked folder. Same heuristics as installs, but a small exe
 * in a subfolder (bin/, Game/) is still accepted: fall back to the biggest non-utility .exe.
 */
QString findImportExecutable(const QString& gameRoot, const QString& title);

/** 0..100, how well a catalog title matches the guessed one. */
int importTitleScore(const QString& wanted, const QString& candidate);

/** Id for a game that has no catalog match: "local-hollow-knight". */
QString localGameIdFromTitle(const QString& title);

/**
 * Best catalog matches for a folder: exact Steam app id first, then by title score.
 * Only real games (no DLC rows).
 */
QVector<CatalogEntry> pickImportCandidates(const QVector<CatalogEntry>& catalog,
                                           const QString& title, const QString& steamAppId,
                                           int limit);

} // namespace arachnel::core
