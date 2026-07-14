#include "install_analyzer.h"

#include "install_heuristics.h"
#include "plugin_host.h"

#include <algorithm>

namespace arachnel::core {

InstallAnalyzer::InstallAnalyzer(PluginHost* pluginHost)
    : m_pluginHost(pluginHost)
{
}

QVector<InstallAnalyzer::Candidate> InstallAnalyzer::collectDownloadCandidates(
    const InstallContext& ctx) const
{
    QVector<Candidate> candidates;
    candidates.reserve(m_pluginHost ? m_pluginHost->count() + 1 : 1);

    {
        Candidate coreCandidate;
        coreCandidate.pluginId = QStringLiteral("core");
        coreCandidate.analysis = analyzeDownloadPath(ctx.downloadPath);
        candidates.append(coreCandidate);
    }

    if (!m_pluginHost)
        return candidates;

    for (const QString& pluginId : m_pluginHost->pluginIds()) {
        ISourcePlugin* plugin = m_pluginHost->plugin(pluginId);
        if (!plugin)
            continue;

        Candidate candidate;
        candidate.pluginId = pluginId;
        candidate.plugin = plugin;
        candidate.analysis = plugin->analyzeDownload(ctx);
        candidates.append(candidate);
    }

    return candidates;
}

QVector<InstallAnalyzer::Candidate> InstallAnalyzer::collectFileNameCandidates(
    const QString& sourceId, const QStringList& fileNames) const
{
    QVector<Candidate> candidates;
    candidates.reserve(m_pluginHost ? m_pluginHost->count() + 1 : 1);

    {
        Candidate coreCandidate;
        coreCandidate.pluginId = QStringLiteral("core");
        coreCandidate.analysis = analyzeTorrentFileNames(fileNames);
        candidates.append(coreCandidate);
    }

    if (!m_pluginHost)
        return candidates;

    for (const QString& pluginId : m_pluginHost->pluginIds()) {
        ISourcePlugin* plugin = m_pluginHost->plugin(pluginId);
        if (!plugin)
            continue;

        Candidate candidate;
        candidate.pluginId = pluginId;
        candidate.plugin = plugin;
        candidate.analysis = plugin->analyzeFileNames(fileNames);
        candidates.append(candidate);
    }

    return candidates;
}

InstallPlan InstallAnalyzer::pickBestPlan(const QVector<Candidate>& candidates,
                                          const InstallAnalysis& fallbackAnalysis,
                                          const QString& sourceId) const
{
    InstallPlan plan;
    plan.analysis = fallbackAnalysis;
    plan.needsUserInput = true;

    const Candidate* bestInstallable = nullptr;
    int bestInstallableScore = -1;
    const Candidate* bestOverall = nullptr;
    int bestOverallScore = -1;

    for (const Candidate& candidate : candidates) {
        int score = candidate.analysis.confidence;
        if (candidate.pluginId == sourceId)
            score += 5;
        if (!candidate.analysis.canInstall)
            score -= 100;

        if (score > bestOverallScore) {
            bestOverallScore = score;
            bestOverall = &candidate;
        }

        if (!candidate.analysis.canInstall || !candidate.plugin)
            continue;

        if (score > bestInstallableScore) {
            bestInstallableScore = score;
            bestInstallable = &candidate;
        }
    }

    const Candidate* chosen = bestInstallable ? bestInstallable : bestOverall;
    if (!chosen)
        return plan;

    plan.analysis = chosen->analysis;
    if (chosen->plugin) {
        plan.installerPlugin = chosen->plugin;
        plan.installerPluginId = chosen->pluginId;
        plan.needsUserInput = chosen->analysis.confidence < 40;
    } else {
        plan.installerPlugin = bestInstallable ? bestInstallable->plugin : nullptr;
        plan.installerPluginId = bestInstallable ? bestInstallable->pluginId : QString();
        plan.needsUserInput = true;
    }

    if (plan.installerPlugin)
        plan.needsUserInput = plan.analysis.confidence < 40;

    return plan;
}

InstallPlan InstallAnalyzer::resolveDownload(const InstallContext& ctx) const
{
    const QVector<Candidate> candidates = collectDownloadCandidates(ctx);
    const InstallAnalysis fallback = analyzeDownloadPath(ctx.downloadPath);
    return pickBestPlan(candidates, fallback, ctx.sourceId);
}

InstallPlan InstallAnalyzer::resolveFileNames(const QString& sourceId,
                                              const QStringList& fileNames) const
{
    const QVector<Candidate> candidates = collectFileNameCandidates(sourceId, fileNames);
    const InstallAnalysis fallback = analyzeTorrentFileNames(fileNames);
    return pickBestPlan(candidates, fallback, sourceId);
}

} // namespace arachnel::core
