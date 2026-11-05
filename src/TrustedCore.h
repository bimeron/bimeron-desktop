#pragma once

#include <QString>
#include <QStringList>

namespace TrustedCore {
bool protectedDirectory(const QString &path);
bool protectedExecutable(const QString &path);
// Returns only an independently installed, protected repair broker. An empty
// result is a missing trust root, never permission to elevate a downloaded file.
QString repairExecutable();
bool supportsSourceUpdate(const QString &path);
QStringList updateArguments(const QString &source, const QString &version, const QString &sha256);
QString windowsCommandLine(const QStringList &arguments);
QString failureMessage(int exitCode, const QString &details);
}
