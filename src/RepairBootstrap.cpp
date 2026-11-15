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
#include <QDebug>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
extern char **environ;
#endif
namespace RepairBootstrap {
QString detail::packageDirectory(const QString &gui) {
    if (!QFileInfo(gui).isAbsolute()) return {};
#ifdef Q_OS_WIN
    return QFileInfo(gui).absolutePath() + "/repair";
#else
    // DEB installs the GUI in /usr/bin. Do not infer paths for other layouts.
    if (QFileInfo(gui).absolutePath() != "/usr/bin") return {};
    return QDir::cleanPath(QFileInfo(gui).absolutePath() + "/../libexec/bimeron/repair");
#endif
}

bool detail::provision(const QString &target, const QByteArray &data,
    const std::function<bool(const QString &)> &trustedDirectory,
    const std::function<bool(const QString &)> &trustedFile, QString *error) {
    const auto fail = [&] {
        if (error) *error = "Repair payload refused (unsafe path, differing bytes or I/O failure): " + target;
        return false;
    };
    if (!QFileInfo(target).isAbsolute()) return fail();
    const QString directory = QFileInfo(target).absolutePath();
    QStringList missing;
    QString ancestor = directory;
    while (!QFileInfo::exists(ancestor)) {
        if (QFileInfo(ancestor).isSymLink()) return fail();
        missing.prepend(ancestor);
        const QString next = QFileInfo(ancestor).dir().absolutePath();
        if (next == ancestor) return fail();
        ancestor = next;
    }
    if (!trustedDirectory(ancestor)) return fail();
    for (const QString &part : missing)
        if (!QDir().mkdir(part) || !trustedDirectory(part)) return fail();
    if (!trustedDirectory(directory)) return fail();
    const auto matches = [&](const QString &path) {
        if (!trustedFile(path)) return false;
        QFile file;
#ifndef Q_OS_WIN
        // Bind hashing to the checked regular inode, never follow a leaf link.
        struct stat before{}, opened{};
        const auto native = QFile::encodeName(path);
        if (::lstat(native.constData(), &before) != 0) return false;
        const int fd = ::open(native.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        if (fd < 0) return false;
        const bool safe = ::fstat(fd, &opened) == 0 && S_ISREG(opened.st_mode)
            && before.st_dev == opened.st_dev && before.st_ino == opened.st_ino
            && before.st_uid == opened.st_uid && before.st_mode == opened.st_mode;
        if (!safe || !file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
            ::close(fd); return false;
        }
#else
        file.setFileName(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
#endif
        QCryptographicHash hash(QCryptographicHash::Sha256);
        return file.size() == data.size() && hash.addData(&file)
            && hash.result() == QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    };
    if (QFileInfo::exists(target) || QFileInfo(target).isSymLink())
        return matches(target) || fail();
    QTemporaryFile temporary(directory + "/.repair-XXXXXX");
    if (!temporary.open() || temporary.write(data) != data.size() || !temporary.flush()) return fail();
#ifndef Q_OS_WIN
    if (::fchmod(temporary.handle(), 0755) != 0 || ::fsync(temporary.handle()) != 0) return fail();
#endif
    const QString temporaryPath = temporary.fileName();
    temporary.close();
    if (!matches(temporaryPath) || !trustedDirectory(directory)) return fail();
#ifdef Q_OS_WIN
    const bool published = MoveFileExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(temporaryPath).utf16()),
        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(target).utf16()), MOVEFILE_WRITE_THROUGH);
#else
    // Atomic rename without replacement, even if another bootstrap won the race.
    const bool published = ::syscall(SYS_renameat2, AT_FDCWD, QFile::encodeName(temporaryPath).constData(),
        AT_FDCWD, QFile::encodeName(target).constData(), 1 /* RENAME_NOREPLACE */) == 0;
#endif
    if (published) temporary.setAutoRemove(false);
    return matches(target) || fail();
}

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
    const auto provision = [](const QString &target, const QByteArray &data) {
        QString error;
        if (detail::provision(target, data, TrustedCore::protectedDirectory,
                              TrustedCore::protectedExecutable, &error)) return true;
        qCritical().noquote() << error;
        return false;
    };
    const QString packageDirectory = detail::packageDirectory(app.applicationFilePath());
    if (packageDirectory.isEmpty()) return 1;
    // Repair package-owned missing files during this same elevated session.
    if (!provision(packageDirectory + "/" + QFileInfo(broker).fileName(), bytes)
        || !provision(broker, bytes)) return 1;
#ifdef Q_OS_WIN
    for (const QString &name : {QString("wintun.dll"), QString("WINTUN-LICENSE.txt")}) {
        QFile resource(":/repair-bootstrap/" + name);
        if (!resource.open(QIODevice::ReadOnly)) return 1;
        const auto data = resource.readAll();
        if (!provision(packageDirectory + "/" + name, data)
            || !provision(directory + "/" + name, data)) return 1;
    }
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
