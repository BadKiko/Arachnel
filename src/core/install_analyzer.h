#pragma once

#include "install_analysis.h"
#include "plugin_interface.h"

#include <QString>
#include <QStringList>

namespace arachnel::core {

class PluginHost;

struct InstallPlan {
    InstallAnalysis analysis;
    QString installerPluginId;
    ISourcePlugin* installerPlugin = nullptr;
    bool needsUserInput = false;
};

class InstallAnalyzer
{
public:
    explicit InstallAnalyzer(PluginHost* pluginHost);

    InstallPlan resolveDownload(const InstallContext& ctx) const;
    InstallPlan resolveFileNames(const QString& sourceId, const QStringList& fileNames) const;

private:
    struct Candidate {
        QString pluginId;
        ISourcePlugin* plugin = nullptr;
        InstallAnalysis analysis;
    };

    QVector<Candidate> collectDownloadCandidates(const InstallContext& ctx) const;
    QVector<Candidate> collectFileNameCandidates(const QString& sourceId,
                                               const QStringList& fileNames) const;
    InstallPlan pickBestPlan(const QVector<Candidate>& candidates,
                             const InstallAnalysis& fallbackAnalysis,
                             const QString& sourceId) const;

    PluginHost* m_pluginHost = nullptr;
};

} // namespace arachnel::core
