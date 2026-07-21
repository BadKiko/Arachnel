#pragma once

#include <QString>
#include <QStringList>

namespace arachnel::core {

struct WindowsRunEnv {
    QString protonExecutable;
    QString compatDataPath;
    QString steamCompatClientPath;

    bool useProton() const { return !protonExecutable.isEmpty(); }
};

bool runWindowsProgramAndWait(const QString& program, const QStringList& arguments, int timeoutMs,
                              QString* errorOut, const QString& workingDirectory = {},
                              const WindowsRunEnv& env = {});

void fillProtonInstallFields(const QString& entryId, const QString& preferredProtonId,
                             QString* protonExecutable, QString* compatDataPath,
                             QString* steamCompatClientPath);

} // namespace arachnel::core
