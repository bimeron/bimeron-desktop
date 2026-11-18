#pragma once
#include "ReconnectPolicy.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <QTranslator>

#include <cstdio>

// Model-level tests for the reconnect policy view (bimeron-desktop #60). They
// run without a window, Core or network.
namespace ReconnectPolicyTest {

inline int failures = 0;

inline void expect(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

inline void expectText(const QString &actual, const QString &expected, const char *message) {
    if (actual != expected) {
        std::fprintf(stderr, "FAIL: %s\n  expected: %s\n  actual:   %s\n", message,
                     qPrintable(expected), qPrintable(actual));
        ++failures;
    }
}

class JsonTranslator final : public QTranslator {
public:
    QJsonObject strings;
    QString translate(const char *, const char *source, const char *, int) const override {
        return strings.value(QString::fromUtf8(source)).toString();
    }
    bool isEmpty() const override { return strings.isEmpty(); }
};

using namespace ReconnectPolicy;

inline const qint64 now = QDateTime::fromString("2026-11-18T12:00:00Z", Qt::ISODate).toMSecsSinceEpoch();

inline QString at(qint64 offsetMs) {
    return QDateTime::fromMSecsSinceEpoch(now + offsetMs, QTimeZone::utc()).toString(Qt::ISODate);
}

// KillSwitch is absent when the run owns no protection.
inline QJsonObject withHold(QJsonObject status, bool held) {
    if (held) status.insert("KillSwitch", "holding");
    return status;
}

inline QJsonObject attempting(int cycle, int attempt, bool held = false) {
    return withHold({{"State", "reconnecting"}, {"ReconnectAttempt", attempt}, {"ReconnectLimit", 10},
                     {"ReconnectCycle", cycle}, {"ReconnectPhase", "attempting"}}, held);
}

inline QJsonObject waiting(int cycle, qint64 remainingMs, bool held = false) {
    // Core omits ReconnectAttempt and ReconnectLimit while waiting.
    return withHold({{"State", "reconnecting"}, {"ReconnectCycle", cycle}, {"ReconnectPhase", "waiting"},
                     {"NextAttemptAt", at(remainingMs)}}, held);
}

inline Presentation view(const QJsonObject &status, const QJsonObject &capabilities = {},
                  const QString &cause = {}, bool proxy = false, qint64 clock = now) {
    return present(status.value("State").toString(), fromIpc(status, capabilities), cause, proxy, clock);
}

inline void mapping() {
    // Desktop 0.3.0 Core: only the published fields.
    State legacy = fromIpc({{"State", "reconnecting"}, {"ReconnectAttempt", 2}, {"ReconnectLimit", 3}}, {});
    expect(legacy.kind == Kind::Legacy && legacy.attempt == 2 && legacy.limit == 3, "legacy progress");
    expect(!view({{"State", "reconnecting"}, {"ReconnectAttempt", 2}, {"ReconnectLimit", 3}}).title.size(),
           "legacy view keeps the 0.3.0 label");
    expect(fromIpc({}, {}).kind == Kind::Legacy, "empty status is legacy");

    State initial = fromIpc({{"State", "reconnecting"}, {"ReconnectAttempt", 1}, {"ReconnectLimit", 3},
                             {"ReconnectPhase", "attempting"}}, {});
    expect(initial.kind == Kind::Initial && initial.attempt == 1 && initial.limit == 3, "initial connect");
    for (const char *capability : {"reconnect_now", "kill_switch_hold"}) {
        State advertised = fromIpc({{"State", "reconnecting"}, {"ReconnectAttempt", 2}, {"ReconnectLimit", 3}},
                                   {{capability, false}});
        expect(advertised.kind == Kind::Initial, "a policy capability marks the new Core");
    }

    State dropped = fromIpc(attempting(2, 4, true), {{"reconnect_now", true}});
    expect(dropped.kind == Kind::Interrupted && dropped.cycle == 2 && dropped.attempt == 4
           && dropped.limit == 10 && dropped.phase == Phase::Attempting && dropped.protectionHeld
           && dropped.retryNowSupported, "interrupted attempt");

    State paused = fromIpc(waiting(1, 59000), {});
    expect(paused.phase == Phase::Waiting && paused.nextAttemptAtMs == now + 59000, "waiting time");
    expect(!paused.retryNowSupported, "retry now needs the capability");
    expect(!fromIpc(waiting(1, 1000), {{"reconnect_now", "yes"}}).retryNowSupported, "capability must be boolean");

    QJsonObject backoff = attempting(1, 3);
    backoff["NextAttemptAt"] = at(4000);
    State scheduled = fromIpc(backoff, {});
    expect(scheduled.phase == Phase::Attempting && scheduled.nextAttemptAtMs == now + 4000, "backoff time");
    expect(fromIpc(attempting(1, 3), {}).nextAttemptAtMs == 0, "running attempt has no schedule");
    QJsonObject closing{{"State", "reconnecting"}, {"ReconnectCycle", 2}};
    expect(fromIpc(closing, {}).kind == Kind::Interrupted && fromIpc(closing, {}).phase == Phase::Attempting
           && view(closing).title.isEmpty() && !view(closing).problem, "absent phase is plain reconnecting");

    QJsonObject permanent = waiting(1, 1000);
    permanent["LastError"] = "access_key_unavailable";
    permanent["LastErrorPermanent"] = true;
    expect(fromIpc(permanent, {}).lastErrorPermanent && !fromIpc(waiting(1, 1000), {}).lastErrorPermanent,
           "permanent error marker");
    expect(view(waiting(1, 1000), {}, "Transient cause").detail == "Transient cause", "the cause is shown for every error");

    // Recovery routing: any *_run_recover code, and leftover protection.
    for (const char *code : {"cleanup_failed_run_recover", "kill_switch_pending_run_recover", "kill_switch_arm_failed_run_recover"}) {
        const State recover = fromIpc({{"State", "failed"}, {"LastError", code}}, {});
        expect(recover.recoveryRequired && !recover.residualProtection && !recover.protectionHeld, "run_recover code routes to recovery");
        expect(fromIpc({{"State", "failed"}, {"LastError", code}}, {}).kind == Kind::Legacy, "old Core keeps its recovery code");
    }
    for (const char *code : {"transport_check_failed", "access_key_unavailable", "run_recover_unknown", ""})
        expect(!fromIpc({{"State", "failed"}, {"LastError", code}}, {}).recoveryRequired, "other codes do not route to recovery");
    const QJsonObject residual{{"State", "failed"}, {"LastError", "cleanup_failed_run_recover"},
                               {"LastErrorPermanent", true}, {"KillSwitch", "residual"}};
    State leftover = fromIpc(residual, {{"reconnect_now", true}});
    expect(leftover.residualProtection && leftover.recoveryRequired && leftover.lastErrorPermanent, "residual protection");
    expect(!leftover.protectionHeld, "residual is not holding");
    QJsonObject residualOther = residual;
    residualOther["LastError"] = "access_key_unavailable";
    expect(fromIpc(residualOther, {}).recoveryRequired, "the marker routes to recovery when another failure is shown");
    QJsonObject residualSession = residual;
    residualSession["Session"] = QJsonObject{{"active", false}};
    expect(fromIpc(residualSession, {}).residualProtection, "residual with an inactive session");
    const QJsonObject held{{"State", "failed"}, {"KillSwitch", "holding"}, {"Session", QJsonObject{{"active", true}}},
                           {"LastError", "transport_check_failed"}};
    expect(fromIpc(held, {}).protectionHeld && !fromIpc(held, {}).residualProtection && !fromIpc(held, {}).recoveryRequired,
           "holding is not residual");

    QJsonObject badTime = waiting(1, 1000);
    badTime["NextAttemptAt"] = "soon";
    expect(fromIpc(badTime, {}).nextAttemptAtMs == 0, "invalid time is unknown");

    for (const QJsonValue bad : {QJsonValue(0), QJsonValue(11), QJsonValue(1.5), QJsonValue("1")}) {
        QJsonObject status = attempting(1, 1);
        status["ReconnectAttempt"] = bad;
        expect(fromIpc(status, {}).attempt == 0, "invalid attempt is ignored");
    }
    QJsonObject badCycle = attempting(1, 1);
    badCycle["ReconnectCycle"] = -1;
    expect(fromIpc(badCycle, {}).kind == Kind::Initial, "invalid cycle is not an interruption");
    QJsonObject attemptingPhase = waiting(1, 1000);
    attemptingPhase["ReconnectPhase"] = "unknown";
    expect(fromIpc(attemptingPhase, {}).phase == Phase::Attempting, "unknown phase is attempting");
}

inline void countdownText() {
    expectText(countdown(60000), "1:00", "full minute");
    expectText(countdown(59000), "0:59", "59 seconds");
    expectText(countdown(58001), "0:59", "rounds up");
    expectText(countdown(1), "0:01", "last second");
    expectText(countdown(0), "0:00", "due");
    expectText(countdown(-5000), "0:00", "late");
    expectText(countdown(125000), "2:05", "minutes");
}

inline void views(bool russian) {
    const auto pick = [russian](const char *en, const char *ru) { return QString::fromUtf8(russian ? ru : en); };

    Presentation p = view({{"State", "connecting"}, {"ReconnectAttempt", 2}, {"ReconnectLimit", 3},
                           {"ReconnectPhase", "attempting"}});
    expectText(p.title, pick("Connecting… (2/3)", "Подключение… (2/3)"), "initial title");
    expect(!p.problem && !p.disconnectVpn && !p.holdDisconnect && !p.retryNow && p.detail.isEmpty(), "initial view");

    p = view(attempting(1, 7), {{"reconnect_now", true}});
    expectText(p.title, pick("Reconnecting… (7/10)", "Переподключение… (7/10)"), "cycle attempt title");
    expect(!p.problem && p.disconnectVpn && !p.holdDisconnect && !p.countdown && !p.retryNow && p.detail.isEmpty(),
           "cycle attempt view: no Retry now while an attempt runs");
    expect(!view(attempting(1, 7), {}, {}, true).disconnectVpn, "proxy keeps Disconnect");

    p = view(waiting(1, 59000), {{"reconnect_now", true}});
    expectText(p.title, pick("Server unavailable · retrying in 0:59", "Сервер недоступен · повтор через 0:59"), "waiting title");
    expect(p.problem && p.countdown && p.retryNow && p.disconnectVpn && !p.holdDisconnect, "waiting view");
    expectText(view(waiting(1, 59000), {}, {}, false, now + 1000).title,
               pick("Server unavailable · retrying in 0:58", "Сервер недоступен · повтор через 0:58"), "countdown ticks");
    expect(!view(waiting(1, 59000)).retryNow, "no capability, no Retry now");

    p = view(waiting(1, 59000, true), {}, pick("The access key was rejected.", "Ключ доступа отклонён."));
    expectText(p.detail, pick("The access key was rejected.\nInternet access is paused until the VPN connects.",
                              "Ключ доступа отклонён.\nИнтернет приостановлен до подключения VPN."), "cause and held line");
    expect(p.holdDisconnect && p.disconnectVpn, "held waiting keeps Disconnect VPN");

    QJsonObject unknownTime = waiting(2, 1000);
    unknownTime.remove("NextAttemptAt");
    p = view(unknownTime);
    expectText(p.title, pick("Server unavailable", "Сервер недоступен"), "waiting without time");
    expect(p.problem && !p.countdown, "no countdown without time");

    p = view(attempting(1, 3, true));
    expectText(p.detail, pick("Internet access is paused until the VPN connects.", "Интернет приостановлен до подключения VPN."), "held attempt line");
    p = view({{"State", "connecting"}, {"KillSwitch", "holding"}});
    expect(!p.detail.isEmpty() && p.holdDisconnect && p.disconnectVpn, "held initial connect");

    const QJsonObject heldFailure{{"State", "failed"}, {"ReconnectAttempt", 3}, {"ReconnectLimit", 3},
                                  {"KillSwitch", "holding"}, {"Session", QJsonObject{{"active", true}}}};
    p = view(heldFailure);
    expectText(p.title, pick("Couldn't connect. Check your internet connection and access key. Internet access stays paused until you select Disconnect VPN.",
                             "Не удалось подключиться. Проверьте подключение к интернету и ключ доступа. Интернет остаётся приостановленным, пока вы не нажмёте «Отключить VPN»."),
               "held failure title");
    expect(p.problem && p.holdDisconnect && p.disconnectVpn && p.detail.isEmpty() && !p.retryNow, "held failure view");
    expect(view(heldFailure, {{"reconnect_now", true}}).retryNow, "held failure can retry");
    QJsonObject released = heldFailure;
    released["Session"] = QJsonObject{{"active", false}};
    expect(!fromIpc(released, {}).protectionHeld && view(released).title.isEmpty(), "an ended run holds nothing");
    const QJsonObject newCore{{"kill_switch_hold", true}, {"reconnect_now", true}};
    expect(view({{"State", "failed"}}, newCore).title.isEmpty(), "plain failure keeps its label");
    expect(!view({{"State", "failed"}}, newCore).holdDisconnect, "plain failure offers Connect");
    expect(!fromIpc({{"State", "connected"}, {"KillSwitch", "active"}}, newCore).protectionHeld, "active protection is not held");

    const QJsonObject residual{{"State", "failed"}, {"LastError", "cleanup_failed_run_recover"},
                               {"LastErrorPermanent", true}, {"KillSwitch", "residual"}};
    p = view(residual, {{"reconnect_now", true}});
    expectText(p.title, pick("Internet is blocked by leftover VPN protection.", "Интернет заблокирован оставшейся защитой VPN."),
               "residual text");
    expect(p.problem && !p.retryNow && p.detail.isEmpty() && !p.holdDisconnect && !p.disconnectVpn && !p.countdown,
           "residual: no Retry now, no paused line, no held Disconnect VPN");
    expectText(residualProtectionText(), p.title, "one residual text");

    for (const char *state : {"connected", "disconnected", "stopping"}) {
        QJsonObject status = waiting(1, 1000, true);
        status["State"] = state;
        const Presentation idle = view(status);
        expect(idle.title.isEmpty() && idle.detail.isEmpty() && !idle.holdDisconnect && !idle.problem, "other states are untouched");
    }
}

inline void notifications(bool russian) {
    const auto pick = [russian](const char *en, const char *ru) { return QString::fromUtf8(russian ? ru : en); };
    Notifications tray;
    expect(tray.update("connected", {}, false).isEmpty(), "first connect is not a recovery");
    expectText(tray.update("reconnecting", fromIpc(attempting(1, 1), {}), false),
               pick("VPN connection lost. Reconnecting…", "VPN-соединение прервано. Переподключение…"), "first cycle");
    expect(tray.update("reconnecting", fromIpc(attempting(1, 2), {}), false).isEmpty(), "one message per cycle");
    expect(tray.update("reconnecting", fromIpc(waiting(1, 59000), {}), false).isEmpty(), "pause is the same cycle");
    expectText(tray.update("reconnecting", fromIpc(attempting(2, 1), {}), false),
               pick("VPN is still not connected. Retrying…", "VPN всё ещё не подключён. Повторяем попытки…"), "next cycle");
    expect(tray.recovering(), "recovery pending");
    expectText(tray.update("connected", {}, false), pick("VPN reconnected", "VPN переподключён"), "recovered");
    expect(!tray.recovering() && tray.update("connected", {}, false).isEmpty(), "recovery reported once");

    expectText(tray.update("reconnecting", fromIpc(attempting(3, 1), {}), true),
               pick("Proxy is still not connected. Retrying…", "Прокси всё ещё не подключён. Повторяем попытки…"), "joined mid-session");
    expect(tray.update("stopping", {}, true).isEmpty() && !tray.recovering(), "stop resets");
    expect(tray.update("connected", {}, true).isEmpty(), "connect after stop is not a recovery");
    expectText(tray.update("reconnecting", fromIpc(attempting(1, 1), {}), true),
               pick("Proxy connection lost. Reconnecting…", "Соединение прокси прервано. Переподключение…"), "proxy first cycle");
    expectText(tray.update("connected", {}, true), pick("Proxy reconnected", "Прокси переподключён"), "proxy recovered");

    expect(tray.update("reconnecting", fromIpc({{"State", "reconnecting"}, {"ReconnectAttempt", 1}, {"ReconnectLimit", 3}}, {}), false).isEmpty(),
           "legacy Core has no cycle messages");
    expect(tray.update("connected", {}, false).isEmpty(), "legacy recovery keeps the old notification");
}

inline void translationsComplete(const QJsonObject &ru) {
    for (const char *source : {
             "Connecting… (%1/%2)", "Reconnecting… (%1/%2)", "Server unavailable · retrying in %1",
             "Server unavailable", "Internet access is paused until the VPN connects.",
             "Couldn't connect. Check your internet connection and access key. Internet access stays paused until you select Disconnect VPN.",
             "Retry now", "Disconnect VPN", "Internet will work without VPN protection.",
             "VPN connection lost. Reconnecting…", "VPN is still not connected. Retrying…", "VPN reconnected",
             "Proxy connection lost. Reconnecting…", "Proxy is still not connected. Retrying…", "Proxy reconnected",
             "Internet is blocked by leftover VPN protection."}) {
        if (ru.value(QString::fromUtf8(source)).toString().isEmpty()) {
            std::fprintf(stderr, "FAIL: missing Russian translation: %s\n", source);
            ++failures;
        }
    }
}

inline void run() {
    QFile file(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json"));
    if (!file.open(QIODevice::ReadOnly)) { std::fprintf(stderr, "FAIL: ru.json\n"); ++failures; return; }
    JsonTranslator translator;
    translator.strings = QJsonDocument::fromJson(file.readAll()).object();
    mapping();
    countdownText();
    translationsComplete(translator.strings);
    views(false);
    notifications(false);
    QCoreApplication::installTranslator(&translator);
    views(true);
    notifications(true);
    QCoreApplication::removeTranslator(&translator);
}

} // namespace ReconnectPolicyTest
