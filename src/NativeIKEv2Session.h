#pragma once
#include "NativeIKEv2Transport.h"
#include <QTimer>
#include <functional>

// Thread-affine, foreground-only controller. Destruction releases local handles,
// never stops an owner. No settings, packet backend, credentials or statistics.
class NativeIKEv2Session final : public QObject {
    Q_OBJECT
public:
    using Endpoint = NativeIKEv2Transport::Endpoint;
    struct Gates {
        bool managedHostsCompatibleAndIdle = false;
        bool protectionAuthoritativelyAbsent = false;
        bool ownerAuthoritativelyAbsent = false;
        bool protectedInstallationApproved = false;
    };
    struct Snapshot {
        bool authoritative = false, ownerClosed = false, closureAccepted = false;
        bool requested = false, recoveryRequired = false;
        QString instance, installation, session, state, operation;
        QString wfp = "unknown", dns = "unknown", safeError;
        QJsonObject owner, native, profile, ikev2;
    };
    explicit NativeIKEv2Session(NativeIKEv2Transport::Approval approval, QObject *parent = nullptr);
    ~NativeIKEv2Session() override;
    const Snapshot &snapshot() const { return snapshot_; }
    // Fresh authoritative external checks are required for EACH launch attempt.
    void setLaunchGates(Gates gates) { gates_ = gates; }
    bool discover(Endpoint endpoint = Endpoint::Session);
    // Digest is from selected-Core validation of these exact config bytes.
    bool connectProfile(const QString &absolutePath, const QString &reviewedSha256);
    bool previewProfile(const QString &name, const QString &server, const QString &action);
    bool confirmPreview(const QString &token);
    bool stop();
    bool closeOwner();
    // Local cancellation only: preserves owner and marks the snapshot unknown.
    void cancelExchange();
signals:
    void changed();
private:
    // Narrow offline seam: production always binds these calls to the real transport.
    struct Hooks {
        std::function<bool(Endpoint, const QString &, const QJsonObject &)> request;
        std::function<bool(const QString &, const QString &)> session;
        std::function<bool(const QString &, const QString &, bool)> provision;
        std::function<void()> cancel;
        std::function<NativeIKEv2Transport::ProcessState()> process;
    };
    NativeIKEv2Session(NativeIKEv2Transport::Approval, Hooks, QObject *);
    void initialize();
    bool send(const QString &, QJsonObject = {});
    bool launchAllowed();
    void received(const QString &, const QJsonObject &);
    void failed(const QString &, NativeIKEv2Transport::Failure);
    void launched();
    void poll();
    void schedule();
    void unknown(const QString &);
    bool terminal() const;
    bool clean() const;
    bool supported() const;
    const NativeIKEv2Transport::Approval approval_;
    NativeIKEv2Transport *transport_ = nullptr;
    Hooks hooks_;
    Gates gates_;
    Snapshot snapshot_;
    Endpoint endpoint_ = Endpoint::Session;
    QTimer timer_, deadline_;
    QString pending_, action_, name_, server_;
    bool launching_ = false, activationPending_ = false, activationSent_ = false;
    bool stopPending_ = false, confirmationSent_ = false, closeSent_ = false;
    bool launchAttempted_ = false, sessionActive_ = false;
    bool fake_ = false, poisoned_ = false, eventsNext_ = false;
    double cursor_ = 0;
    friend struct NativeIKEv2SessionTest;
};
