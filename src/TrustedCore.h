#pragma once

#include <QString>
#include <QStringList>

namespace TrustedCore {
bool protectedExecutable(const QString &path);
bool supportsSourceUpdate(const QString &path);
QStringList updateArguments(const QString &source, const QString &version, const QString &sha256);
QString windowsCommandLine(const QStringList &arguments);
QString failureMessage(int exitCode, const QString &details);
}
