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
    const bool vc142 = lower.contains(QStringLiteral("microsoft.vc142.crt"))
                       || lower.contains(QStringLiteral("vc142"))
                       || lower.contains(QStringLiteral("vcruntime140"));
    const bool vc140 = lower.contains(QStringLiteral("microsoft.vc140.crt"))
                       || lower.contains(QStringLiteral("vc140"));
    const bool needsVc = vc142 || vc140;
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

} // namespace

ManifestRuntimeNeeds probeExecutableManifest(const QString& executablePath)
{
    ManifestRuntimeNeeds needs;
    QFile file(executablePath);
    if (!file.open(QIODevice::ReadOnly))
        return needs;

    const QByteArray header = file.read(1024 * 1024);
    parseManifestText(extractEmbeddedManifest(header), &needs);
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
