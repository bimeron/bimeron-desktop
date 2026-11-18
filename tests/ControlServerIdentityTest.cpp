#include "ControlServerIdentity.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUuid>

#include <cstdio>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

using namespace ControlServerIdentity;

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

const QString user = QStringLiteral("S-1-5-21-1000-2000-3000-1001");
const QString otherUser = QStringLiteral("S-1-5-21-1000-2000-3000-1002");

void classification() {
    expect(classifyWindows("Bimeron.Core.Service") == Endpoint::Service, "service pipe");
    expect(classifyWindows("Bimeron.Core.0123456789abcdef0123456789abcdef") == Endpoint::Console, "console pipe");
    expect(classifyWindows("Bimeron.Core.0123456789ABCDEF0123456789ABCDEF") == Endpoint::Unknown, "uppercase hash is not a Core pipe");
    expect(classifyWindows("Bimeron.Core.0123456789abcdef") == Endpoint::Unknown, "short hash is not a Core pipe");
    expect(classifyWindows("Bimeron.Core.Readiness") == Endpoint::Unknown, "readiness pipe is never a control endpoint");
    expect(classifyWindows("Bimeron.Core.Service.extra") == Endpoint::Unknown, "service prefix is not enough");
    expect(classifyWindows("bimeron-test-fixture") == Endpoint::Unknown, "fixture names are unknown in production");
    expect(classifyUnix("/run/bimeron/control.sock") == Endpoint::Service, "Linux control socket");
    expect(classifyUnix("/var/run/bimeron/control.sock") == Endpoint::Unknown, "the /var/run alias is not canonical");
    expect(classifyUnix("/tmp/control.sock") == Endpoint::Unknown, "other sockets are unknown");
}

void service() {
    const PipeObservation trusted{serviceOwner, 812, 812, {}, false};
    expect(serviceMismatch(trusted, 0).isEmpty(), "SYSTEM-owned pipe served by the SCM process");
    expect(serviceMismatch({serviceOwner, 812, 0, {}, false}, 0).isEmpty(), "unavailable SCM process skips binding");
    expect(!serviceMismatch({consoleOwner, 812, 812, {}, false}, 0).isEmpty(), "Administrators owner (pre-#159 service) is rejected");
    expect(legacyServiceOwner({consoleOwner, 812, 812, {}, false}), "Administrators owner is reported as an outdated service");
    expect(!legacyServiceOwner({user, 812, 812, {}, false}), "a user-owned squatter is not an outdated service");
    expect(!serviceMismatch({user, 4242, 812, {}, false}, 0).isEmpty(), "user-owned squatted pipe is rejected");
    expect(!serviceMismatch({{}, 812, 812, {}, false}, 0).isEmpty(), "unreadable owner is rejected");
    expect(!serviceMismatch({serviceOwner, 0, 812, {}, false}, 0).isEmpty(), "unreadable server process is rejected");
    expect(!serviceMismatch({serviceOwner, 999, 812, {}, false}, 0).isEmpty(), "another SYSTEM process is rejected when SCM reports one");
    expect(!serviceMismatch(trusted, 900).isEmpty(), "caller-expected process must match");
    expect(serviceMismatch(trusted, 812).isEmpty(), "caller-expected process matches");
    expect(serviceMismatch({consoleOwner, 812, 812, {}, false}, 0).contains(consoleOwner), "diagnostic names the observed owner");
}

void console() {
    const PipeObservation trusted{consoleOwner, 4000, 0, user, true};
    expect(consoleMismatch(trusted, user, 0).isEmpty(), "elevated same-user daemon");
    expect(consoleMismatch(trusted, user, 4000).isEmpty(), "launched daemon process matches");
    expect(!consoleMismatch(trusted, user, 4001).isEmpty(), "different daemon process than launched");
    expect(!consoleMismatch({user, 4000, 0, user, true}, user, 0).isEmpty(), "user-owned pipe is rejected");
    expect(!consoleMismatch({serviceOwner, 4000, 0, user, true}, user, 0).isEmpty(), "SYSTEM owner is not the console owner");
    expect(!consoleMismatch({consoleOwner, 4000, 0, otherUser, true}, user, 0).isEmpty(), "other user's elevated daemon");
    expect(!consoleMismatch({consoleOwner, 4000, 0, user, false}, user, 0).isEmpty(), "non-elevated daemon");
    expect(!consoleMismatch({consoleOwner, 4000, 0, {}, false}, user, 0).isEmpty(), "unreadable token");
    expect(!consoleMismatch(trusted, {}, 0).isEmpty(), "unknown client user");
    expect(!consoleMismatch({consoleOwner, 0, 0, user, true}, user, 0).isEmpty(), "unreadable server process");
}

void unixDecisions() {
    const PathObservation trusted{true, true, 0, 0711, true, true};
    expect(pathState(trusted) == PathState::Trusted, "root 0711 directory with a socket");
    expect(pathState({true, true, 0, 0755, true, true}) == PathState::Trusted, "root 0755 repair directory");
    expect(pathState({}) == PathState::Missing, "missing directory is an unavailable Core");
    expect(pathState({true, true, 0, 0711, false, false}) == PathState::Missing, "missing socket is an unavailable Core");
    expect(pathState({true, false, 0, 0777, true, true}) == PathState::Untrusted, "symlink or file instead of the directory");
    expect(pathState({true, true, 1000, 0711, true, true}) == PathState::Untrusted, "user-owned directory");
    expect(pathState({true, true, 0, 0731, true, true}) == PathState::Untrusted, "group-writable directory");
    expect(pathState({true, true, 0, 0713, true, true}) == PathState::Untrusted, "other-writable directory");
    expect(pathState({true, true, 0, 01777, true, true}) == PathState::Untrusted, "sticky world-writable directory");
    expect(pathState({true, true, 0, 0711, true, false}) == PathState::Untrusted, "regular file at the socket path");
    QString diagnostic;
    pathState({true, true, 0, 0733, true, true}, &diagnostic);
    expect(diagnostic.contains("0733"), "diagnostic names the mode");

    expect(peerMismatch(true, 0, 700, 0).isEmpty(), "root peer");
    expect(peerMismatch(true, 0, 700, 700).isEmpty(), "root peer with expected process");
    expect(!peerMismatch(true, 1000, 700, 0).isEmpty(), "non-root peer");
    expect(!peerMismatch(true, 0, 701, 700).isEmpty(), "root peer of another process");
    expect(!peerMismatch(false, 0, 700, 0).isEmpty(), "unreadable peer credentials");
}

#ifdef Q_OS_WIN
// Real handle observation of a pipe this test owns: never SYSTEM, so the
// service policy must reject it, and the client handle must allow READ_CONTROL.
void windowsObservation() {
    const std::wstring name = (QStringLiteral("\\\\.\\pipe\\Bimeron.Desktop.IdentityTest.")
        + QUuid::createUuid().toString(QUuid::Id128)).toStdWString();
    HANDLE server = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 4096, 4096, 0, nullptr);
    expect(server != INVALID_HANDLE_VALUE, "fixture pipe");
    HANDLE client = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    expect(client != INVALID_HANDLE_VALUE, "identification-only client handle");
    if (server == INVALID_HANDLE_VALUE || client == INVALID_HANDLE_VALUE) return;
    const PipeObservation observed = observePipe(reinterpret_cast<quintptr>(client), Endpoint::Service);
    expect(!observed.owner.isEmpty(), "READ_CONTROL on the client handle reads the owner");
    expect(observed.owner != serviceOwner, "a user process cannot create a SYSTEM-owned pipe");
    expect(observed.serverProcess == GetCurrentProcessId(), "server process ID");
    expect(!serviceMismatch(observed, 0).isEmpty(), "service policy rejects the fixture pipe");
    const QString self = currentUserSid();
    expect(self.startsWith("S-1-5-"), "current user SID");
    const PipeObservation console = observePipe(reinterpret_cast<quintptr>(client), Endpoint::Console);
    if (console.owner == consoleOwner) // Elevated administrators own new objects as Administrators.
        expect(console.serverUser == self, "console token user of an Administrators-owned pipe");
    else
        expect(!consoleMismatch(console, self, 0).isEmpty(), "console policy rejects a user-owned pipe");
    std::fprintf(stdout, "fixture pipe owner %s, user %s\n", qPrintable(observed.owner), qPrintable(self));
    CloseHandle(client);
    CloseHandle(server);
}
#else
void unixObservation() {
    QTemporaryDir directory;
    expect(directory.isValid(), "temporary directory");
    const QString socketPath = directory.filePath("control.sock");
    expect(pathState(observePath(directory.filePath("absent"), socketPath)) == PathState::Missing, "absent directory");
    expect(pathState(observePath(directory.path(), socketPath)) == (geteuid() == 0 ? PathState::Missing : PathState::Untrusted),
           "user-owned directory is rejected before the socket is inspected");
    const int listener = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const QByteArray encoded = QFile::encodeName(socketPath);
    std::snprintf(address.sun_path, sizeof(address.sun_path), "%s", encoded.constData());
    expect(listener >= 0 && ::bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0
           && ::listen(listener, 1) == 0, "fixture socket");
    const PathObservation observed = observePath(directory.path(), socketPath);
    expect(observed.directoryIsDirectory && observed.socketPresent && observed.socketIsSocket, "socket observation");
    expect(observed.directoryOwner == geteuid(), "directory owner observation");
    expect(pathState(observed) == (geteuid() == 0 ? PathState::Trusted : PathState::Untrusted), "owner decides trust");
    const QString link = directory.filePath("link");
    expect(QFile::link(directory.path(), link), "fixture symlink");
    expect(!observePath(link, link + "/control.sock").directoryIsDirectory, "lstat never follows the directory symlink");
    QFile regular(directory.filePath("regular"));
    expect(regular.open(QIODevice::WriteOnly), "fixture file");
    regular.close();
    expect(!observePath(directory.path(), regular.fileName()).socketIsSocket, "regular file is not a socket");

    const int client = ::socket(AF_UNIX, SOCK_STREAM, 0);
    expect(::connect(client, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0, "fixture connect");
    quint32 uid = 1;
    qint64 pid = 0;
    expect(peerCredentials(client, &uid, &pid), "SO_PEERCRED");
    expect(uid == geteuid() && pid == getpid(), "peer credentials of this process");
    expect(peerMismatch(true, uid, pid, 0).isEmpty() == (geteuid() == 0), "only a root peer is trusted");
    ::close(client);
    ::close(listener);
}
#endif
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    classification();
    service();
    console();
    unixDecisions();
#ifdef Q_OS_WIN
    windowsObservation();
#else
    unixObservation();
#endif
    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    else std::fprintf(stdout, "control-identity-test passed\n");
    return failures ? 1 : 0;
}
