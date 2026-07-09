#pragma once

#include <QObject>
#include <QString>

namespace arachnel::core {

class SettingsStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString libraryRoot READ libraryRoot WRITE setLibraryRoot NOTIFY libraryRootChanged)
    Q_PROPERTY(QString downloadsRoot READ downloadsRoot WRITE setDownloadsRoot NOTIFY downloadsRootChanged)
    Q_PROPERTY(QString freetpCatalogUrl READ freetpCatalogUrl WRITE setFreetpCatalogUrl NOTIFY
                   freetpCatalogUrlChanged)

public:
    explicit SettingsStore(QObject* parent = nullptr);

    QString libraryRoot() const { return m_libraryRoot; }
    QString downloadsRoot() const { return m_downloadsRoot; }
    QString freetpCatalogUrl() const { return m_freetpCatalogUrl; }

    QString catalogUrlForSource(const QString& sourceId) const;
    QString resolvedLibraryRoot() const;
    QString resolvedDownloadsRoot() const;

    void setLibraryRoot(const QString& path);
    void setDownloadsRoot(const QString& path);
    void setFreetpCatalogUrl(const QString& url);

    void load();
    void save();

signals:
    void libraryRootChanged();
    void downloadsRootChanged();
    void freetpCatalogUrlChanged();

private:
    QString m_libraryRoot;
    QString m_downloadsRoot;
    QString m_freetpCatalogUrl;
};

} // namespace arachnel::core
