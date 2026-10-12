#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace arachnel::core {

/**
 * Finds Steam games with nudity / sexual content from the store's content descriptors.
 *
 * Neither the catalog feeds nor Steam's genre list say it (about 100 of 150k entries have a
 * Nudity / Sexual Content genre), and store search hides adult-only games, so the descriptors are
 * read from the store's batch item API (200 apps per request). Only the games about to be shown
 * are asked for - the head of the current result list - and every answer is cached on disk, so a
 * game is looked up once. Nothing is fetched while adult games are shown anyway.
 */
class AdultScanService : public QObject
{
    Q_OBJECT
public:
    explicit AdultScanService(QObject* parent = nullptr);

    /** Read the cached answers from disk (fast; call once at startup). */
    void loadCache();
    bool contains(const QString& steamAppId) const;
    bool hasAdult() const { return !m_adult.isEmpty() || !m_unlisted.isEmpty(); }
    /** The store has no page for this app (removed or never public): nothing to read there. */
    bool isUnlisted(const QString& steamAppId) const;

    /**
     * Ask about these apps next (those already answered are skipped). Replaces the previous
     * wishlist, so a new search is answered before the one the user left.
     */
    void setWanted(const QVector<quint32>& appIds);
    /** Drop the wishlist (adult games are visible again, nobody needs the answers now). */
    void cancel();

signals:
    /** New adult games were found since the last signal. */
    void foundAdult();

private:
    void pump();
    void handleReply(QNetworkReply* reply);
    void scheduleSave();
    void save() const;
    static QString cacheFilePath();

    QNetworkAccessManager* m_network = nullptr;
    QSet<quint32> m_adult;
    QSet<quint32> m_checked;
    QSet<quint32> m_unlisted;
    QSet<quint32> m_parentAsked; ///< children already re-asked once after their parent
    QVector<quint32> m_wanted;
    bool m_running = false;
    bool m_foundSinceSignal = false;
    qint64 m_retryNotBefore = 0;
    QTimer* m_pumpTimer = nullptr;
    QTimer* m_saveTimer = nullptr;
    QTimer* m_signalTimer = nullptr;
};

} // namespace arachnel::core
