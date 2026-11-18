#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QString>
#include <QStringList>

// Presentation of Core's reconnect policy (bimeron-desktop #60).
//
// The UI is built against State, not against IPC field names: only fromIpc()
// reads Core's status and capabilities, as described in Core docs/CONTROL.MD,
// "Reconnect recovery and kill switch hold" (bimeron-core #162).
namespace ReconnectPolicy {

// Additive IPC 1 fields. ReconnectAttempt and ReconnectLimit date from Desktop
// 0.3.0; Core without the newer fields and capabilities keeps that presentation.
namespace Ipc {
inline const QLatin1String attempt{"ReconnectAttempt"};
inline const QLatin1String limit{"ReconnectLimit"};
inline const QLatin1String cycle{"ReconnectCycle"};       // only after loss of a session
inline const QLatin1String phase{"ReconnectPhase"};       // "attempting" or "waiting"
inline const QLatin1String nextAttemptAt{"NextAttemptAt"}; // RFC 3339 UTC
inline const QLatin1String killSwitch{"KillSwitch"};      // "active" or "holding"
inline const QLatin1String holding{"holding"};
inline const QLatin1String reconnectNowCapability{"reconnect_now"};
inline const QLatin1String killSwitchHoldCapability{"kill_switch_hold"};
inline const QLatin1String reconnectNowCommand{"reconnect_now"}; // guarded like stop
}

enum class Kind {
    Legacy,      // Core without the policy fields: Desktop 0.3.0 presentation
    Initial,     // the first connect: bounded retries
    Interrupted, // an established session dropped: endless retry cycles
};

enum class Phase { Attempting, Waiting };

struct State {
    Kind kind = Kind::Legacy;
    Phase phase = Phase::Attempting;
    int attempt = 0;             // 1..limit, or 0 when unknown
    int limit = 0;
    int cycle = 0;               // 1-based cycle after an interruption, otherwise 0
    qint64 nextAttemptAtMs = 0;  // epoch milliseconds of the next attempt; 0 when unknown
    bool protectionHeld = false; // the kill switch blocks traffic without a tunnel
    bool retryNowSupported = false;
};

struct Presentation {
    QString title;               // empty: keep the generic label of the state
    QString detail;              // lines under the title; empty hides them
    bool problem = false;        // red instead of the colour of the state
    bool countdown = false;      // the title changes every second
    bool retryNow = false;       // show "Retry now"
    bool disconnectVpn = false;  // the main button reads "Disconnect VPN"
    bool holdDisconnect = false; // the main button stays a disconnect button
};

inline QString text(const char *source) {
    return QCoreApplication::translate("ReconnectPolicy", source);
}

// A positive integer no larger than INT_MAX; 0 for anything else.
inline int positiveInteger(const QJsonValue &value) {
    if (!value.isDouble()) return 0;
    const double number = value.toDouble();
    if (number < 1 || number > 2147483647 || number != static_cast<int>(number)) return 0;
    return static_cast<int>(number);
}

// The only function that knows IPC field names.
inline State fromIpc(const QJsonObject &status, const QJsonObject &capabilities) {
    State state;
    state.retryNowSupported = capabilities.value(Ipc::reconnectNowCapability).toBool();
    const int attempt = positiveInteger(status.value(Ipc::attempt));
    const int limit = positiveInteger(status.value(Ipc::limit));
    if (attempt && limit && attempt <= limit) {
        state.attempt = attempt;
        state.limit = limit;
    }
    const QJsonValue phase = status.value(Ipc::phase);
    const QJsonValue killSwitch = status.value(Ipc::killSwitch);
    const bool policy = capabilities.value(Ipc::reconnectNowCapability).isBool()
        || capabilities.value(Ipc::killSwitchHoldCapability).isBool()
        || phase.isString() || killSwitch.isString() || status.contains(Ipc::cycle);
    if (!policy) return state;
    // A failed run holds protection only while its session stays active.
    state.protectionHeld = killSwitch.toString() == Ipc::holding
        && (status.value(QLatin1String("State")).toString() != QLatin1String("failed")
            || status.value(QLatin1String("Session")).toObject().value(QLatin1String("active")).toBool());
    state.cycle = positiveInteger(status.value(Ipc::cycle));
    state.kind = state.cycle ? Kind::Interrupted : Kind::Initial;
    if (state.kind == Kind::Interrupted && phase.toString() == QLatin1String("waiting")) {
        state.phase = Phase::Waiting;
        const QDateTime next = QDateTime::fromString(status.value(Ipc::nextAttemptAt).toString(), Qt::ISODateWithMs);
        if (next.isValid()) state.nextAttemptAtMs = next.toMSecsSinceEpoch();
    }
    return state;
}

// Remaining time as m:ss, rounded up so that 0:00 appears only when it is due.
inline QString countdown(qint64 remainingMs) {
    const qint64 seconds = remainingMs <= 0 ? 0 : (remainingMs + 999) / 1000;
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

// cause: localized text of Core's LastError, shown while waiting so that an
// error a retry cannot fix (for example, a rejected key) stays visible.
inline Presentation present(const QString &coreState, const State &state, const QString &cause,
                            bool proxy, qint64 nowMs) {
    Presentation view;
    if (state.kind == Kind::Legacy) return view;
    if (coreState == QLatin1String("failed")) {
        if (state.protectionHeld) {
            view.title = text("Couldn't connect. Check your internet connection and access key. "
                              "Internet access stays paused until you select Disconnect VPN.");
            view.problem = view.disconnectVpn = view.holdDisconnect = true;
            view.retryNow = state.retryNowSupported;
        }
        return view;
    }
    if (coreState != QLatin1String("connecting") && coreState != QLatin1String("reconnecting")) return view;
    QStringList detail;
    if (state.kind == Kind::Interrupted && state.phase == Phase::Waiting) {
        view.countdown = state.nextAttemptAtMs != 0;
        view.title = view.countdown
            ? text("Server unavailable · retrying in %1").arg(countdown(state.nextAttemptAtMs - nowMs))
            : text("Server unavailable");
        view.problem = true;
        view.retryNow = state.retryNowSupported;
        if (!cause.isEmpty()) detail << cause;
    } else if (state.attempt) {
        view.title = (state.kind == Kind::Initial ? text("Connecting… (%1/%2)") : text("Reconnecting… (%1/%2)"))
            .arg(state.attempt).arg(state.limit);
    }
    view.disconnectVpn = state.kind == Kind::Interrupted && !proxy;
    if (state.protectionHeld) {
        detail << text("Internet access is paused until the VPN connects.");
        view.disconnectVpn = view.holdDisconnect = true;
    }
    view.detail = detail.join(QLatin1Char('\n'));
    return view;
}

// One tray message per retry cycle and one after recovery.
class Notifications {
public:
    QString update(const QString &coreState, const State &state, bool proxy) {
        const bool trying = coreState == QLatin1String("connecting") || coreState == QLatin1String("reconnecting");
        if (coreState == QLatin1String("connected")) {
            const bool recovered = cycle_ != 0;
            cycle_ = 0;
            if (recovered) return proxy ? text("Proxy reconnected") : text("VPN reconnected");
            return {};
        }
        if (!trying) {
            cycle_ = 0;
            return {};
        }
        if (state.kind != Kind::Interrupted || state.cycle == cycle_) return {};
        cycle_ = state.cycle;
        if (state.cycle == 1)
            return proxy ? text("Proxy connection lost. Reconnecting…") : text("VPN connection lost. Reconnecting…");
        return proxy ? text("Proxy is still not connected. Retrying…") : text("VPN is still not connected. Retrying…");
    }

    bool recovering() const { return cycle_ != 0; }

private:
    int cycle_ = 0;
};

} // namespace ReconnectPolicy
