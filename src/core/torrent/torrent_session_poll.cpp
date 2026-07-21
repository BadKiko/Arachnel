#include "torrent_session.h"

#include "torrent_settings.h"


#include "torrent_session_internal.h"

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
