#include "NativeIKEv2Session.h"
#include <QDir>
#include <QRegularExpression>

namespace {
bool hex(const QString &s, int n) {
    return s.size() == n && QRegularExpression("^[0-9a-f]+$").match(s).hasMatch();
}
bool decimal64(const QJsonValue &v) {
    if (!v.isString() || !QRegularExpression("^[0-9]{1,20}$").match(v.toString()).hasMatch()) return false;
    bool ok = false; v.toString().toULongLong(&ok); return ok;
}
QString safe(const QString &s) {
    // UI receives codes only; elevated diagnostic text is never exposed.
    static const QStringList codes{"activation_cancelled", "owner_closing", "cleanup_pending",
        "session_not_terminal", "stale_instance", "installation_mismatch", "operation_pending",
        "core_identity_mismatch", "session_start_failed", "session_stop_failed",
        "ikev2_profile_preview_required", "ikev2_profile_preview_changed",
        "ikev2_profile_incomplete_transaction_manual_review_required",
        "ikev2_profile_rollback_incomplete_manual_review_required"};
    return s.isEmpty() || codes.contains(s) ? s : QStringLiteral("native_operation_failed");
}
}
NativeIKEv2Session::NativeIKEv2Session(NativeIKEv2Transport::Approval a, QObject *parent)
    : QObject(parent), approval_(std::move(a)) {
    transport_ = new NativeIKEv2Transport(approval_, this);
    hooks_.request = [this](Endpoint e, const QString &c, const QJsonObject &f) { return transport_->request(e,c,f); };
    hooks_.session = [this](const QString &p,const QString &h) { return transport_->launchSession(p,h); };
    hooks_.provision = [this](const QString &n,const QString &s,bool r) { return transport_->launchProvisioning(n,s,r); };
    hooks_.cancel = [this] { transport_->cancelExchange(); };
    hooks_.process = [this] { return transport_->processState(); };
    connect(transport_, &NativeIKEv2Transport::replied, this, &NativeIKEv2Session::received);
    connect(transport_, &NativeIKEv2Transport::failed, this, &NativeIKEv2Session::failed);
    connect(transport_, &NativeIKEv2Transport::launched, this, &NativeIKEv2Session::launched);
    initialize();
}
NativeIKEv2Session::NativeIKEv2Session(NativeIKEv2Transport::Approval a, Hooks h, QObject *p)
    : QObject(p), approval_(std::move(a)), hooks_(std::move(h)), fake_(true) { initialize(); }
NativeIKEv2Session::~NativeIKEv2Session() { if (transport_) transport_->releaseHandles(); }
void NativeIKEv2Session::initialize() {
    timer_.setSingleShot(true); timer_.setTimerType(Qt::PreciseTimer); deadline_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, &NativeIKEv2Session::poll);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        hooks_.cancel(); pending_.clear(); launching_ = false; activationPending_ = false;
        unknown("native_timeout"); schedule();
    });
}
bool NativeIKEv2Session::supported() const {
#ifdef Q_OS_WIN
    return true;
#else
    return fake_;
#endif
}
void NativeIKEv2Session::unknown(const QString &code) {
    snapshot_.authoritative = false; snapshot_.safeError = code; emit changed();
}
bool NativeIKEv2Session::discover(Endpoint e) {
    if (!supported()) { unknown("native_unsupported"); return false; }
    if (!pending_.isEmpty() || launching_ || poisoned_ || closeSent_) return false;
    if (!snapshot_.instance.isEmpty() && e != endpoint_) return false;
    endpoint_ = e; return send("hello");
}
bool NativeIKEv2Session::launchAllowed() {
    const auto g = gates_; gates_ = {};
    if (!supported() || launchAttempted_ || poisoned_ || launching_ || !pending_.isEmpty() || !snapshot_.instance.isEmpty() ||
        !g.managedHostsCompatibleAndIdle || !g.protectionAuthoritativelyAbsent ||
        !g.ownerAuthoritativelyAbsent || !g.protectedInstallationApproved ||
        !hex(approval_.sha256,64) || approval_.revision != "66d84d8f4dcfc6ada625eb11894b19feea51b7ba") {
        unknown(supported() ? "native_launch_blocked" : "native_unsupported"); return false;
    }
    timer_.stop(); launchAttempted_ = true; launching_ = true; deadline_.start(30000); return true;
}
bool NativeIKEv2Session::connectProfile(const QString &p, const QString &hash) {
    if (!QDir::isAbsolutePath(p) || !hex(hash,64)) return false;
    if (!launchAllowed()) return false;
    endpoint_ = Endpoint::Session; activationPending_ = true;
    if (hooks_.session(p,hash)) return true;
    launching_ = false; deadline_.stop(); activationPending_ = false; unknown("native_launch_blocked"); return false;
}
bool NativeIKEv2Session::previewProfile(const QString &n,const QString &s,const QString &a) {
    if (n.isEmpty() || s.isEmpty() || (a != "apply" && a != "remove")) return false;
    if (!launchAllowed()) return false;
    endpoint_ = Endpoint::Provisioning; name_ = n; server_ = s; action_ = a;
    if (hooks_.provision(n,s,a == "remove")) return true;
    launching_ = false; deadline_.stop(); unknown("native_launch_blocked"); return false;
}
void NativeIKEv2Session::launched() {
    if (!launching_) return;
    launching_ = false; deadline_.stop(); send("hello");
}
bool NativeIKEv2Session::send(const QString &c,QJsonObject f) {
    if (!pending_.isEmpty() || launching_ || poisoned_) return false;
    if (c != "hello" || f.contains("accept_revision")) {
        if (snapshot_.instance.isEmpty()) return false;
        f.insert("instance",snapshot_.instance);
        if (endpoint_ == Endpoint::Session && c != "hello") f.insert("session",snapshot_.session);
    }
    timer_.stop(); pending_ = c; deadline_.start(c == "stop" ? 16000 : 15000);
    if (hooks_.request(endpoint_,c,f)) return true;
    pending_.clear(); deadline_.stop(); unknown("native_transport_unavailable"); schedule(); return false;
}
bool NativeIKEv2Session::terminal() const {
    if (endpoint_ == Endpoint::Provisioning)
        return QStringList{"previewed","succeeded","failed"}.contains(snapshot_.operation);
    return snapshot_.state == "disconnected" || snapshot_.state == "failed";
}
bool NativeIKEv2Session::clean() const {
    if (!snapshot_.authoritative || !terminal() || sessionActive_) return false;
    if (endpoint_ == Endpoint::Provisioning) return true;
    const auto n = snapshot_.native;
    return !snapshot_.recoveryRequired && snapshot_.wfp == "inactive" && snapshot_.dns == "inactive" &&
        n.value("ras_cleanup") == "succeeded" && n.value("dns_cleanup") == "succeeded" && n.value("wfp_cleanup") == "succeeded";
}
bool NativeIKEv2Session::stop() {
    if (endpoint_ != Endpoint::Session || poisoned_ || closeSent_) return false;
    activationPending_ = false; stopPending_ = true;
    if (launching_ || !pending_.isEmpty()) return true;
    if (snapshot_.instance.isEmpty()) return false;
    stopPending_ = false; return send("stop");
}
bool NativeIKEv2Session::closeOwner() {
    if (closeSent_ || !pending_.isEmpty() || !clean()) return false;
    closeSent_ = true; return send("close_owner");
}
bool NativeIKEv2Session::confirmPreview(const QString &token) {
    const auto p = snapshot_.profile;
    if (endpoint_ != Endpoint::Provisioning || confirmationSent_ || !pending_.isEmpty() || launching_ || poisoned_ || closeSent_ || !snapshot_.authoritative ||
        snapshot_.operation != "previewed" || !hex(token,64) || token != p.value("state_token").toString() ||
        p.value("windows_profile") != name_ || p.value("server_name") != server_ ||
        (action_ == "remove" ? p.value("action") != "remove" :
            (p.value("action") != "create" && p.value("action") != "unchanged"))) return false;
    confirmationSent_ = true;
    return send(action_,{{"accept_revision",approval_.revision},{"plan_token",token}});
}
void NativeIKEv2Session::received(const QString &c,const QJsonObject &r) {
    if (pending_ != c || poisoned_) return;
    pending_.clear(); deadline_.stop();
    const auto owner = r.value("owner").toObject();
    const auto instance = r.value("instance").toString(), installation = r.value("installation").toString();
    const auto status = r.value("status").toObject();
    const auto session = status.value("Session").toObject().value("id").toString();
    if (!hex(instance,32) || !hex(installation,32) || !decimal64(owner.value("created")) ||
        !decimal64(owner.value("image_index")) ||
        (!snapshot_.instance.isEmpty() && (instance != snapshot_.instance || installation != snapshot_.installation || owner != snapshot_.owner)) ||
        (!snapshot_.session.isEmpty() && session != snapshot_.session)) {
        poisoned_ = true; unknown("native_identity_changed"); return;
    }
    if (c == "hello" && r.value("error").toString().isEmpty()) {
        const auto core = r.value("core").toObject();
        const auto ike = r.value("capabilities").toObject().value("ikev2").toObject();
        if (core.value("revision") != approval_.revision || core.value("modified") != false || ike.value("supported") != true) {
            poisoned_ = true; unknown("core_identity_mismatch"); return;
        }
        snapshot_.ikev2 = ike;
    }
    snapshot_.instance = instance; snapshot_.installation = installation; snapshot_.owner = owner;
    sessionActive_ = status.value("Session").toObject().value("active").toBool();
    snapshot_.session = session; snapshot_.state = status.value("State").toString();
    snapshot_.native = status.value("native").toObject();
    snapshot_.requested = snapshot_.native.value("requested").toBool();
    snapshot_.recoveryRequired = snapshot_.native.value("recovery_required").toBool();
    snapshot_.wfp = snapshot_.native.value("wfp").toString("unknown");
    snapshot_.dns = snapshot_.native.value("dns").toString("unknown");
    snapshot_.operation = r.value("operation").toString(); snapshot_.profile = r.value("profile").toObject();
    snapshot_.safeError = safe(r.value("error").toString()); snapshot_.authoritative = true;
    if (snapshot_.safeError == "stale_instance" || snapshot_.safeError == "installation_mismatch") {
        poisoned_ = true; unknown("native_identity_changed"); return;
    }
    if (c == "close_owner") {
        snapshot_.closureAccepted = snapshot_.safeError.isEmpty();
        if (!snapshot_.closureAccepted) closeSent_ = false;
        // Even an accepted close is not observed process exit or cleanup.
        snapshot_.authoritative = false;
    }
    if (c == "events") {
        cursor_ = r.value("cursor").toDouble();
        if (r.value("dropped").toBool() || r.value("source_drops").toDouble() > 0) {
            snapshot_.authoritative = false; emit changed(); send("status"); return;
        }
    }
    emit changed();
    if (stopPending_) { stopPending_ = false; send("stop"); return; }
    if (activationPending_ && !activationSent_ && c == "hello" && snapshot_.safeError.isEmpty()) {
        activationSent_ = true; activationPending_ = false;
        send("hello",{{"accept_revision",approval_.revision}}); return;
    }
    schedule();
}
void NativeIKEv2Session::failed(const QString &c,NativeIKEv2Transport::Failure f) {
    if ((pending_ != c && !launching_) || poisoned_) return;
    pending_.clear(); launching_ = false; deadline_.stop(); activationPending_ = false;
    if (f == NativeIKEv2Transport::Failure::IdentityRejected || f == NativeIKEv2Transport::Failure::InvalidResponse) poisoned_ = true;
    unknown(f == NativeIKEv2Transport::Failure::Cancelled ? "native_cancelled" : "native_transport_unavailable");
    if (c == "launch" && (f == NativeIKEv2Transport::Failure::Cancelled ||
        f == NativeIKEv2Transport::Failure::ApprovalRequired || f == NativeIKEv2Transport::Failure::Unsupported)) return;
    schedule();
}
void NativeIKEv2Session::cancelExchange() {
    hooks_.cancel(); pending_.clear(); launching_ = false; activationPending_ = false; deadline_.stop();
    unknown("native_cancelled"); schedule();
}
void NativeIKEv2Session::schedule() {
    if (!poisoned_ && pending_.isEmpty() && !launching_ && (!snapshot_.instance.isEmpty() || launchAttempted_) && !snapshot_.ownerClosed)
        timer_.start(terminal() ? 2000 : 500);
}
void NativeIKEv2Session::poll() {
    if (!pending_.isEmpty() || poisoned_) return;
    if (snapshot_.instance.isEmpty()) { send("hello"); return; }
    if (closeSent_) {
        if (snapshot_.closureAccepted && hooks_.process() == NativeIKEv2Transport::ProcessState::Exited) {
            snapshot_.ownerClosed = true; emit changed(); return;
        }
        // A missing pipe or process exit after a lost close reply proves nothing.
        unknown("native_closure_unknown"); schedule(); return;
    }
    if (endpoint_ == Endpoint::Session && snapshot_.authoritative && eventsNext_) {
        eventsNext_ = false; send("events",{{"after",cursor_}});
    } else { eventsNext_ = true; send("status"); }
}
