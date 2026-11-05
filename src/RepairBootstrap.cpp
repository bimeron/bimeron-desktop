#include "RepairBootstrap.h"
#include "RepairBootstrap_p.h"
#include "TrustedCore.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
extern char **environ;
#endif
namespace RepairBootstrap {
bool available() { return QFile::exists(":/repair-bootstrap/broker"); }
int run(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 4 || args[1] != "repair-session" ||
        !QRegularExpression("^[0-9a-f]{32}$").match(args[2]).hasMatch()) return 1;
    bool valid = false;
    const auto pid = args[3].toUInt(&valid);
    if (!valid || !pid || !available() || !TrustedCore::protectedExecutable(app.applicationFilePath())) return 1;
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    TOKEN_ELEVATION elevation{}; DWORD returned = 0;
    const bool elevated = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)
        && GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned)
        && elevation.TokenIsElevated;
    if (token) CloseHandle(token);
    if (!elevated) return 1;
#else
    if (geteuid() != 0) return 1;
    umask(0022);
#endif
    QFile embedded(":/repair-bootstrap/broker");
    if (!embedded.open(QIODevice::ReadOnly)) return 1;
    const QByteArray bytes = embedded.readAll();
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
#ifdef Q_OS_WIN
    const QString directory = app.applicationDirPath() + "/repair-bootstrap-" + digest;
    const QString broker = directory + "/bimeron-repair.exe";
#else
    const QString directory = "/usr/libexec/bimeron/repair-bootstrap-" + digest;
    const QString broker = directory + "/bimeron-repair";
#endif
    // Validate each existing ancestor before creating its child. Never traverse a
    // writable or redirected directory with elevated filesystem operations.
    QStringList missing;
    QString ancestor = directory;
    while (!QFileInfo::exists(ancestor)) {
        missing.prepend(ancestor);
        const QString next = QFileInfo(ancestor).dir().absolutePath();
        if (next == ancestor) return 1;
        ancestor = next;
    }
    if (!TrustedCore::protectedDirectory(ancestor)) return 1;
    for (const QString &part : missing)
        if (!QDir().mkdir(part) || !TrustedCore::protectedDirectory(part)) return 1;
    if (!TrustedCore::protectedDirectory(directory)) return 1;
    auto provision = [&](const QString &target, const QByteArray &data) {
        QFile file(target);
        if (!file.exists()) {
            QTemporaryFile temporary(directory + "/.repair-XXXXXX");
            if (!temporary.open() || temporary.write(data) != data.size() || !temporary.flush()) return false;
#ifndef Q_OS_WIN
            if (::chmod(QFile::encodeName(temporary.fileName()).constData(), 0755) != 0) return false;
#endif
            temporary.close();
            if (!TrustedCore::protectedExecutable(temporary.fileName()) || !temporary.rename(target)) return false;
            temporary.setAutoRemove(false);
        }
        if (!TrustedCore::protectedExecutable(target) || !file.open(QIODevice::ReadOnly)
            || file.size() != data.size()) return false;
        QCryptographicHash hash(QCryptographicHash::Sha256);
        return hash.addData(&file) && hash.result() == QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    };
    if (!provision(broker, bytes)) return 1;
#ifdef Q_OS_WIN
    QFile dll(":/repair-bootstrap/wintun.dll");
    if (!dll.open(QIODevice::ReadOnly) || !provision(directory + "/wintun.dll", dll.readAll())) return 1;
    QFile license(":/repair-bootstrap/WINTUN-LICENSE.txt");
    if (!license.open(QIODevice::ReadOnly) || !provision(directory + "/WINTUN-LICENSE.txt", license.readAll())) return 1;
    QProcess child;
    child.setProgram(broker);
    child.setArguments(args.mid(1));
    child.setWorkingDirectory(directory);
    child.start();
    if (!child.waitForStarted(5000)) return 1;
    // Keep the elevated parent alive, retaining the child's OS process handle.
    child.waitForFinished(-1);
    return child.exitStatus() == QProcess::NormalExit ? child.exitCode() : 1;
#else
    const QByteArray executable = QFile::encodeName(broker), nonce = args[2].toLatin1(), caller = args[3].toLatin1();
    const int fd = ::open(executable.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return 1;
    struct stat identity{};
    QFile verified;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const bool validImage = ::fstat(fd, &identity) == 0 && S_ISREG(identity.st_mode)
        && identity.st_uid == 0 && !(identity.st_mode & (S_IWGRP | S_IWOTH))
        && identity.st_size == bytes.size() && verified.open(fd, QIODevice::ReadOnly, QFileDevice::DontCloseHandle)
        && hash.addData(&verified) && hash.result() == QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    if (validImage) {
        char *arguments[] = {const_cast<char *>(executable.constData()), const_cast<char *>("repair-session"),
            const_cast<char *>(nonce.constData()), const_cast<char *>(caller.constData()), nullptr};
        // Execute the verified inode, never resolve the pathname again.
        ::fexecve(fd, arguments, environ);
    }
    verified.close();
    ::close(fd);
    return 1;
#endif
}
quintptr authenticatedChild(quintptr parent, unsigned long pid) {
    return detail::authenticatedChild(parent, pid, ":/repair-bootstrap/broker", TrustedCore::protectedExecutable);
}
void bindOwnedChild(QObject *owner, quintptr child) {
#ifdef Q_OS_WIN
    if (child) QObject::connect(owner, &QObject::destroyed, [child] { CloseHandle(reinterpret_cast<HANDLE>(child)); });
#else
    Q_UNUSED(owner); Q_UNUSED(child);
#endif
}
quintptr detail::authenticatedChild(quintptr parent, unsigned long pid, const QString &payloadPath,
    const std::function<bool(const QString &)> &trusted) {
#ifdef Q_OS_WIN
    HANDLE owner = reinterpret_cast<HANDLE>(parent);
    if (!owner || WaitForSingleObject(owner, 0) != WAIT_TIMEOUT) return false;
    HANDLE child = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (!child) return false;
    FILETIME pc{}, pe{}, pk{}, pu{}, cc{}, ce{}, ck{}, cu{};
    bool valid = GetProcessTimes(owner, &pc, &pe, &pk, &pu) && GetProcessTimes(child, &cc, &ce, &ck, &cu)
        && CompareFileTime(&pc, &cc) <= 0 && WaitForSingleObject(child, 0) == WAIT_TIMEOUT;
    wchar_t path[32768]; DWORD length = 32768;
    valid = valid && QueryFullProcessImageNameW(child, 0, path, &length);
    const QString image = valid ? QString::fromWCharArray(path, int(length)) : QString();
    valid = valid && trusted(image);
    length = 32768;
    valid = valid && QueryFullProcessImageNameW(owner, 0, path, &length);
    const QString parentImage = valid ? QString::fromWCharArray(path, int(length)) : QString();
    valid = valid && QDir::cleanPath(parentImage).compare(QDir::cleanPath(QCoreApplication::applicationFilePath()), Qt::CaseInsensitive) == 0;
    QFile payload(payloadPath), actual(image);
    valid = valid && payload.open(QIODevice::ReadOnly) && actual.open(QIODevice::ReadOnly)
        && payload.size() == actual.size();
    QCryptographicHash expectedHash(QCryptographicHash::Sha256), actualHash(QCryptographicHash::Sha256);
    valid = valid && expectedHash.addData(&payload) && actualHash.addData(&actual) && expectedHash.result() == actualHash.result();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    bool related = false;
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) do {
            if (entry.th32ProcessID == pid) { related = entry.th32ParentProcessID == GetProcessId(owner); break; }
        } while (Process32NextW(snapshot, &entry));
        CloseHandle(snapshot);
    }
    if (valid && related && WaitForSingleObject(child, 0) == WAIT_TIMEOUT)
        return reinterpret_cast<quintptr>(child);
    CloseHandle(child);
    return 0;
#else
    Q_UNUSED(parent); Q_UNUSED(pid); Q_UNUSED(payloadPath); Q_UNUSED(trusted); return false;
#endif
}
}
