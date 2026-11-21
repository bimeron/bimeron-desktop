#include "DesktopHandoffDirective.h"
#include "TrustedCore.h"
#include "../packaging/windows/msi-handoff-lab-process.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUuid>

namespace {
QByteArray readDirectiveFile(const QString &path) {
    QFile file(path);
    if (!TrustedCore::protectedExecutable(path) || !file.open(QIODevice::ReadOnly)
        || file.size() > 64 * 1024) return {};
    return file.readAll();
}
}

bool beginDesktopHandoffLab(bool tray) {
#if defined(_M_ARM64)
    const QString architecture = "arm64";
#elif defined(_M_X64)
    const QString architecture = "amd64";
#else
    return false;
#endif
    const QString path = QStringLiteral(BIMERON_MSI_HANDOFF_LAB_DIRECTIVE);
    QList<QByteArray> keys;
    if (!CoreUpdate::parseTrustedKeys(BIMERON_MSI_HANDOFF_LAB_DIRECTIVE_KEY, &keys)) return false;
    const auto directive = DesktopHandoffDirective::verify(
        readDirectiveFile(path), readDirectiveFile(path + ".sig"), keys, architecture);
    const QString helper = directive.value("helper_path").toString();
    const QString gui = QCoreApplication::applicationFilePath();
    if (directive.isEmpty() || !TrustedCore::protectedExecutable(helper) || !TrustedCore::protectedExecutable(gui)
        || QFileInfo(helper).fileName().compare("bimeron-msi-handoff-lab.exe", Qt::CaseInsensitive) != 0
        || QDir::cleanPath(QDir::fromNativeSeparators(helper)).startsWith(QFileInfo(gui).absolutePath() + '/', Qt::CaseInsensitive))
        return false;
    const auto exe = QDir::toNativeSeparators(helper).toStdWString();
    const auto helperPin = InstallerPreflight::openPinnedFile(exe, directive.value("helper_sha256").toString().toStdString());
    if (!helperPin || !InstallerPreflight::matchesPinnedDigest(QDir::toNativeSeparators(gui).toStdWString(),
                                                                 directive.value("old_gui_sha256").toString().toStdString()))
        return false;
    HANDLE raw = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &raw,
                         SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, TRUE, 0)) return false;
    const InstallerPreflight::FileHandle origin(raw);
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    const QString name = QStringLiteral("BimeronDesktopHandoffLab-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!server.listen(name)) return false;
    HANDLE inherited = origin.get();
    const auto command = QStringLiteral("\"%1\" --from-desktop-%2 %3 %4").arg(
        QString::fromStdWString(exe), tray ? QStringLiteral("tray") : QStringLiteral("window"),
        QString::number(quintptr(origin.get())), name).toStdWString();
    const auto child = MsiHandoff::startLabProcess(exe, command, &inherited, 1);
    if (!child || !server.waitForNewConnection(10000)) return false;
    const std::unique_ptr<QLocalSocket> socket(server.nextPendingConnection());
    server.close();
    ULONG sender = 0;
    if (!socket || !GetNamedPipeClientProcessId(reinterpret_cast<HANDLE>(socket->socketDescriptor()), &sender)
        || sender != GetProcessId(child.get()) || WaitForSingleObject(child.get(), 0) != WAIT_TIMEOUT
        || (socket->bytesAvailable() == 0 && !socket->waitForReadyRead(10000))
        || socket->read(1) != "R")
        return false;
    return socket->write("C", 1) == 1 && (socket->bytesToWrite() == 0 || socket->waitForBytesWritten(1000));
}
