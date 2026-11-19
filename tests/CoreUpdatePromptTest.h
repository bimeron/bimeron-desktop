#pragma once
#include "CoreUpdatePrompt.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTranslator>

#include <cstdio>

// Core update confirmation text with and without session-owned kill switch protection.
namespace CoreUpdatePromptTest {

inline int failures = 0;

inline void expect(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

inline void expectText(const QString &actual, const QString &expected, const char *message) {
    if (actual != expected) {
        std::fprintf(stderr, "FAIL: %s\n  expected: %s\n  actual:   %s\n", message, qPrintable(expected), qPrintable(actual));
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

inline const QJsonObject newCore{{"reconnect_now", true}, {"kill_switch_hold", true}};

inline CoreUpdatePrompt::Prompt promptFor(const QJsonObject &status, bool proxy = false, const QJsonObject &capabilities = newCore) {
    return CoreUpdatePrompt::prompt("0.4.1", status.value("State").toString(),
                                    ReconnectPolicy::fromIpc(status, capabilities), proxy);
}

inline void cases(bool russian) {
    const auto pick = [russian](const char *en, const char *ru) { return QString::fromUtf8(russian ? ru : en); };
    const QString protectedText = pick(
        "Installing Core 0.4.1 will interrupt the active VPN. Leak protection will be removed during the update, and internet traffic will bypass the VPN. Bimeron will reconnect the same profile after a successful update. Update now?",
        "Установка ядра 0.4.1 прервёт работу VPN. На время обновления защита от утечек будет снята, и интернет пойдёт без VPN. После успешного обновления Bimeron снова подключит тот же профиль. Обновить сейчас?");
    const QString vpnText = pick(
        "Installing Core 0.4.1 will interrupt the active VPN. Bimeron will reconnect the same profile after a successful update. Update now?",
        "Установка ядра 0.4.1 прервёт работу VPN. После успешного обновления Bimeron снова подключит тот же профиль. Обновить сейчас?");
    const QString proxyText = pick(
        "Installing Core 0.4.1 will interrupt the active proxy. Bimeron will reconnect the same profile after a successful update. Update now?",
        "Установка ядра 0.4.1 прервёт работу прокси. После успешного обновления Bimeron снова подключит тот же профиль. Обновить сейчас?");
    const QString idleText = pick("Core 0.4.1 is downloaded and verified. Update the service now?",
                                  "Ядро 0.4.1 загружено и проверено. Обновить службу сейчас?");
    const QJsonObject active{{"active", true}};
    const QJsonObject inactive{{"active", false}};

    auto p = promptFor({{"State", "connected"}, {"KillSwitch", "active"}, {"Session", active}});
    expectText(p.text, protectedText, "active kill switch warns");
    expect(p.reconnect, "active kill switch reconnects");

    p = promptFor({{"State", "reconnecting"}, {"KillSwitch", "holding"}, {"ReconnectCycle", 2},
                   {"ReconnectPhase", "waiting"}, {"Session", active}});
    expectText(p.text, protectedText, "held reconnecting warns");

    p = promptFor({{"State", "failed"}, {"KillSwitch", "holding"}, {"Session", active}, {"LastError", "transport_check_failed"}});
    expectText(p.text, protectedText, "held failed warns");
    expect(p.reconnect, "held failed counts as active and reconnects");

    p = promptFor({{"State", "connected"}, {"Session", active}});
    expectText(p.text, vpnText, "no kill switch: no warning");
    expect(p.reconnect, "no kill switch reconnects");

    p = promptFor({{"State", "connected"}, {"Session", QJsonObject{{"active", true}, {"mode", "proxy"}}}}, true);
    expectText(p.text, proxyText, "proxy mode: proxy wording, no warning");

    p = promptFor({{"State", "failed"}, {"KillSwitch", "residual"}, {"LastError", "cleanup_failed_run_recover"},
                   {"LastErrorPermanent", true}});
    expectText(p.text, idleText, "residual: no session owns protection");
    expect(!p.reconnect, "residual does not reconnect");

    p = promptFor({{"State", "failed"}, {"KillSwitch", "holding"}, {"Session", inactive}});
    expectText(p.text, idleText, "an ended failed run holds nothing");
    p = promptFor({{"State", "failed"}, {"Session", inactive}, {"LastError", "transport_check_failed"}});
    expectText(p.text, idleText, "plain failure stays idle");
    p = promptFor({{"State", "disconnected"}});
    expectText(p.text, idleText, "disconnected stays idle");
    expect(!p.reconnect, "disconnected does not reconnect");

    p = promptFor({{"State", "connected"}}, false, {});
    expectText(p.text, vpnText, "old Core keeps the 0.3.0 wording");
}

inline void run() {
    QFile file(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json"));
    if (!file.open(QIODevice::ReadOnly)) { std::fprintf(stderr, "FAIL: ru.json\n"); ++failures; return; }
    JsonTranslator translator;
    translator.strings = QJsonDocument::fromJson(file.readAll()).object();
    cases(false);
    QCoreApplication::installTranslator(&translator);
    cases(true);
    QCoreApplication::removeTranslator(&translator);
}

} // namespace CoreUpdatePromptTest
