#pragma once

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>

class CoreClient final : public QObject {
    Q_OBJECT
public:
    explicit CoreClient(QObject *parent = nullptr);
    bool busy() const { return socket_ != nullptr; }
    void request(const QString &command, const QJsonObject &arguments = {}, int timeoutMs = 5000);

signals:
    void replied(QString command, QJsonObject response);
    void unavailable(QString message);

private:
    QString endpoint() const;
    void finishError(const QString &message);

    QLocalSocket *socket_ = nullptr;
    QByteArray input_;
    QString command_;
    QString requestId_;
};

