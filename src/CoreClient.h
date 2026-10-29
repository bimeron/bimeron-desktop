#pragma once

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>

class CoreClient final : public QObject {
    Q_OBJECT
public:
    enum class Failure { Transport, AccessDenied, Timeout, Incompatible, InvalidResponse, PeerMismatch, TooLarge };
    Q_ENUM(Failure)
    explicit CoreClient(QObject *parent = nullptr);
    bool busy() const { return socket_ != nullptr; }
    static QString systemEndpoint();
    void setEndpoint(const QString &endpoint = {}, qint64 expectedProcess = 0);
    void cancel() { setEndpoint(endpoint_, expectedProcess_); }
    bool request(const QString &command, const QJsonObject &arguments = {}, int timeoutMs = 5000);

signals:
    void replied(QString command, QJsonObject response);
    void unavailable(QString message);
    void failed(QString command, CoreClient::Failure reason, QString message);

private:
    QString endpoint() const;
    void finishError(const QString &message, Failure reason = Failure::Transport);

    QLocalSocket *socket_ = nullptr;
    QByteArray input_;
    QString command_;
    QString requestId_;
    QString endpoint_;
    qint64 expectedProcess_ = 0;
};

