#pragma once

#include "library_model.h"

#include <QObject>
#include <QTimer>
#include <QVector>

namespace arachnel::core {

class LibraryStore : public QObject
{
    Q_OBJECT

public:
    explicit LibraryStore(QObject* parent = nullptr);
    ~LibraryStore() override;

    QVector<LibraryGame> games() const { return m_games; }
    void setGames(QVector<LibraryGame> games);

    const LibraryGame* gameById(const QString& id) const;
    void upsertGame(const LibraryGame& game);
    void removeGame(const QString& id);

    void load();
    /** Marks the library dirty; the file is written shortly after (coalesces bursts of changes). */
    void save();
    /** Write now if there are unsaved changes (also runs on quit and from the destructor). */
    void flush();

signals:
    void gamesChanged();

private:
    QVector<LibraryGame> m_games;
    QTimer m_saveTimer;
    bool m_dirty = false;
};

} // namespace arachnel::core
