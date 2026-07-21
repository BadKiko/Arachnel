#pragma once

#include <QObject>

namespace arachnel::core {

Q_NAMESPACE

enum class InstallKind {
    PortableArchive = 0,
    Installer,
    BundledFix,
    FixDownload,
};
Q_ENUM_NS(InstallKind)

QString installKindLabel(InstallKind kind);

} // namespace arachnel::core
