#include "torrent_session.h"

#include "torrent_settings.h"


#include "torrent_session_internal.h"

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


} // namespace arachnel::core
