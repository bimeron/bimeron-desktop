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

void CoreClient::request(const QString &command, const QJsonObject &arguments, int timeoutMs) {
    if (socket_)
        return;
    command_ = command;
    input_.clear();
    requestId_ = QUuid::createUuid().toString(QUuid::WithoutBraces).left(32);
    QJsonObject body{{"version", 1}, {"id", requestId_}, {"command", command}};
    for (auto it = arguments.begin(); it != arguments.end(); ++it)
        body.insert(it.key(), it.value());

    socket_ = new QLocalSocket(this);
    auto *current = socket_;
    connect(current, &QLocalSocket::connected, this, [this, current, body] {
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
                finishError(tr("The control endpoint belongs to a different process"));
                return;
            }
        }
        current->write(QJsonDocument(body).toJson(QJsonDocument::Compact) + '\n');
        current->flush();
    });
    connect(current, &QLocalSocket::readyRead, this, [this, current] {
        if (socket_ != current) return;
        input_ += current->readAll();
        if (input_.size() > 256 * 1024) {
            finishError(tr("The core response exceeds the allowed size"));
            return;
        }
        const qsizetype lf = input_.indexOf('\n');
        if (lf < 0)
            return;
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(input_.left(lf), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            finishError(tr("Core returned invalid JSON"));
            return;
        }
        const QJsonObject response = document.object();
        if (response.value("version").toInt() != 1 || response.value("id").toString() != requestId_) {
            finishError(tr("Incompatible response from core"));
            return;
        }
        const QString command = command_;
        socket_->disconnect(this);
        socket_->deleteLater();
        socket_ = nullptr;
        emit replied(command, response);
    });
    connect(current, &QLocalSocket::errorOccurred, this, [this, current](QLocalSocket::LocalSocketError) {
        if (socket_ == current)
            finishError(socket_->errorString());
    });
    const QString requestId = requestId_;
    QTimer::singleShot(timeoutMs, this, [this, current, requestId] {
        if (socket_ == current && requestId_ == requestId)
            finishError(tr("Core did not respond in time"));
    });
    current->connectToServer(endpoint(), QIODevice::ReadWrite);
}

void CoreClient::finishError(const QString &message) {
    if (!socket_)
        return;
    socket_->disconnect(this);
    socket_->abort();
    socket_->deleteLater();
    socket_ = nullptr;
    emit unavailable(message);
}

