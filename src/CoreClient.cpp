#include "CoreClient.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QTimer>
#include <QUuid>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

using namespace ControlServerIdentity;

CoreClient::CoreClient(QObject *parent) : QObject(parent) {}

QString CoreClient::systemEndpoint() {
#ifdef Q_OS_WIN
    return windowsServicePipe;
#elif defined(Q_OS_LINUX)
    return unixControlSocket;
#else
    return QStringLiteral("/var/run/bimeron/control.sock");
#endif
}

QString CoreClient::endpoint() const {
    return endpoint_.isEmpty() ? systemEndpoint() : endpoint_;
}

Endpoint CoreClient::policy() const {
#ifdef Q_OS_WIN
    const Endpoint production = classifyWindows(endpoint());
#else
    const Endpoint production = classifyUnix(endpoint());
#endif
#ifdef BIMERON_CONTROL_FIXTURE_ENDPOINTS
    if (testPolicy_ != Endpoint::Unknown) return testPolicy_;
    if (production == Endpoint::Unknown) return Endpoint::Fixture;
#endif
    return production;
}

void CoreClient::setEndpoint(const QString &endpoint, qint64 expectedProcess) {
    if (socket_) {
        socket_->disconnect(this);
        socket_->abort();
        socket_->deleteLater();
        socket_ = nullptr;
    }
    input_.clear();
    endpoint_ = endpoint;
    expectedProcess_ = expectedProcess;
}

bool CoreClient::request(const QString &command, const QJsonObject &arguments, int timeoutMs, std::optional<Qt::TimerType> timerType) {
    if (socket_)
        return false;
    command_ = command;
    input_.clear();
    requestId_ = QUuid::createUuid().toString(QUuid::WithoutBraces).left(32);
    QJsonObject body{{"version", 1}, {"id", requestId_}, {"command", command}};
    for (auto it = arguments.begin(); it != arguments.end(); ++it)
        body.insert(it.key(), it.value());
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact) + '\n';
    if (payload.size() > 16 * 1024) {
        const QString message = tr("The core request exceeds the allowed size");
        emit failed(command, Failure::TooLarge, message);
        emit unavailable(message);
        return false;
    }

    socket_ = new QLocalSocket(this);
    auto *current = socket_;
    current->setReadBufferSize(256 * 1024 + 1);
    connect(current, &QLocalSocket::readyRead, this, [this, current] {
        if (socket_ != current) return;
        input_ += current->readAll();
        if (input_.size() > 256 * 1024) {
            finishError(tr("The core response exceeds the allowed size"), Failure::TooLarge);
            return;
        }
        const qsizetype lf = input_.indexOf('\n');
        if (lf < 0)
            return;
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(input_.left(lf), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            finishError(tr("Core returned invalid JSON"), Failure::InvalidResponse);
            return;
        }
        const QJsonObject response = document.object();
        if (response.value("version") != QJsonValue(1)) {
            finishError(tr("The GUI and core protocol versions are incompatible"), Failure::Incompatible);
            return;
        }
        if (response.value("id").toString() != requestId_) {
            finishError(tr("Incompatible response from core"), Failure::InvalidResponse);
            return;
        }
        const QString command = command_;
        socket_->disconnect(this);
        socket_->deleteLater();
        socket_ = nullptr;
        emit replied(command, response);
    });
    connect(current, &QLocalSocket::errorOccurred, this, [this, current](QLocalSocket::LocalSocketError error) {
        if (socket_ == current)
            finishError(error == QLocalSocket::SocketAccessError ? tr("Access to Core was denied") : tr("Core unavailable"),
                        error == QLocalSocket::SocketAccessError ? Failure::AccessDenied : Failure::Transport);
    });
    const QString requestId = requestId_;
    const auto timeout = [this, current, requestId] {
        if (socket_ == current && requestId_ == requestId)
            finishError(tr("Core did not respond in time"), Failure::Timeout);
    };
    if (timerType) QTimer::singleShot(timeoutMs, *timerType, this, timeout);
    else QTimer::singleShot(timeoutMs, this, timeout);
    connectVerified(current, payload, timeoutMs);
    return true;
}

// The server identity is verified before the first byte is written. A failed
// check closes the endpoint and never falls back to another one.
void CoreClient::connectVerified(QLocalSocket *socket, const QByteArray &payload, int timeoutMs) {
    const Endpoint kind = policy();
    if (kind == Endpoint::Unknown) { finishUntrusted(QStringLiteral("unrecognized control endpoint")); return; }
#ifdef Q_OS_WIN
    // Open the pipe directly: QLocalSocket cannot request identification-only
    // SQOS, which keeps the server from impersonating this user. GENERIC_READ
    // includes the READ_CONTROL needed to read the pipe owner.
    const QString name = endpoint();
    const std::wstring path = (name.startsWith(QStringLiteral("\\\\")) ? name : QStringLiteral("\\\\.\\pipe\\") + name).toStdWString();
    const ULONGLONG deadline = GetTickCount64() + ULONGLONG(qBound(0, timeoutMs, 5000));
    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (;;) {
        pipe = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) break;
        const DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) { finishError(tr("Access to Core was denied"), Failure::AccessDenied); return; }
        if (error != ERROR_PIPE_BUSY) { finishError(tr("Core unavailable")); return; }
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline) { finishError(tr("Core did not respond in time"), Failure::Timeout); return; }
        WaitNamedPipeW(path.c_str(), DWORD(deadline - now));
    }
    QString mismatch;
    Failure failure = Failure::Untrusted;
    quintptr serverProcess = 0; // Console daemon: retained while the socket lives.
    if (kind == Endpoint::Service) {
        const PipeObservation observed = observePipe(reinterpret_cast<quintptr>(pipe), kind);
        if (serviceTrust(observed, quint32(expectedProcess_), &mismatch) == ServiceTrust::NotRunning)
            failure = Failure::ServiceNotRunning;
        if (!mismatch.isEmpty() && legacyServiceOwner(observed))
            mismatch += QStringLiteral("; the service predates server identity checks and needs to be updated or repaired");
    } else if (kind == Endpoint::Console) {
        mismatch = consoleMismatch(observePipe(reinterpret_cast<quintptr>(pipe), kind, &serverProcess),
                                   currentUserSid(), quint32(expectedProcess_));
    } else if (expectedProcess_) {
        ULONG server = 0;
        if (!GetNamedPipeServerProcessId(pipe, &server) || qint64(server) != expectedProcess_)
            mismatch = QStringLiteral("server process %1, expected %2").arg(server).arg(expectedProcess_);
    }
    if (!mismatch.isEmpty()) {
        if (serverProcess) CloseHandle(reinterpret_cast<HANDLE>(serverProcess));
        CloseHandle(pipe);
        finishUntrusted(mismatch, failure);
        return;
    }
    // The verified daemon process handle lives as long as this connection, so
    // its process ID cannot be reused while requests are exchanged.
    if (serverProcess)
        connect(socket, &QObject::destroyed, [serverProcess] { CloseHandle(reinterpret_cast<HANDLE>(serverProcess)); });
    // QLocalSocket takes ownership of the verified overlapped handle.
    if (!socket->setSocketDescriptor(reinterpret_cast<qintptr>(pipe), QLocalSocket::ConnectedState, QIODevice::ReadWrite)) {
        finishError(tr("Core unavailable"));
        return;
    }
    socket->write(payload);
    socket->flush();
#else
    Q_UNUSED(timeoutMs);
    const QString path = endpoint();
    if (kind == Endpoint::Console) { finishUntrusted(QStringLiteral("unrecognized control endpoint")); return; }
    if (kind == Endpoint::Service) {
        QString diagnostic;
        switch (pathState(observePath(QFileInfo(path).path(), path), &diagnostic)) {
        case PathState::Missing: finishError(tr("Core unavailable")); return;
        case PathState::Untrusted: finishUntrusted(diagnostic); return;
        case PathState::Trusted: break;
        }
    }
    connect(socket, &QLocalSocket::connected, this, [this, socket, payload, kind] {
        if (socket_ != socket) return;
        quint32 uid = 0;
        qint64 pid = 0;
        const bool readable = peerCredentials(int(socket->socketDescriptor()), &uid, &pid);
        QString mismatch;
        if (kind == Endpoint::Service) mismatch = peerMismatch(readable, uid, pid, expectedProcess_);
        else if (expectedProcess_ && (!readable || pid != expectedProcess_))
            mismatch = QStringLiteral("peer process %1, expected %2").arg(pid).arg(expectedProcess_);
        if (!mismatch.isEmpty()) { finishUntrusted(mismatch); return; }
        socket->write(payload);
        socket->flush();
    });
    socket->connectToServer(path, QIODevice::ReadWrite);
#endif
}

void CoreClient::finishUntrusted(const QString &diagnostic, Failure reason) {
    finishError(tr("The Core control server could not be verified; nothing was sent (control_server_untrusted): %1")
                    .arg(diagnostic), reason);
}

void CoreClient::finishError(const QString &message, Failure reason) {
    if (!socket_)
        return;
    socket_->disconnect(this);
    socket_->abort();
    socket_->deleteLater();
    socket_ = nullptr;
    emit failed(command_, reason, message);
    emit unavailable(message);
}
