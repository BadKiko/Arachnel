#include "torrent_session.h"

#include <QDir>
#include <QTimer>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

namespace arachnel::core {

namespace {

QString statusStateLabel(const lt::torrent_status& status)
{
    switch (status.state) {
    case lt::torrent_status::checking_files:
        return QStringLiteral("checking");
    case lt::torrent_status::downloading_metadata:
        return QStringLiteral("metadata");
    case lt::torrent_status::downloading:
        return QStringLiteral("downloading");
    case lt::torrent_status::finished:
        return QStringLiteral("finished");
    case lt::torrent_status::seeding:
        return QStringLiteral("seeding");
    default:
        return QStringLiteral("queued");
    }
}

} // namespace

struct TorrentSession::Impl
{
    lt::session session{lt::session_params{}};
    QHash<QString, lt::torrent_handle> handles;
    QHash<QString, QString> savePaths;
};

TorrentSession::TorrentSession(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
    , m_available(true)
{
    auto* timer = new QTimer(this);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, this, &TorrentSession::pollAlerts);
    timer->start();
}

TorrentSession::~TorrentSession() = default;

void TorrentSession::addMagnet(const QString& jobId, const QString& magnetUri,
                               const QString& savePath)
{
    if (!m_impl)
        return;

    QDir().mkpath(savePath);

    lt::error_code ec;
    lt::add_torrent_params params = lt::parse_magnet_uri(magnetUri.toStdString(), ec);
    if (ec) {
        emit torrentFailed(jobId, QString::fromStdString(ec.message()));
        return;
    }

    params.save_path = savePath.toStdString();
    params.flags |= lt::torrent_flags::auto_managed;

    const lt::torrent_handle handle = m_impl->session.add_torrent(params, ec);
    if (ec) {
        emit torrentFailed(jobId, QString::fromStdString(ec.message()));
        return;
    }

    m_impl->handles.insert(jobId, handle);
    m_impl->savePaths.insert(jobId, savePath);
}

void TorrentSession::cancel(const QString& jobId)
{
    if (!m_impl)
        return;

    const lt::torrent_handle handle = m_impl->handles.take(jobId);
    m_impl->savePaths.remove(jobId);
    if (handle.is_valid()) {
        m_impl->session.remove_torrent(handle, lt::session::delete_files);
    }
}

void TorrentSession::pollAlerts()
{
    if (!m_impl)
        return;

    std::vector<lt::alert*> alerts;
    m_impl->session.pop_alerts(&alerts);

    for (lt::alert* alert : alerts) {
        if (auto* failed = lt::alert_cast<lt::torrent_error_alert>(alert)) {
            const QString jobId = m_impl->handles.key(failed->handle, QString());
            if (!jobId.isEmpty()) {
                emit torrentFailed(jobId, QString::fromStdString(failed->error.message()));
                m_impl->handles.remove(jobId);
                m_impl->savePaths.remove(jobId);
            }
            if (failed->handle.is_valid())
                m_impl->session.remove_torrent(failed->handle);
            continue;
        }

        if (auto* finished = lt::alert_cast<lt::torrent_finished_alert>(alert)) {
            const QString jobId = m_impl->handles.key(finished->handle, QString());
            if (!jobId.isEmpty()) {
                const QString savePath = m_impl->savePaths.take(jobId);
                m_impl->handles.remove(jobId);
                emit torrentFinished(jobId, savePath);
                // Keep the torrent in session for seeding, but drop our job mapping
                // so cancel() cannot delete_files on an already-finished download.
            }
            continue;
        }
    }

    for (auto it = m_impl->handles.constBegin(); it != m_impl->handles.constEnd(); ++it) {
        const QString jobId = it.key();
        const lt::torrent_handle handle = it.value();
        if (!handle.is_valid())
            continue;

        const lt::torrent_status status = handle.status();
        const qint64 total = status.total_wanted;
        const qint64 downloaded = status.total_wanted_done;
        const int progress = total > 0 ? static_cast<int>((downloaded * 100) / total) : 0;
        emit torrentProgress(jobId, progress, downloaded, total,
                             static_cast<int>(status.download_rate), status.num_peers,
                             statusStateLabel(status));
    }
}

} // namespace arachnel::core
