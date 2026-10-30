#pragma once

#include <QJsonObject>
#include <QObject>
#include <memory>

// Foreground IPC only. No daemon, networking, policy writing or lifecycle inference.
// The integrator must supply reviewed final-artifact approval, never a user setting.
// All methods run on the QObject thread. Windows consent/IO run on workers.
class NativeIKEv2Transport final : public QObject {
    Q_OBJECT
public:
    struct Approval {
        QString executable;
        QString sha256;
        QString revision;
        QString architecture; // Go spelling: amd64 or arm64
    };
    struct Identity {
        quint32 pid = 0, session = 0, imageVolume = 0;
        quint64 created = 0, imageIndex = 0;
        QString sid, imageSha256;
        bool elevated = false;
        bool operator==(const Identity &) const = default;
    };
    enum class Endpoint { Session, Provisioning };
    enum class ProcessState { Unknown, Running, Exited };
    enum class Failure { Unsupported, ApprovalRequired, IdentityRejected, Unavailable,
                         InvalidRequest, InvalidResponse, Timeout, Cancelled, LaunchRejected };
    Q_ENUM(Failure)

    explicit NativeIKEv2Transport(Approval approval, QObject *parent = nullptr);
    ~NativeIKEv2Transport() override;
    bool busy() const;
    // Nonblocking OS observation only. Exited never proves successful cleanup.
    ProcessState processState() const;
    bool discover(Endpoint endpoint);
    // Caller must first establish authoritative managed-host inactivity and absence
    // of retained/unknown protection. Missing IPC alone never authorizes launch.
    // Configuration bytes must already have passed selected-Core native validation;
    // this layer pins the local file/ancestors and rechecks their reviewed digest.
    bool launchSession(const QString &absoluteConfig, const QString &reviewedConfigSha256);
    bool launchProvisioning(const QString &profile, const QString &server, bool remove);
    // Fields are typed/allowlisted by command; version/id/installation are owned here.
    bool request(Endpoint endpoint, const QString &command, const QJsonObject &fields = {});
    void cancelExchange(); // Cancels local IO only, never terminates an owner.
    // Permanently closes this client; never sends stop/close. An outstanding UAC
    // call retains its pins until it returns, without blocking this QObject thread.
    void releaseHandles();

signals:
    void replied(QString command, QJsonObject response);
    void launched(); // Process creation only; not authentication, activation or readiness.
    void failed(QString command, NativeIKEv2Transport::Failure reason);

private:
    struct State;
    const Approval approval_;
    std::shared_ptr<State> state_;
    quint64 generation_ = 0;
    bool closed_ = false;
    bool start(Endpoint endpoint, QString command, QJsonObject fields,
               QString config = {}, QString configHash = {});
    static QString endpointName(const QString &sid, quint32 session);
    static bool approved(const Approval &approval);
#ifdef Q_OS_WIN
    static bool protectedDescriptor(const void *descriptor);
#endif
    static QByteArray encodeRequest(Endpoint endpoint, const QString &command,
                                   const QString &id, const QString &installation,
                                   const QString &revision, const QJsonObject &fields);
    static bool decodeResponse(const QByteArray &frame, const QString &id,
                               const QString &installation, const Approval &approval,
                               Endpoint endpoint, const Identity &peer,
                               const QString &instance, QJsonObject *response);
    friend struct NativeIKEv2TransportTest;
};
