#pragma once

// Fail fast when QT_QPA_PLATFORM names a platform plugin that Qt cannot find.
//
// On Windows a release Qt build reports a missing platform plugin with a modal
// "no Qt platform plugin could be initialized" message box when the process has
// no console window, before aborting. A headless test run then blocks forever
// without output. windeployqt copies only qwindows.dll beside the executables,
// so QT_QPA_PLATFORM=offscreen needs QT_QPA_PLATFORM_PLUGIN_PATH pointing to the
// target Qt plugins/platforms directory.
//
// This check mirrors Qt's platform plugin search directories and the stock
// q<key>.dll file naming. Call it before constructing QApplication.

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QStringList>
#include <qt_windows.h>
#include <cstdio>
#include <iterator>
#endif

namespace QtPlatformPreflight {

inline bool requestedPlatformAvailable() {
#ifdef Q_OS_WIN
    const QString requested = qEnvironmentVariable("QT_QPA_PLATFORM");
    if (requested.trimmed().isEmpty()) return true;

    QStringList directories;
    const QString pluginPath = qEnvironmentVariable("QT_QPA_PLATFORM_PLUGIN_PATH");
    if (!pluginPath.isEmpty()) directories += pluginPath.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    wchar_t module[MAX_PATH * 4] = {};
    const DWORD length = GetModuleFileNameW(nullptr, module, DWORD(std::size(module)));
    if (length > 0 && length < std::size(module))
        directories << QFileInfo(QString::fromWCharArray(module, int(length))).absolutePath() + "/platforms";
    for (const QString &root : qEnvironmentVariable("QT_PLUGIN_PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts))
        directories << root + "/platforms";
    directories << QLibraryInfo::path(QLibraryInfo::PluginsPath) + "/platforms";

    // Qt 6 accepts a ';'-separated fallback list; each entry may carry ':'-separated options.
    for (const QString &entry : requested.split(';', Qt::SkipEmptyParts)) {
        const QString key = entry.section(':', 0, 0).trimmed();
        if (key.isEmpty()) continue;
        for (const QString &directory : directories) {
            const QDir dir(directory);
            if (dir.exists("q" + key + ".dll") || dir.exists("q" + key + "d.dll")) return true;
        }
    }
    std::fprintf(stderr,
        "Qt platform plugin requested by QT_QPA_PLATFORM=%s was not found in: %s\n"
        "The deployed build tree contains only qwindows.dll. Set QT_QPA_PLATFORM_PLUGIN_PATH "
        "to the target Qt plugins/platforms directory.\n",
        qPrintable(requested), qPrintable(directories.join(QDir::listSeparator())));
    return false;
#else
    return true;
#endif
}

} // namespace QtPlatformPreflight
