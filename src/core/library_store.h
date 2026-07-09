#pragma once

#include "library_model.h"

#include <QObject>
#include <QVector>

namespace arachnel::core {

class LibraryStore : public QObject
{
    Q_OBJECT

public:
    explicit LibraryStore(QObject* parent = nullptr);

    QVector<LibraryGame> games() const { return m_games; }
    void setGames(QVector<LibraryGame> games);

    const LibraryGame* gameById(const QString& id) const;
    void upsertGame(const LibraryGame& game);
    void removeGame(const QString& id);

    void load();
    void save();

signals:
    void gamesChanged();

private:
    QVector<LibraryGame> m_games;
};

} // namespace arachnel::core
