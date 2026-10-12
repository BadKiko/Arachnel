#pragma once

#include <QByteArray>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

/**
 * The server-built list of Steam app ids with nudity / sexual content (see scripts/adult).
 *
 * The catalog feeds do not say which games are 18+, so the list is built once on the server
 * from the store's content descriptors and downloaded here: a small file, cached on disk and
 * revalidated with an ETag. Nothing is looked up per game on the client, so hidden games are
 * hidden from the first frame on and never flicker in and out.
 */
class AdultListService : public QObject
{
    Q_OBJECT
public:
    explicit AdultListService(QObject* parent = nullptr);

    /** Read the cached list from disk (fast; call once at startup). */
    void loadCache();
    bool isEmpty() const { return m_ids.isEmpty(); }
    bool contains(const QString& steamAppId) const;
    /** Download a newer list if the server has one (no-op while one is in flight). */
    void refresh();

signals:
    /** A different list arrived. */
    void changed();

private:
    void handleReply(QNetworkReply* reply);
    void save() const;
    static QString cacheFilePath();

    QNetworkAccessManager* m_network = nullptr;
    QSet<quint32> m_ids;
    QByteArray m_etag;
    bool m_running = false;
};

} // namespace arachnel::core
