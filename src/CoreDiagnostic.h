#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

// Free text from any CLI/IPC version can contain credentials, even without a URI
// scheme or a recognized field name. Only complete safe templates are retained.
inline QString safeCoreDiagnostic(const QByteArray &output) {
    const QString text = QString::fromUtf8(output).trimmed();
    if (text.isEmpty()) return {};
    if (text == "error: [Interface] IPv6 = disable requires Mode = vpn")
        return QObject::tr("IPv6 can only be disabled in system VPN mode.");
    if (text == "error: KillSwitch requires Mode = vpn")
        return QObject::tr("The kill switch is only available in system VPN mode.");
    const auto line = QRegularExpression(
        QStringLiteral("^error: (invalid configuration|duplicate section|duplicate option|unknown section|unknown option) at line ([0-9]{1,5})$")).match(text);
    if (line.hasMatch())
        return QObject::tr("Invalid configuration at line %1.").arg(line.captured(2));
    return QObject::tr("Core diagnostic details were hidden because they may contain credentials.");
}

inline QString safeCoreCode(const QString &code) {
    if (code.isEmpty()) return {};
    static const QSet<QString> known{
        "unsupported_protocol", "absolute_config_path_required", "invalid_configuration",
        "session_already_running", "session_start_failed", "session_stop_failed",
        "daemon_shutting_down", "access_key_unavailable", "invalid_auth_challenge",
        "invalid_auth_response", "unknown_command", "ikev2_requires_foreground_user_session", "state"
    };
    return known.contains(code) ? code : QStringLiteral("redacted_error");
}

inline QString safeCoreEvent(const QString &code, const QString &message) {
    if (code == "state" && QStringList{"disconnected", "connecting", "connected", "reconnecting", "stopping", "failed"}.contains(message))
        return QStringLiteral("state: ") + message;
    return code.isEmpty() ? safeCoreDiagnostic(message.toUtf8()) : safeCoreCode(code);
}

// Defense at the shared log sink for application messages interpolating URIs or
// config fragments. Untrusted Core free text must use safeCoreDiagnostic first.
inline QString safeLogMessage(const QString &message) {
    if (message.contains(QRegularExpression(QStringLiteral("[a-zA-Z][a-zA-Z0-9+.-]*://")))
        || message.contains(QRegularExpression(QStringLiteral("(?i)(password|passphrase|answers|authorization|token|secret|key|ID|XHTTP[^ ]*)\\s*[=:]"))))
        return safeCoreDiagnostic(message.toUtf8());
    return message;
}

inline QByteArray safeDiagnosticReport(const QByteArray &data) {
    if (data.size() > 256 * 1024) return {};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    const auto source = document.object();
    if (source.value("schema").toInt() != 1) return {};
    QJsonObject result{{"schema", 1}};
    const auto copyEnum = [](const QJsonObject &from, QJsonObject &to, const QString &key, const QStringList &values) {
        const QString value = from.value(key).toString();
        if (values.contains(value)) to.insert(key, value);
    };
    copyEnum(source, result, "platform", {"windows", "linux", "darwin"});
    copyEnum(source, result, "architecture", {"amd64", "arm64"});
    if (source.value("protocol").toInt() == 1) result.insert("protocol", 1);
    const auto copyBuild = [](const QJsonObject &from) {
        QJsonObject to;
        const QRegularExpression version(QStringLiteral("^v?[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}(?:-(?:dev|(?:alpha|beta|rc)\\.[0-9]{1,4}))?$"));
        const QRegularExpression revision(QStringLiteral("^[0-9a-f]{40}$"));
        if (version.match(from.value("version").toString()).hasMatch()) to.insert("version", from.value("version"));
        if (revision.match(from.value("revision").toString()).hasMatch()) to.insert("revision", from.value("revision"));
        if (from.value("modified").isBool()) to.insert("modified", from.value("modified"));
        return to;
    };
    if (source.value("core").isObject()) result.insert("core", copyBuild(source.value("core").toObject()));
    QJsonObject cli = copyBuild(QJsonObject{{"revision", source.value("cli_revision")}, {"modified", source.value("cli_modified")}});
    if (cli.contains("revision")) result.insert("cli_revision", cli.value("revision"));
    if (cli.contains("modified")) result.insert("cli_modified", cli.value("modified"));
    QJsonArray checks;
    for (const auto &value : source.value("checks").toArray()) {
        if (checks.size() >= 32) break;
        const auto from = value.toObject();
        QJsonObject to;
        copyEnum(from, to, "name", {"local_control", "session_stats", "recent_events", "system_network_inspection",
            "windows_service", "tunnel_adapters", "tunnel_adapters_up", "captured_ipv4_routes",
            "captured_ipv6_routes", "owned_dns_rules", "owned_tunnel_dns_rules", "kill_switch_filters"});
        copyEnum(from, to, "result", {"available", "unavailable", "unavailable_or_restarted", "unavailable_or_permission_denied",
            "observed", "requires_elevation", "absent", "present", "not_implemented_on_this_platform",
            "not_installed", "stopped", "running", "startpending", "stoppending", "paused"});
        if (!to.contains("name") || !to.contains("result")) continue;
        if (from.value("count").isDouble()) to.insert("count", from.value("count").toInteger());
        checks.append(to);
    }
    result.insert("checks", checks);
    QJsonObject stats;
    const auto fromStats = source.value("stats").toObject();
    for (const QString &key : {"tcp_total", "tcp_active", "tcp_failed", "udp_total", "udp_active", "udp_failed",
            "dns_queries", "dns_failed", "bytes_up", "bytes_down", "routed_proxy", "routed_direct", "routed_block"})
        if (fromStats.value(key).isDouble()) stats.insert(key, fromStats.value(key).toInteger());
    if (fromStats.value("unavailable").isBool()) stats.insert("unavailable", fromStats.value("unavailable"));
    if (!stats.isEmpty()) result.insert("stats", stats);
    const auto cleanStatus = [&copyEnum](const QJsonObject &from) {
        QJsonObject to;
        copyEnum(from, to, "state", {"disconnected", "connecting", "connected", "reconnecting", "stopping", "failed"});
        if (from.value("attempt").isDouble()) to.insert("attempt", from.value("attempt").toInt());
        if (from.contains("last_error")) to.insert("last_error", safeCoreCode(from.value("last_error").toString()));
        return to;
    };
    if (source.value("status").isObject()) result.insert("status", cleanStatus(source.value("status").toObject()));
    QJsonArray events;
    for (const auto &value : source.value("events").toArray()) {
        if (events.size() >= 20) break;
        const auto from = value.toObject();
        auto to = cleanStatus(from);
        to.insert("code", safeCoreCode(from.value("code").toString()));
        if (from.contains("error")) to.insert("error", safeCoreCode(from.value("error").toString()));
        events.append(to);
    }
    result.insert("events", events);
    // No arbitrary string, unknown object, auth response, or profile is exported.
    return QJsonDocument(result).toJson();
}
