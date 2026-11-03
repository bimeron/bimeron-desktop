#pragma once
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QSet>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>

inline QString routingGroupIdentity(const QString &id) { return id.trimmed().toLower(); }
inline bool routingHasSection(const QString &text, const QString &name) {
    for (QString line : text.split('\n')) {
        line = line.trimmed();
        if (line.startsWith(QChar::ByteOrderMark)) line.remove(0, 1);
        line = line.trimmed();
        if (line.startsWith('[') && line.endsWith(']')
            && line.mid(1, line.size() - 2).trimmed() == name) return true;
    }
    return false;
}
inline bool routingGroupContains(const QStringList &ids, const QString &id) {
    for (const auto &candidate : ids)
        if (routingGroupIdentity(candidate) == routingGroupIdentity(id)) return true;
    return false;
}
inline QSet<QString> routingGroupSet(const QStringList &ids) {
    QSet<QString> result;
    for (const auto &id : ids) result.insert(routingGroupIdentity(id));
    return result;
}
inline QStringList routingGroupRows(const QStringList &available, const QStringList &selected) {
    QStringList result;
    QSet<QString> seen;
    for (const auto &id : available + selected) {
        const auto key = routingGroupIdentity(id);
        if (!seen.contains(key)) { seen.insert(key); result.append(id); }
    }
    return result;
}
inline QStringList routingRetainedGroups(QStringList checked, const QStringList &original) {
    QStringList result;
    auto remaining = routingGroupSet(checked);
    for (const auto &id : original)
        if (remaining.remove(routingGroupIdentity(id))) result.append(id);
    checked.sort();
    for (const auto &id : checked)
        if (remaining.remove(routingGroupIdentity(id))) result.append(id);
    return result;
}

// Decode the JSON-compatible quoted values emitted by Core section patches.
// Unsupported Go escape syntax remains unavailable to host interpretation.
inline QString routingInterfaceValue(const QString &text, const QString &key, const QString &fallback = {}) {
    QString section;
    for (QString line : text.split('\n')) {
        line = line.trimmed();
        if (line.startsWith(QChar::ByteOrderMark)) line.remove(0, 1);
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) continue;
        if (line.startsWith('[') && line.endsWith(']')) { section = line.mid(1, line.size() - 2).trimmed(); continue; }
        const int eq = line.indexOf('=');
        if (section != "Interface" || eq < 0 || line.left(eq).trimmed() != key) continue;
        const QString value = line.mid(eq + 1).trimmed();
        if (!value.startsWith('"')) return value;
        QJsonParseError error;
        const auto parsed = QJsonDocument::fromJson(('[' + value + ']').toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && parsed.isArray() && parsed.array().size() == 1 && parsed.array().at(0).isString()) return parsed.array().at(0).toString();
        return fallback;
    }
    return fallback;
}

// Conservative host gate; Core independently validates executable policy.
inline QString routingCompatibilityError(const QString &text, const QJsonObject &caps) {
    QString section, mode = "vpn";
    QStringList matchers;
    bool kill = false, ikev2 = false;
    QSet<QString> interfaceFields;
    for (QString line : text.split('\n')) {
        line = line.trimmed();
        if (line.startsWith(QChar::ByteOrderMark)) line.remove(0, 1);
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2).trimmed();
            if (section == "IKEv2") ikev2 = true;
            continue;
        }
        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed();
        QString value = line.mid(eq + 1).trimmed();
        if (section == "Interface" && (key == "Mode" || key == "KillSwitch")) {
            if (interfaceFields.contains(key))
                return QObject::tr("Duplicate routing protection setting. Correct the profile in source view.");
            interfaceFields.insert(key);
            // Core owns Go escape decoding; ambiguous host policy must fail closed.
            if (value.startsWith('"')) {
                if (value.size() < 2 || !value.endsWith('"') || value.contains('\\'))
                    return QObject::tr("Ambiguous routing protection setting. Correct the profile in source view.");
                value = value.mid(1, value.size() - 2);
            }
            if ((key == "Mode" && value != "vpn" && value != "proxy")
                || (key == "KillSwitch" && value != "true" && value != "false"))
                return QObject::tr("Invalid routing protection setting. Correct the profile in source view.");
        }
        if (section == "Interface" && key == "Mode") mode = value;
        if (section == "Interface" && key == "KillSwitch" && value == "true") kill = true;
        if (section == "Rule" && QStringList{"CIDR", "Domain", "GeoIP", "GeoSite"}.contains(key)) matchers.append(key);
    }
    if (ikev2 && !matchers.isEmpty()) return QObject::tr("IKEv2 does not support split routing. Correct the profile or keep the rules disabled.");
    for (const QString &matcher : matchers) {
        if (kill && matcher != "CIDR") return QObject::tr("Kill switch permits only CIDR rules. Disable incompatible rules explicitly or change protection explicitly.");
        const QString flag = matcher == "CIDR" ? "ip_split" : matcher == "Domain" ? "domain_split" : matcher == "GeoIP" ? "geoip_split" : "geosite_split";
        if (!caps.value(flag).toBool()) return QObject::tr("The active Core has not advertised support for %1 rules.").arg(matcher);
        if ((matcher == "GeoIP" || matcher == "GeoSite") && caps.value("geodata_schema").toString() != "v2ray-protobuf")
            return QObject::tr("The active Core has not advertised the supported GeoData schema (v2ray-protobuf).");
        if (matcher == "GeoSite" && mode == "vpn" && !caps.value("domain_split").toBool())
            return QObject::tr("GeoSite in system VPN requires domain-routing support.");
    }
    return {};
}

// Structured Core errors are data, never control flow inferred from prose.
inline QString geoDataValidationError(const QByteArray &output) {
    if (output.size() > 65536) return QObject::tr("Core validation response exceeded its limit.");
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(output, &parse);
    const auto result = document.object();
    if (parse.error != QJsonParseError::NoError || !document.isObject()
        || result.value("version").toInt() != 1 || (result.contains("error") && !result.value("error").isString()))
        return QObject::tr("The selected Core does not provide structured GeoData validation.");
    if (result.value("error").toString().isEmpty()) return {};
    const auto details = result.value("details").toObject();
    QString message = QObject::tr("Core rejected the configuration. Correct the profile before applying it.");
    const QString code = details.value("code").toString();
    if (QRegularExpression("^geodata_[a-z_]+$").match(code).hasMatch()) {
        message += "\n" + code;
        const int rule = details.value("rule").toInt();
        if (rule > 0) message += "\n" + QObject::tr("Active rule %1 (disabled rows are not counted).").arg(rule);
        const QString group = details.value("group").toString();
        if (QRegularExpression("^[a-zA-Z0-9_-]{1,256}$").match(group).hasMatch()) message += "\n" + QObject::tr("Group: %1").arg(group);
    }
    return message;
}
