#include "CoreClient.h"

#include <QJsonDocument>
#include <QTimer>
#include <QUuid>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#elif defined(Q_OS_LINUX)
#include <sys/socket.h>
#endif

CoreClient::CoreClient(QObject *parent) : QObject(parent) {}

QString CoreClient::systemEndpoint() {
#ifdef Q_OS_WIN
    return QStringLiteral("Bimeron.Core.Service");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("/run/bimeron/control.sock");
#else
    return QStringLiteral("/var/run/bimeron/control.sock");
#endif
}

QString CoreClient::endpoint() const {
    return endpoint_.isEmpty() ? systemEndpoint() : endpoint_;
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

bool CoreClient::request(const QString &command, const QJsonObject &arguments, int timeoutMs) {
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
    connect(current, &QLocalSocket::connected, this, [this, current, payload] {
        if (socket_ != current) return;
        if (expectedProcess_) {
            qint64 peer = 0;
#ifdef Q_OS_WIN
            ULONG process = 0;
            if (GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(current->socketDescriptor()), &process))
                peer = process;
#elif defined(Q_OS_LINUX)
            struct ucred credentials {};
            socklen_t size = sizeof(credentials);
            if (getsockopt(static_cast<int>(current->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &credentials, &size) == 0)
                peer = credentials.pid;
#endif
            if (peer != expectedProcess_) {
                finishError(tr("The control endpoint belongs to a different process"), Failure::PeerMismatch);
                return;
            }
        }
        current->write(payload);
        current->flush();
    });
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
    QTimer::singleShot(timeoutMs, this, [this, current, requestId] {
        if (socket_ == current && requestId_ == requestId)
            finishError(tr("Core did not respond in time"), Failure::Timeout);
    });
    current->connectToServer(endpoint(), QIODevice::ReadWrite);
    return true;
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

