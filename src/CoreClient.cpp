#include "CoreClient.h"

#include <QJsonDocument>
#include <QTimer>
#include <QUuid>

CoreClient::CoreClient(QObject *parent) : QObject(parent) {}

QString CoreClient::endpoint() const {
#ifdef Q_OS_WIN
    return QStringLiteral("Bimeron.Core.Service");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("/run/bimeron/control.sock");
#else
    return QStringLiteral("/var/run/bimeron/control.sock");
#endif
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
    connect(current, &QLocalSocket::connected, this, [current, body] {
        current->write(QJsonDocument(body).toJson(QJsonDocument::Compact) + '\n');
        current->flush();
    });
    connect(current, &QLocalSocket::readyRead, this, [this, current] {
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
        socket_->deleteLater();
        socket_ = nullptr;
        emit replied(command, response);
    });
    connect(current, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        if (socket_)
            finishError(socket_->errorString());
    });
    QTimer::singleShot(timeoutMs, this, [this, current] {
        if (socket_ == current)
            finishError(tr("Core did not respond in time"));
    });
    current->connectToServer(endpoint(), QIODevice::ReadWrite);
}

void CoreClient::finishError(const QString &message) {
    if (!socket_)
        return;
    socket_->abort();
    socket_->deleteLater();
    socket_ = nullptr;
    emit unavailable(message);
}

