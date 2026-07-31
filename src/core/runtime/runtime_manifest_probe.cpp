#include "runtime_manifest_probe.h"

#include "runtime_depot_catalog.h"

#include <QFile>

namespace arachnel::core {

namespace {

QString extractEmbeddedManifest(const QByteArray& peData)
{
    const int marker = peData.indexOf("<?xml");
    if (marker >= 0)
        return QString::fromUtf8(peData.constData() + marker, peData.size() - marker);

    const int asmMarker = peData.indexOf("<assembly");
    if (asmMarker >= 0)
        return QString::fromUtf8(peData.constData() + asmMarker, peData.size() - asmMarker);

    return {};
}

void parseManifestText(const QString& manifest, ManifestRuntimeNeeds* out)
{
    if (!out || manifest.isEmpty())
        return;

    const QString lower = manifest.toLower();
    const bool vc143 = lower.contains(QStringLiteral("microsoft.vc143.crt"))
                       || lower.contains(QStringLiteral("vc143"));
    const bool vc142 = lower.contains(QStringLiteral("microsoft.vc142.crt"))
                       || lower.contains(QStringLiteral("vc142"))
                       || lower.contains(QStringLiteral("vcruntime140"))
                       || lower.contains(QStringLiteral("msvcp140"));
    const bool vc140 = lower.contains(QStringLiteral("microsoft.vc140.crt"))
                       || lower.contains(QStringLiteral("vc140"));
    const bool needsVc = vc143 || vc142 || vc140;
    if (!needsVc)
        return;

    const bool amd64 = lower.contains(QStringLiteral("processorarchitecture=\"amd64\""))
                       || lower.contains(QStringLiteral("processorarchitecture=\"*\""));
    const bool x86 = lower.contains(QStringLiteral("processorarchitecture=\"x86\""))
                     || lower.contains(QStringLiteral("processorarchitecture=\"*\""));

    if (amd64 || (!x86 && !amd64))
        out->needsVc2015x64 = true;
    if (x86)
        out->needsVc2015x86 = true;
}

void parsePeImportHints(const QByteArray& peData, ManifestRuntimeNeeds* out)
{
    if (!out || peData.isEmpty())
        return;
    const QByteArray lower = peData.toLower();
    const bool needsVc = lower.contains("vcruntime140") || lower.contains("msvcp140")
                         || lower.contains("concrt140") || lower.contains("vccorlib140");
    if (!needsVc)
        return;
    // Default to x64 for modern Steam Windows builds; also mark x86 if wow64 hints exist.
    out->needsVc2015x64 = true;
    if (lower.contains("syswow64") || lower.contains("wow64"))
        out->needsVc2015x86 = true;
}

} // namespace

ManifestRuntimeNeeds probeExecutableManifest(const QString& executablePath)
{
    ManifestRuntimeNeeds needs;
    QFile file(executablePath);
    if (!file.open(QIODevice::ReadOnly))
        return needs;

    const QByteArray header = file.read(2 * 1024 * 1024);
    parseManifestText(extractEmbeddedManifest(header), &needs);
    if (!needs.needsVc2015x64 && !needs.needsVc2015x86)
        parsePeImportHints(header, &needs);
    return needs;
}

QVector<RuntimeDepotRef> depotsFromManifestNeeds(const ManifestRuntimeNeeds& needs)
{
    QVector<RuntimeDepotRef> depots;
    auto append = [&](const char* depotId) {
        RuntimeDepotRef ref;
        ref.depotId = QString::fromLatin1(depotId);
        ref.label = RuntimeDepotCatalog::labelForDepotId(ref.depotId);
        ref.osList = QStringLiteral("windows");
        depots.append(ref);
    };

    if (needs.needsVc2015x64)
        append("228986");
    if (needs.needsVc2015x86)
        append("228985");
    return depots;
}

} // namespace arachnel::core
