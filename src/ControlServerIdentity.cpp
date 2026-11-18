#include "ControlServerIdentity.h"

#include <QFile>
#include <vector>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <aclapi.h>
#include <sddl.h>
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace ControlServerIdentity {

#ifdef Q_OS_WIN
namespace {
QString sidText(PSID sid) {
    LPWSTR text = nullptr;
    if (!sid || !ConvertSidToStringSidW(sid, &text)) return {};
    const QString result = QString::fromWCharArray(text);
    LocalFree(text);
    return result;
}

bool tokenIdentity(HANDLE process, QString *user, bool *elevated) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return false;
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> storage(size);
    TOKEN_ELEVATION elevation{};
    DWORD returned = 0;
    const bool ok = size && GetTokenInformation(token, TokenUser, storage.data(), size, &size)
        && GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned);
    if (ok) {
        *user = sidText(reinterpret_cast<TOKEN_USER *>(storage.data())->User.Sid);
        *elevated = elevation.TokenIsElevated != 0;
    }
    CloseHandle(token);
    return ok && !user->isEmpty();
}

// Queried after the pipe is open, so a service that stopped meanwhile is seen.
void queryService(PipeObservation *result) {
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager) return;
    SC_HANDLE service = OpenServiceW(manager, reinterpret_cast<LPCWSTR>(serviceName.utf16()), SERVICE_QUERY_STATUS);
    SERVICE_STATUS_PROCESS status{};
    DWORD size = 0;
    result->serviceQueried = service && QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
        reinterpret_cast<LPBYTE>(&status), sizeof(status), &size);
    if (result->serviceQueried) {
        result->serviceState = status.dwCurrentState;
        result->serviceProcess = status.dwProcessId;
    }
    if (service) CloseServiceHandle(service);
    CloseServiceHandle(manager);
}
} // namespace

QString currentUserSid() {
    QString user;
    bool elevated = false;
    return tokenIdentity(GetCurrentProcess(), &user, &elevated) ? user : QString();
}

QString processUserSid(quintptr process) {
    QString user;
    bool elevated = false;
    return tokenIdentity(reinterpret_cast<HANDLE>(process), &user, &elevated) ? user : QString();
}

PipeObservation observePipe(quintptr pipe, Endpoint endpoint) {
    PipeObservation result;
    const HANDLE handle = reinterpret_cast<HANDLE>(pipe);
    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetSecurityInfo(handle, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION,
                        &owner, nullptr, nullptr, nullptr, &descriptor) == ERROR_SUCCESS) {
        result.owner = sidText(owner);
        LocalFree(descriptor);
    }
    ULONG server = 0;
    if (GetNamedPipeServerProcessId(handle, &server)) result.serverProcess = server;
    if (endpoint == Endpoint::Service) {
        // Never open the server token: a standard user cannot open LocalSystem's.
        queryService(&result);
    } else if (endpoint == Endpoint::Console && result.owner == consoleOwner && result.serverProcess) {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, result.serverProcess);
        if (process) {
            tokenIdentity(process, &result.serverUser, &result.serverElevated);
            CloseHandle(process);
        }
    }
    return result;
}
#else
PathObservation observePath(const QString &directory, const QString &socket) {
    PathObservation result;
    struct stat info{};
    if (::lstat(QFile::encodeName(directory).constData(), &info) != 0) return result;
    result.directoryPresent = true;
    result.directoryIsDirectory = S_ISDIR(info.st_mode); // lstat: a symlink is never a directory.
    result.directoryOwner = info.st_uid;
    result.directoryMode = info.st_mode & 07777;
    if (::lstat(QFile::encodeName(socket).constData(), &info) != 0) return result;
    result.socketPresent = true;
    result.socketIsSocket = S_ISSOCK(info.st_mode);
    return result;
}

bool peerCredentials(int descriptor, quint32 *uid, qint64 *pid) {
#ifdef SO_PEERCRED
    struct ucred credentials{};
    socklen_t size = sizeof(credentials);
    if (getsockopt(descriptor, SOL_SOCKET, SO_PEERCRED, &credentials, &size) != 0 || size != sizeof(credentials))
        return false;
    *uid = credentials.uid;
    *pid = credentials.pid;
    return true;
#else
    Q_UNUSED(descriptor); Q_UNUSED(uid); Q_UNUSED(pid);
    return false;
#endif
}
#endif

} // namespace ControlServerIdentity
