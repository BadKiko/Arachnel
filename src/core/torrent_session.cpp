#include "torrent_session.h"

#include "torrent_settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/write_resume_data.hpp>

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

QString libtorrentErrorMessage(const lt::error_code& ec)
{
    const std::string& msg = ec.message();
    if (msg.empty())
        return QCoreApplication::translate("Core", "Torrent error %1").arg(ec.value());

    QString text = QString::fromUtf8(msg.data(), static_cast<int>(msg.size()));
    if (text.contains(QChar::ReplacementCharacter))
        text = QString::fromLocal8Bit(msg.data(), static_cast<int>(msg.size()));
    if (text.isEmpty())
        return QCoreApplication::translate("Core", "Torrent error %1").arg(ec.value());
    return text;
}

} // namespace

namespace arachnel::core {

struct TorrentSession::Impl
{
    lt::session session{lt::session_params{}};
    QHash<QString, lt::torrent_handle> handles;
    QHash<QString, QString> savePaths;
    QHash<QString, QString> magnetUris;
    QSet<QString> pausedJobs;
    QHash<QString, qint64> metadataStallSinceMs;
    QHash<QString, qint64> lastPeerRefreshMs;
};

namespace {

constexpr int kMetadataKickAfterMs = 3000;

void kickStalledMetadata(const QString& jobId, lt::torrent_handle handle,
                         QHash<QString, qint64>& metadataStallSinceMs)
{
    if (!handle.is_valid())
        return;

    const lt::torrent_status status = handle.status();
    if (status.state != lt::torrent_status::downloading_metadata) {
        metadataStallSinceMs.remove(jobId);
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 since = metadataStallSinceMs.value(jobId, now);
    if (!metadataStallSinceMs.contains(jobId))
        metadataStallSinceMs.insert(jobId, now);

    if (status.num_peers > 0 || status.download_rate > 0) {
        metadataStallSinceMs.remove(jobId);
        return;
    }

    if (now - since < kMetadataKickAfterMs)
        return;

    handle.resume();
    handle.force_reannounce(0, lt::torrent_handle::ignore_min_interval);
    handle.force_dht_announce();
    metadataStallSinceMs.insert(jobId, now);
}

} // namespace

TorrentSession::TorrentSession(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
    , m_available(true)
{
    torrent_settings::applySessionDefaults(m_impl->session);
    QDir().mkpath(resumeDirectory());

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(500);
    connect(m_pollTimer, &QTimer::timeout, this, &TorrentSession::pollAlerts);

    m_resumeTimer = new QTimer(this);
    m_resumeTimer->setInterval(30000);
    connect(m_resumeTimer, &QTimer::timeout, this, &TorrentSession::saveAllResumeData);
}

void TorrentSession::updateIdleTimers()
{
    const bool active = m_impl && !m_impl->handles.isEmpty();
    if (m_pollTimer) {
        if (active)
            m_pollTimer->start();
        else
            m_pollTimer->stop();
    }
    if (m_resumeTimer) {
        if (active)
            m_resumeTimer->start();
        else
            m_resumeTimer->stop();
    }
}

TorrentSession::~TorrentSession()
{
    shutdown();
}

void TorrentSession::shutdown()
{
    if (!m_impl)
        return;

    if (QCoreApplication::instance())
        flushResumeData();

    m_impl.reset();
    m_available = false;
}

QString TorrentSession::resumeDirectory()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/torrent_resume");
    QDir().mkpath(dir);
    return dir;
}

QString TorrentSession::resumeFilePath(const QString& jobId) const
{
    return resumeDirectory() + QLatin1Char('/') + jobId + QStringLiteral(".resume");
}

void TorrentSession::requestResumeSave(const QString& jobId)
{
    if (!m_impl)
        return;
    const lt::torrent_handle handle = m_impl->handles.value(jobId);
    if (handle.is_valid())
        handle.save_resume_data(lt::torrent_handle::save_info_dict);
}

void TorrentSession::saveAllResumeData()
{
    if (!m_impl)
        return;
    for (auto it = m_impl->handles.constBegin(); it != m_impl->handles.constEnd(); ++it)
        requestResumeSave(it.key());
}

void TorrentSession::flushResumeData()
{
    if (!m_impl)
        return;

    saveAllResumeData();
    for (int i = 0; i < 40; ++i) {
        pollAlerts();
        if (!QCoreApplication::instance())
            break;
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 25);
    }
}

void TorrentSession::removeResumeFile(const QString& jobId)
{
    QFile::remove(resumeFilePath(jobId));
}

bool TorrentSession::addJob(const QString& jobId, const QString& magnetUri, const QString& savePath)
{
    if (!m_impl)
        return false;

    if (m_impl->handles.contains(jobId)) {
        torrent_settings::tuneActiveDownloadHandle(m_impl->handles.value(jobId));
        updateIdleTimers();
        return true;
    }

    QDir().mkpath(savePath);

    lt::error_code ec;
    lt::add_torrent_params params;
    bool usedResume = false;

    const QString resumePath = resumeFilePath(jobId);
    if (QFile::exists(resumePath)) {
        QFile file(resumePath);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray data = file.readAll();
            params = lt::read_resume_data(
                lt::span<char const>(data.constData(), data.size()), ec);
            usedResume = !ec;
        }
    }

    if (!usedResume) {
        if (magnetUri.isEmpty()) {
            emit torrentFailed(jobId,
                               QCoreApplication::translate("Core", "No download link"));
            return false;
        }
        params = lt::parse_magnet_uri(magnetUri.toStdString(), ec);
        if (ec) {
            emit torrentFailed(jobId, libtorrentErrorMessage(ec));
            return false;
        }
    }

    torrent_settings::prepareDownloadParams(params, usedResume);

    params.save_path = savePath.toStdString();

    const lt::torrent_handle handle = m_impl->session.add_torrent(params, ec);
    if (ec) {
        emit torrentFailed(jobId, libtorrentErrorMessage(ec));
        return false;
    }

    torrent_settings::tuneActiveDownloadHandle(handle);
    m_impl->metadataStallSinceMs.remove(jobId);
    m_impl->handles.insert(jobId, handle);
    m_impl->savePaths.insert(jobId, savePath);
    m_impl->magnetUris.insert(jobId, magnetUri);
    updateIdleTimers();
    return true;
}

void TorrentSession::cancel(const QString& jobId, bool deleteFiles)
{
    if (!m_impl)
        return;

    const lt::torrent_handle handle = m_impl->handles.take(jobId);
    m_impl->savePaths.remove(jobId);
    m_impl->magnetUris.remove(jobId);
    m_impl->pausedJobs.remove(jobId);
    m_impl->metadataStallSinceMs.remove(jobId);
    m_impl->lastPeerRefreshMs.remove(jobId);

    removeResumeFile(jobId);

    if (handle.is_valid()) {
        const auto flags = deleteFiles ? lt::session::delete_files : lt::session::delete_partfile;
        m_impl->session.remove_torrent(handle, flags);
    }
    updateIdleTimers();
}

void TorrentSession::setPaused(const QString& jobId, bool paused)
{
    if (!m_impl)
        return;

    const lt::torrent_handle handle = m_impl->handles.value(jobId);
    if (!handle.is_valid())
        return;

    if (paused) {
        handle.unset_flags(lt::torrent_flags::auto_managed);
        handle.pause();
        m_impl->pausedJobs.insert(jobId);
    } else {
        torrent_settings::tuneActiveDownloadHandle(handle);
        m_impl->pausedJobs.remove(jobId);
        m_impl->metadataStallSinceMs.remove(jobId);
    }
    requestResumeSave(jobId);
}

bool TorrentSession::isPaused(const QString& jobId) const
{
    return m_impl && m_impl->pausedJobs.contains(jobId);
}

void TorrentSession::pollAlerts()
{
    if (!m_impl)
        return;

    std::vector<lt::alert*> alerts;
    m_impl->session.pop_alerts(&alerts);

    const auto findJobId = [this](const lt::torrent_handle& handle) -> QString {
        if (!handle.is_valid())
            return {};
        for (auto it = m_impl->handles.constBegin(); it != m_impl->handles.constEnd(); ++it) {
            if (it.value() == handle)
                return it.key();
        }
        return {};
    };

    for (lt::alert* alert : alerts) {
        if (auto* resume = lt::alert_cast<lt::save_resume_data_alert>(alert)) {
            const QString jobId = findJobId(resume->handle);
            if (jobId.isEmpty())
                continue;

            const std::vector<char> buffer = lt::write_resume_data_buf(resume->params);
            QFile file(resumeFilePath(jobId));
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(buffer.data(), static_cast<qint64>(buffer.size()));
            continue;
        }

        if (auto* meta = lt::alert_cast<lt::metadata_received_alert>(alert)) {
            const QString jobId = findJobId(meta->handle);
            if (!jobId.isEmpty() && !m_impl->pausedJobs.contains(jobId))
                torrent_settings::tuneActiveDownloadHandle(meta->handle);
            continue;
        }

        if (auto* failed = lt::alert_cast<lt::torrent_error_alert>(alert)) {
            const QString jobId = findJobId(failed->handle);
            if (!jobId.isEmpty()) {
                emit torrentFailed(jobId, libtorrentErrorMessage(failed->error));
                m_impl->handles.remove(jobId);
                m_impl->savePaths.remove(jobId);
                m_impl->magnetUris.remove(jobId);
                m_impl->pausedJobs.remove(jobId);
                m_impl->metadataStallSinceMs.remove(jobId);
            }
            if (failed->handle.is_valid())
                m_impl->session.remove_torrent(failed->handle);
            continue;
        }

        if (auto* finished = lt::alert_cast<lt::torrent_finished_alert>(alert)) {
            const QString jobId = findJobId(finished->handle);
            if (!jobId.isEmpty()) {
                const QString savePath = m_impl->savePaths.take(jobId);
                m_impl->handles.remove(jobId);
                m_impl->magnetUris.remove(jobId);
                m_impl->pausedJobs.remove(jobId);
                m_impl->metadataStallSinceMs.remove(jobId);
                removeResumeFile(jobId);
                emit torrentFinished(jobId, savePath);
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
        const QString state =
            m_impl->pausedJobs.contains(jobId) ? QStringLiteral("paused") : statusStateLabel(status);
        const int downloadRate =
            m_impl->pausedJobs.contains(jobId) ? 0 : static_cast<int>(status.download_rate);
        emit torrentProgress(jobId, progress, downloaded, total, downloadRate, status.num_peers,
                             state);

        if (!m_impl->pausedJobs.contains(jobId)) {
            kickStalledMetadata(jobId, handle, m_impl->metadataStallSinceMs);

            if (status.state == lt::torrent_status::downloading_metadata) {
                torrent_settings::maintainDownloadHandle(handle, status.num_peers,
                                                         static_cast<int>(status.download_rate));
            } else if (status.state == lt::torrent_status::downloading) {
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                const qint64 lastRefresh = m_impl->lastPeerRefreshMs.value(jobId, 0);
                if (now - lastRefresh >= 15000) {
                    torrent_settings::maintainDownloadHandle(handle, status.num_peers,
                                                             downloadRate);
                    m_impl->lastPeerRefreshMs.insert(jobId, now);
                }
            }
        }
    }

    QStringList completedJobs;
    for (auto it = m_impl->handles.constBegin(); it != m_impl->handles.constEnd(); ++it) {
        const QString jobId = it.key();
        if (m_impl->pausedJobs.contains(jobId))
            continue;

        const lt::torrent_handle handle = it.value();
        if (!handle.is_valid())
            continue;

        const lt::torrent_status status = handle.status();
        const qint64 total = status.total_wanted;
        const qint64 downloaded = status.total_wanted_done;
        if (total <= 0 || downloaded < total)
            continue;

        if (status.state == lt::torrent_status::seeding
            || status.state == lt::torrent_status::finished)
            completedJobs.append(jobId);
    }

    for (const QString& jobId : completedJobs) {
        const lt::torrent_handle handle = m_impl->handles.take(jobId);
        const QString savePath = m_impl->savePaths.take(jobId);
        m_impl->magnetUris.remove(jobId);
        m_impl->pausedJobs.remove(jobId);
        removeResumeFile(jobId);
        if (handle.is_valid())
            m_impl->session.remove_torrent(handle, lt::session::delete_partfile);
        emit torrentFinished(jobId, savePath);
    }

    updateIdleTimers();
}

} // namespace arachnel::core
