#pragma once

// Server identity checks for local Core control endpoints. See Core
// docs/CONTROL.MD "Server identity" and docs/control-server-identity.md.
// Decisions are pure functions of OS observations so they can be unit tested.

#include <QString>

namespace ControlServerIdentity {

inline const QString serviceOwner = QStringLiteral("S-1-5-18");      // SYSTEM
inline const QString consoleOwner = QStringLiteral("S-1-5-32-544");  // Administrators
inline const QString serviceName = QStringLiteral("BimeronCore");
inline const QString windowsServicePipe = QStringLiteral("Bimeron.Core.Service");
inline const QString unixRuntimeDirectory = QStringLiteral("/run/bimeron");
inline const QString unixControlSocket = QStringLiteral("/run/bimeron/control.sock");

enum class Endpoint {
    Unknown,  // Never connected in production builds.
    Fixture,  // Test builds only: bound by the optional expected process ID.
    Service,  // Windows service pipe or the Unix control socket.
    Console,  // Windows console daemon pipe of the current user.
};

inline bool lowerHex(const QString &text, int length) {
    if (text.size() != length) return false;
    for (const QChar c : text)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

// Production policy. Test builds map Unknown to Fixture at the call site.
inline Endpoint classifyWindows(const QString &name) {
    if (name == windowsServicePipe) return Endpoint::Service;
    const QString prefix = QStringLiteral("Bimeron.Core.");
    if (name.startsWith(prefix) && lowerHex(name.mid(prefix.size()), 32)) return Endpoint::Console;
    return Endpoint::Unknown;
}
inline Endpoint classifyUnix(const QString &path) {
    return path == unixControlSocket ? Endpoint::Service : Endpoint::Unknown;
}

// What a Windows client observed on its connected pipe handle. Empty strings
// and zero IDs mean the observation failed.
//
// The owner applies to the whole pipe, and GetNamedPipeServerProcessId reports
// the process that created the pipe, also for a later instance that another
// process serves. Neither identifies the server of one instance. Extra
// instances are prevented by Core's pipe DACLs, which grant clients 0x12019B
// without FILE_CREATE_PIPE_INSTANCE, WRITE_DAC or WRITE_OWNER. The checks here
// are the client-side layer of the Core "Server identity" contract.
struct PipeObservation {
    QString owner;
    quint32 serverProcess = 0;
    // Service pipe only: QueryServiceStatusEx result. serviceState is
    // dwCurrentState (SERVICE_RUNNING is 4); serviceProcess is dwProcessId.
    bool serviceQueried = false;
    quint32 serviceState = 0;
    quint32 serviceProcess = 0;
    // Console daemon only. serverRunning: the retained process handle still
    // showed the process running after the token inspection.
    QString serverUser;
    bool serverElevated = false;
    bool serverRunning = false;
};

inline constexpr quint32 serviceRunning = 4; // SERVICE_RUNNING

// Every result except Trusted is control_server_untrusted. NotRunning means
// SCM does not report a running service process (not installed, stopped,
// pending, process ID 0 or a failed query) while the pipe exists. The pipe
// can then outlive the service, for example an old service's pipe with an
// instance held by another process. The PID cross-check while the service
// runs is an extra layer; it cannot detect an extra instance. NotRunning is
// presented as a service that is not running.
enum class ServiceTrust { Trusted, NotRunning, Untrusted };

inline ServiceTrust serviceTrust(const PipeObservation &pipe, quint32 expectedProcess, QString *diagnostic = nullptr) {
    const auto result = [diagnostic](ServiceTrust trust, const QString &reason) {
        if (diagnostic) *diagnostic = reason;
        return trust;
    };
    if (pipe.owner != serviceOwner)
        return result(ServiceTrust::Untrusted, QStringLiteral("service pipe owner %1, expected %2")
            .arg(pipe.owner.isEmpty() ? QStringLiteral("unreadable") : pipe.owner, serviceOwner));
    if (!pipe.serverProcess) return result(ServiceTrust::Untrusted, QStringLiteral("service pipe server process unavailable"));
    if (!pipe.serviceQueried)
        return result(ServiceTrust::NotRunning, QStringLiteral("SCM status of %1 unavailable").arg(serviceName));
    if (pipe.serviceState != serviceRunning)
        return result(ServiceTrust::NotRunning, QStringLiteral("SCM reports %1 state %2, not running")
            .arg(serviceName).arg(pipe.serviceState));
    if (!pipe.serviceProcess)
        return result(ServiceTrust::NotRunning, QStringLiteral("SCM reports no %1 process").arg(serviceName));
    if (pipe.serverProcess != pipe.serviceProcess)
        return result(ServiceTrust::Untrusted, QStringLiteral("service pipe server process %1, SCM reports %2")
            .arg(pipe.serverProcess).arg(pipe.serviceProcess));
    if (expectedProcess && pipe.serverProcess != expectedProcess)
        return result(ServiceTrust::Untrusted, QStringLiteral("service pipe server process %1, expected %2")
            .arg(pipe.serverProcess).arg(expectedProcess));
    return result(ServiceTrust::Trusted, {});
}

// Empty when trusted; otherwise an English diagnostic for the log.
inline QString serviceMismatch(const PipeObservation &pipe, quint32 expectedProcess) {
    QString diagnostic;
    serviceTrust(pipe, expectedProcess, &diagnostic);
    return diagnostic;
}

inline QString consoleMismatch(const PipeObservation &pipe, const QString &clientUser, quint32 expectedProcess) {
    if (pipe.owner != consoleOwner)
        return QStringLiteral("daemon pipe owner %1, expected %2")
            .arg(pipe.owner.isEmpty() ? QStringLiteral("unreadable") : pipe.owner, consoleOwner);
    if (!pipe.serverProcess) return QStringLiteral("daemon pipe server process unavailable");
    if (expectedProcess && pipe.serverProcess != expectedProcess)
        return QStringLiteral("daemon pipe server process %1, launched %2").arg(pipe.serverProcess).arg(expectedProcess);
    if (clientUser.isEmpty() || pipe.serverUser != clientUser)
        return QStringLiteral("daemon token user %1 differs from the current user")
            .arg(pipe.serverUser.isEmpty() ? QStringLiteral("unreadable") : pipe.serverUser);
    if (!pipe.serverElevated) return QStringLiteral("daemon token is not elevated");
    if (!pipe.serverRunning) return QStringLiteral("daemon process exited during inspection");
    return {};
}

// A service built before explicit pipe owners leaves Administrators as owner.
inline bool legacyServiceOwner(const PipeObservation &pipe) { return pipe.owner == consoleOwner; }

// lstat results for a Unix runtime directory and its socket.
struct PathObservation {
    bool directoryPresent = false, directoryIsDirectory = false;
    quint32 directoryOwner = 0, directoryMode = 0; // st_uid, st_mode permission bits
    bool socketPresent = false, socketIsSocket = false;
};
enum class PathState { Trusted, Missing, Untrusted };

inline PathState pathState(const PathObservation &path, QString *diagnostic = nullptr) {
    const auto reject = [diagnostic](const QString &reason) {
        if (diagnostic) *diagnostic = reason;
        return PathState::Untrusted;
    };
    if (!path.directoryPresent) return PathState::Missing;
    if (!path.directoryIsDirectory) return reject(QStringLiteral("runtime directory is not a directory"));
    if (path.directoryOwner != 0)
        return reject(QStringLiteral("runtime directory owner UID %1, expected 0").arg(path.directoryOwner));
    if (path.directoryMode & 0022)
        return reject(QStringLiteral("runtime directory mode %1 is group or other writable")
                          .arg(path.directoryMode & 07777, 4, 8, QChar('0')));
    if (!path.socketPresent) return PathState::Missing;
    if (!path.socketIsSocket) return reject(QStringLiteral("control path is not a socket"));
    return PathState::Trusted;
}

// SO_PEERCRED: the kernel records the listener credentials.
inline QString peerMismatch(bool readable, quint32 uid, qint64 pid, qint64 expectedProcess) {
    if (!readable) return QStringLiteral("peer credentials unavailable");
    if (uid != 0) return QStringLiteral("peer UID %1, expected 0").arg(uid);
    if (expectedProcess && pid != expectedProcess)
        return QStringLiteral("peer process %1, expected %2").arg(pid).arg(expectedProcess);
    return {};
}

// OS observations. Windows functions take a pipe HANDLE as quintptr.
#ifdef Q_OS_WIN
QString currentUserSid();
QString processUserSid(quintptr process);
// For the console daemon, the server process is opened once with
// PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE. When retainedProcess is
// given, that handle is returned for the caller to keep until the connection
// closes, so the process ID cannot be reused meanwhile; otherwise it is closed.
PipeObservation observePipe(quintptr pipe, Endpoint endpoint, quintptr *retainedProcess = nullptr);
#else
PathObservation observePath(const QString &directory, const QString &socket);
bool peerCredentials(int descriptor, quint32 *uid, qint64 *pid);
#endif

} // namespace ControlServerIdentity
