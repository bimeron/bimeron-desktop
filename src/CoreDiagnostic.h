#pragma once

#include <QObject>
#include <QRegularExpression>

inline QString safeCoreDiagnostic(QByteArray output) {
    QString text = QString::fromUtf8(output).trimmed();
    text.replace(QRegularExpression(R"(((?:ss(?:conf)?|socks5|vless|hysteria2|hy2)://)[^\s\"']+)"), QStringLiteral("\\1") + QObject::tr("[hidden]"));
    text.replace(QRegularExpression(R"((?im)^\s*(Password|Key|SS|SSConf|SOCKS5|VLESS|Hysteria2|ID|PublicKey|ShortID)\s*=.*$)"), QStringLiteral("\\1 = ") + QObject::tr("[hidden]"));
    text.replace("error: [Interface] IPv6 = disable requires Mode = vpn",
                 QObject::tr("IPv6 can only be disabled in system VPN mode."));
    text.replace("error: KillSwitch requires Mode = vpn",
                 QObject::tr("The kill switch is only available in system VPN mode."));
    return text.left(1000);
}
