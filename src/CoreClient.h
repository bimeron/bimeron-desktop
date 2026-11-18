#pragma once

#include "ControlServerIdentity.h"

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>

class CoreClient final : public QObject {
    Q_OBJECT
public:
    // Untrusted: the endpoint failed the server identity check; nothing was
    // written (control_server_untrusted).
    enum class Failure { Transport, AccessDenied, Timeout, Incompatible, InvalidResponse, Untrusted, TooLarge };
    Q_ENUM(Failure)
    explicit CoreClient(QObject *parent = nullptr);
    bool busy() const { return socket_ != nullptr; }
    static QString systemEndpoint();
    void setEndpoint(const QString &endpoint = {}, qint64 expectedProcess = 0);
    void cancel() { setEndpoint(endpoint_, expectedProcess_); }
    bool request(const QString &command, const QJsonObject &arguments = {}, int timeoutMs = 5000);
#ifdef BIMERON_CONTROL_FIXTURE_ENDPOINTS
    // Applies a production identity policy to a fixture endpoint.
    void setIdentityPolicyForTesting(ControlServerIdentity::Endpoint policy) { testPolicy_ = policy; }
#endif

signals:
    void replied(QString command, QJsonObject response);
    void unavailable(QString message);
    void failed(QString command, CoreClient::Failure reason, QString message);

private:
    QString endpoint() const;
    ControlServerIdentity::Endpoint policy() const;
    void finishError(const QString &message, Failure reason = Failure::Transport);
    void finishUntrusted(const QString &diagnostic);
    void connectVerified(QLocalSocket *socket, const QByteArray &payload, int timeoutMs);

    QLocalSocket *socket_ = nullptr;
    QByteArray input_;
    QString command_;
    QString requestId_;
    QString endpoint_;
    qint64 expectedProcess_ = 0;
#ifdef BIMERON_CONTROL_FIXTURE_ENDPOINTS
    ControlServerIdentity::Endpoint testPolicy_ = ControlServerIdentity::Endpoint::Unknown;
#endif
};
