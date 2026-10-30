#include "NativeIKEv2Transport.h"
#include "TrustedCore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QStringDecoder>
#include <QThread>
#include <QUuid>
#include <atomic>
#include <vector>
#if defined(Q_OS_WIN) && defined(NATIVE_IKEV2_TRANSPORT_TESTING)
#include <QFile>
#include <QTemporaryDir>
#include <thread>
#endif

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

namespace {
using Transport = NativeIKEv2Transport;
bool hex(const QString &s, int length) {
    if (s.size() != length) return false;
    for (QChar c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
bool keys(const QJsonObject &o, const QStringList &allowed) {
    for (auto i = o.begin(); i != o.end(); ++i) if (!allowed.contains(i.key())) return false;
    return true;
}
bool text(const QJsonValue &v, int maximum = 1024) {
    return v.isString() && v.toString().size() <= maximum && !v.toString().contains(QChar(0));
}
bool integer(const QJsonValue &v, double maximum = 9007199254740991.0) {
    if (!v.isDouble()) return false;
    const double d = v.toDouble();
    return d >= 0 && d <= maximum && d == double(quint64(d));
}
bool decimal(const QJsonValue &v, quint64 *result) {
    if (!text(v, 20) || v.toString().isEmpty()) return false;
    const QString s = v.toString();
    for (auto c : s) if (c < '0' || c > '9') return false;
    if (s.size() > 1 && s.front() == '0') return false;
    bool ok = false;
    *result = s.toULongLong(&ok);
    return ok;
}

// Qt's object parser overwrites duplicate keys. Scan every object before using it,
// decoding escaped keys as JSON strings so "id" and "\u0069d" collide.
class UniqueJSON {
    const QByteArray &data;
    qsizetype at = 0;
    void whitespace() { while (at < data.size() && QByteArray(" \t\r\n").contains(data[at])) ++at; }
    bool string(QString *decoded = nullptr) {
        if (at >= data.size() || data[at] != '"') return false;
        const qsizetype begin = at++;
        bool escaped = false;
        while (at < data.size()) {
            const char c = data[at++];
            if (!escaped && c == '"') {
                const auto d = QJsonDocument::fromJson("[" + data.mid(begin, at - begin) + "]");
                if (!d.isArray() || !d.array().first().isString()) return false;
                if (decoded) *decoded = d.array().first().toString();
                return true;
            }
            if (!escaped && c == '\\') escaped = true; else escaped = false;
        }
        return false;
    }
    bool value(int depth) {
        whitespace();
        if (depth > 32 || at >= data.size()) return false;
        const char c = data[at];
        if (c == '"') return string();
        if (c == '{' || c == '[') {
            ++at;
            const char close = c == '{' ? '}' : ']';
            QSet<QString> seen;
            whitespace();
            if (at < data.size() && data[at] == close) { ++at; return true; }
            while (at < data.size()) {
                if (c == '{') {
                    QString key;
                    whitespace();
                    if (!string(&key) || seen.contains(key)) return false;
                    seen.insert(key);
                    whitespace();
                    if (at >= data.size() || data[at++] != ':') return false;
                }
                if (!value(depth + 1)) return false;
                whitespace();
                if (at >= data.size()) return false;
                if (data[at] == close) { ++at; return true; }
                if (data[at++] != ',') return false;
            }
            return false;
        }
        const auto begin = at;
        while (at < data.size() && !QByteArray(",]} \t\r\n").contains(data[at])) ++at;
        const auto token = data.mid(begin, at - begin);
        if (token == "true" || token == "false" || token == "null") return true;
        // This protocol has integer fields only. Reject fractional/exponent
        // lexemes before Qt can round them into an apparently valid integer.
        if (token.isEmpty() || (token.size() > 1 && token[0] == '0')) return false;
        for (char digit : token) if (digit < '0' || digit > '9') return false;
        return true;
    }
public:
    explicit UniqueJSON(const QByteArray &input) : data(input) {}
    bool valid() {
        QStringDecoder decoder(QStringDecoder::Utf8);
        decoder.decode(data);
        if (decoder.hasError()) return false;
        whitespace();
        if (at >= data.size() || data[at] != '{' || !value(0)) return false;
        whitespace();
        return at == data.size();
    }
};
bool parse(const QByteArray &data, QJsonObject *out) {
    if (!UniqueJSON(data).valid()) return false;
    QJsonParseError error;
    const auto d = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !d.isObject()) return false;
    *out = d.object();
    return true;
}
QString safeError(const QString &code) {
    static const QStringList allowed = {
        "installation_mismatch", "foreground_installation_policy_rejected", "stale_instance",
        "cleanup_pending", "session_not_terminal", "owner_closing", "activation_cancelled",
        "foreground_identity_rejected", "foreground_owner_unavailable", "foreground_configuration_rejected",
        "foreground_configuration_changed", "core_identity_mismatch", "invalid_request", "unsupported_protocol",
        "unknown_command", "invalid_cursor", "session_start_failed", "session_stop_failed", "response_too_large",
        "operation_pending", "ikev2_profile_name_not_owned", "ikev2_profile_not_owned",
        "ikev2_profile_manifest_conflict", "ikev2_profile_incomplete_transaction_manual_review_required",
        "ikev2_owned_profile_modified_or_missing", "ikev2_existing_ras_connection", "ikev2_profile_preview_required",
        "ikev2_profile_preview_changed", "ikev2_profile_rollback_incomplete_manual_review_required",
        "ikev2_profile_rollback_manifest_cleanup_failed", "ikev2_profile_creation_failed_rolled_back",
        "ikev2_profile_removed_manifest_cleanup_failed", "ikev2_profile_journal_write_failed",
        "ikev2_saved_credentials_required", "ikev2_saved_eap_identity_required", "ikev2_windows_profile_not_found",
        "ikev2_profile_identity_changed", "ikev2_profile_operation_failed"};
    return code.isEmpty() || allowed.contains(code) ? code : QStringLiteral("foreground_operation_failed");
}
bool identity(const QJsonObject &o, Transport::Identity *i) {
    if (o.size() != 8 || !keys(o, {"pid", "created", "sid", "session", "image_sha256", "image_volume", "image_index", "elevated"}) ||
        !integer(o.value("pid"), 0xffffffff) || !integer(o.value("session"), 0xffffffff) ||
        !integer(o.value("image_volume"), 0xffffffff) || !decimal(o.value("created"), &i->created) ||
        !decimal(o.value("image_index"), &i->imageIndex) || !text(o.value("sid"), 184) ||
        !hex(o.value("image_sha256").toString(), 64) || o.value("elevated") != QJsonValue(true)) return false;
    i->pid = quint32(o.value("pid").toDouble()); i->session = quint32(o.value("session").toDouble());
    i->imageVolume = quint32(o.value("image_volume").toDouble());
    i->sid = o.value("sid").toString(); i->imageSha256 = o.value("image_sha256").toString(); i->elevated = true;
    return i->pid && i->session && i->created && !i->sid.isEmpty();
}

// Closed schemas for the immutable Core API. Optional fields may be absent;
// present fields always have their declared type. Free diagnostic text is removed.
bool fields(const QJsonObject &o, const QStringList &strings, const QStringList &booleans = {},
            const QStringList &numbers = {}, const QStringList &objects = {}) {
    if (!keys(o, strings + booleans + numbers + objects)) return false;
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (strings.contains(it.key()) && !text(it.value(), 4096)) return false;
        if (booleans.contains(it.key()) && !it.value().isBool()) return false;
        if (numbers.contains(it.key()) && !integer(it.value())) return false;
        if (objects.contains(it.key()) && !it.value().isObject()) return false;
    }
    return true;
}
bool payload(QJsonObject *o) {
    if (!o->value("status").isObject()) return false;
    auto s = o->value("status").toObject();
    if (!fields(s, {"State", "Since", "LastError", "ErrorStage"}, {}, {"Attempt"}, {"Session", "native"})) return false;
    // The response_too_large fallback serializes a zero-valued Status.
    for (const auto &key : {"State", "Attempt", "Since", "LastError"}) if (!s.contains(key)) return false;
    if (!QStringList{"", "connecting", "connected", "stopping", "disconnected", "failed"}.contains(s.value("State").toString()) ||
        (s.value("State") == "" && o->value("error") != "response_too_large")) return false;
    if (s.contains("Session")) {
        const auto session = s.value("Session").toObject();
        if (!fields(session, {"backend", "id", "profile_id", "profile_name", "mode", "ipv6", "server_tcp", "server_udp", "server_tcp_name", "server_udp_name"},
                    {"stats_unavailable", "active", "kill_switch_requested"}) ||
            !hex(session.value("id").toString(), 32) || session.value("backend") != "ikev2" ||
            session.value("stats_unavailable") != QJsonValue(true) || !session.value("active").isBool() ||
            !session.value("kill_switch_requested").isBool() || session.value("mode") != "vpn" || session.value("ipv6") != "system") return false;
    }
    if (s.contains("native")) {
        const auto n = s.value("native").toObject();
        if (n.size() != 10 || !fields(n,
            {"profile_readiness", "credential_readiness", "certificate_readiness", "wfp", "dns", "ras_cleanup", "dns_cleanup", "wfp_cleanup"},
            {"requested", "recovery_required"})) return false;
        for (const auto &key : {"wfp", "dns"})
            if (!QStringList{"inactive", "installing", "active", "retained", "modified", "unknown"}.contains(n.value(key).toString())) return false;
        for (const auto &key : {"ras_cleanup", "dns_cleanup", "wfp_cleanup"})
            if (!QStringList{"not_started", "pending", "succeeded", "failed"}.contains(n.value(key).toString())) return false;
        if (!QStringList{"not_checked", "missing", "validated"}.contains(n.value("profile_readiness").toString()) ||
            !QStringList{"not_checked", "missing", "usable"}.contains(n.value("credential_readiness").toString()) ||
            !QStringList{"not_checked", "authentication_failed", "windows_authenticated"}.contains(n.value("certificate_readiness").toString())) return false;
    }
    // Lifecycle consumers use structured fields, never elevated diagnostic text.
    s.remove("LastError"); s.remove("ErrorStage"); o->insert("status", s);
    if (o->contains("capabilities")) {
        const auto c = o->value("capabilities").toObject();
        if (!o->value("capabilities").isObject() || c.size() != 11 || !fields(c, {"platform"},
            {"vpn", "proxy", "ipv4", "ipv6", "ip_split", "domain_split", "app_split", "kill_switch", "daemon"}, {}, {"ikev2"}) ||
            c.value("platform") != "windows" || !c.value("ikev2").isObject() || c.value("ikev2").toObject().size() != 10 ||
            !fields(c.value("ikev2").toObject(), {}, {"administrator_required", "supported", "foreground_only", "preconfigured_profile", "proxy", "ip_split", "domain_split", "kill_switch", "dns_protection", "stats"})) return false;
    }
    if (o->contains("operation") && (!text(o->value("operation")) || !QStringList{"preview_pending", "previewing", "previewed", "applying", "succeeded", "failed"}.contains(o->value("operation").toString()))) return false;
    if (o->contains("profile")) {
        const auto p = o->value("profile").toObject();
        if (!o->value("profile").isObject() || !keys(p, {"state_token", "action", "windows_profile", "server_name", "authentication", "ipsec", "ipv6_routes", "credentials", "trust"})) return false;
        for (const auto &key : {"action", "windows_profile", "server_name", "authentication", "ipsec", "ipv6_routes", "credentials", "trust"}) if (!p.contains(key)) return false;
        if (!QStringList{"create", "unchanged", "remove", "removed", "created"}.contains(p.value("action").toString()) ||
            (o->value("operation") == "previewed" && !hex(p.value("state_token").toString(), 64))) return false;
        for (auto it = p.begin(); it != p.end(); ++it) {
            if (it.key() == "ipv6_routes") {
                if (!it.value().isArray() || it.value().toArray().size() > 16) return false;
                for (auto route : it.value().toArray()) if (!text(route, 128)) return false;
            } else if (!text(it.value(), 4096)) return false;
        }
        if (p.contains("state_token") && !hex(p.value("state_token").toString(), 64)) return false;
    }
    for (const auto &key : {"cursor", "source_drops"}) if (o->contains(key) && !integer(o->value(key))) return false;
    if (o->contains("dropped") && !o->value("dropped").isBool()) return false;
    if (o->contains("events")) {
        if (!o->value("events").isArray() || o->value("events").toArray().size() > 128) return false;
        QJsonArray events;
        for (auto entry : o->value("events").toArray()) {
            auto e = entry.toObject();
            if (!entry.isObject() || e.size() != 2 || !keys(e, {"sequence", "event"}) || !integer(e.value("sequence")) || !e.value("event").isObject()) return false;
            auto event = e.value("event").toObject();
            if (!fields(event, {"Time", "State", "Code", "Message", "LastError", "ErrorStage"}, {}, {"Attempt"})) return false;
            // core.Event at the approved revision omits only ErrorStage. Validate
            // diagnostics before removing them, including the zero-valued event.
            for (const auto &key : {"Time", "State", "Code", "Message", "Attempt", "LastError"})
                if (!event.contains(key)) return false;
            event.remove("Message"); event.remove("LastError"); event.remove("ErrorStage");
            if (event.contains("Code")) event.insert("Code", safeError(event.value("Code").toString()));
            e.insert("event", event); events.append(e);
        }
        o->insert("events", events);
    }
    return true;
}
}

bool NativeIKEv2Transport::approved(const Approval &a) {
    return !a.executable.isEmpty() && hex(a.sha256, 64) &&
        a.revision == "66d84d8f4dcfc6ada625eb11894b19feea51b7ba" &&
        (a.architecture == "amd64" || a.architecture == "arm64");
}
QString NativeIKEv2Transport::endpointName(const QString &sid, quint32 session) {
    if (sid.isEmpty() || !session) return {};
    return QStringLiteral("\\\\.\\pipe\\Bimeron.Core.IKEv2.%1.%2")
        .arg(QString::fromLatin1(QCryptographicHash::hash(sid.toUtf8(), QCryptographicHash::Sha256).left(16).toHex()))
        .arg(session);
}
QByteArray NativeIKEv2Transport::encodeRequest(Endpoint endpoint, const QString &command,
        const QString &id, const QString &installation, const QString &revision, const QJsonObject &f) {
    if (id.isEmpty() || id.toUtf8().size() > 64 || !hex(installation, 32) || !hex(revision, 40) ||
        !keys(f, {"instance", "session", "after", "accept_revision", "plan_token"})) return {};
    for (const auto &key : {"instance", "session", "accept_revision", "plan_token"})
        if (f.contains(key) && !text(f.value(key), 128)) return {};
    if ((f.contains("instance") && !hex(f.value("instance").toString(), 32)) ||
        (f.contains("session") && !f.value("session").toString().isEmpty() && !hex(f.value("session").toString(), 32))) return {};
    const bool hello = command == "hello";
    if (!hello && (f.value("instance").toString().isEmpty())) return {};
    if (endpoint == Endpoint::Session) {
        if (!QStringList{"hello", "status", "events", "stop", "close_owner"}.contains(command) || f.contains("plan_token")) return {};
        if (hello) {
            if (f.contains("session") || f.contains("after")) return {};
            const bool activation = f.contains("accept_revision");
            if (activation ? (f.value("instance").toString().isEmpty() || f.value("accept_revision") != revision) : f.contains("instance")) return {};
        } else if (!f.contains("session") || f.contains("accept_revision")) return {};
        if (f.contains("after") && (command != "events" || !integer(f.value("after")))) return {};
    } else {
        if (!QStringList{"hello", "status", "apply", "remove", "close_owner"}.contains(command) || f.contains("session") || f.contains("after")) return {};
        const bool confirmation = command == "apply" || command == "remove";
        if (confirmation) {
            if (f.value("accept_revision") != revision || !hex(f.value("plan_token").toString(), 64)) return {};
        } else if (f.contains("accept_revision") || f.contains("plan_token") || (hello && f.contains("instance"))) return {};
    }
    QJsonObject body = f;
    body.insert("version", 1); body.insert("id", id); body.insert("command", command); body.insert("installation", installation);
    const auto encoded = QJsonDocument(body).toJson(QJsonDocument::Compact) + '\n';
    return encoded.size() <= 16 * 1024 ? encoded : QByteArray{};
}

bool NativeIKEv2Transport::decodeResponse(const QByteArray &frame, const QString &id,
        const QString &installation, const Approval &approval, Endpoint endpoint,
        const Identity &peer, const QString &instance, QJsonObject *response) {
    if (!response || frame.size() > 256 * 1024 || !frame.endsWith('\n') || frame.count('\n') != 1) return false;
    QJsonObject o;
    if (!parse(frame, &o) || !keys(o, {"installation", "source_drops", "owner", "endpoint", "profile", "operation", "core", "id", "instance", "version", "status", "error", "events", "cursor", "dropped", "capabilities"}) ||
        o.value("version") != QJsonValue(1) || o.value("id") != id || o.value("installation") != installation ||
        !hex(o.value("instance").toString(), 32) || (!instance.isEmpty() && o.value("instance") != instance) ||
        o.value("endpoint") != (endpoint == Endpoint::Session ? "ikev2_foreground" : "ikev2_profile")) return false;
    Identity observed;
    if (!o.value("owner").isObject() || !identity(o.value("owner").toObject(), &observed) || observed != peer) return false;
    if (o.contains("error") && !text(o.value("error"))) return false;
    if (o.contains("core")) {
        const auto c = o.value("core").toObject();
        if (!o.value("core").isObject() || !keys(c, {"version", "revision", "commit_time", "modified", "go_version", "platform", "architecture"}) ||
            c.value("revision") != approval.revision || c.value("modified") != QJsonValue(false) ||
            c.value("platform") != "windows" || c.value("architecture") != approval.architecture ||
            !text(c.value("version")) || !text(c.value("go_version")) || (c.contains("commit_time") && !text(c.value("commit_time")))) return false;
    }
    if (!payload(&o)) return false;
    if (endpoint == Endpoint::Session && (o.contains("profile") || o.contains("operation"))) return false;
    if (endpoint == Endpoint::Provisioning && (o.contains("events") || o.contains("cursor") || o.contains("source_drops") || o.contains("dropped"))) return false;
    *response = o;
    if (o.contains("error")) response->insert("error", safeError(o.value("error").toString()));
    return true;
}

#ifdef Q_OS_WIN
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    Handle(Handle &&h) noexcept : value(h.value) { h.value = INVALID_HANDLE_VALUE; }
    Handle &operator=(Handle &&h) noexcept {
        if (this != &h) { reset(); value = h.value; h.value = INVALID_HANDLE_VALUE; }
        return *this;
    }
    ~Handle() { reset(); }
    void reset() { if (valid()) CloseHandle(value); value = INVALID_HANDLE_VALUE; }
    bool valid() const { return value && value != INVALID_HANDLE_VALUE; }
};
struct Pins {
    Handle process;
    std::vector<Handle> files;
    Transport::Identity identity;
};
std::wstring native(const QString &s) { return QDir::toNativeSeparators(s).toStdWString(); }
QString sidString(PSID sid) {
    LPWSTR text = nullptr;
    if (!sid || !ConvertSidToStringSidW(sid, &text)) return {};
    const QString result = QString::fromWCharArray(text); LocalFree(text); return result;
}
bool trustedSid(PSID sid) {
    const auto s = sidString(sid);
    return s == "S-1-5-18" || s == "S-1-5-32-544" ||
        s == "S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";
}
bool verifyDescriptor(PSECURITY_DESCRIPTOR sd) {
    PSID owner = nullptr; PACL acl = nullptr; BOOL defaulted = FALSE, present = FALSE;
    if (!sd || !IsValidSecurityDescriptor(sd) || !GetSecurityDescriptorOwner(sd, &owner, &defaulted) ||
        !GetSecurityDescriptorDacl(sd, &present, &acl, &defaulted)) return false;
    bool valid = owner && trustedSid(owner) && present && acl && IsValidAcl(acl);
    for (DWORD i = 0; valid && i < acl->AceCount; ++i) {
        void *entry = nullptr;
        if (!GetAce(acl, i, &entry)) { valid = false; break; }
        const auto *header = static_cast<ACE_HEADER *>(entry);
        if (header->AceFlags & INHERIT_ONLY_ACE) continue;
        if (header->AceType == ACCESS_DENIED_ACE_TYPE) continue;
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE || header->AceSize < sizeof(ACCESS_ALLOWED_ACE)) { valid = false; break; }
        const auto *allow = static_cast<ACCESS_ALLOWED_ACE *>(entry);
        // Identical conservative effective-write mask to reviewed Core. Deny ACEs
        // never make an otherwise unsafe allow acceptable; unfamiliar forms fail.
        if ((allow->Mask & 0x500d0156u) && !trustedSid(const_cast<DWORD *>(&allow->SidStart))) valid = false;
    }
    return valid;
}
bool protectedHandle(HANDLE h) {
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (GetSecurityInfo(h, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                       nullptr, nullptr, nullptr, nullptr, &sd) != ERROR_SUCCESS) return false;
    const bool valid = verifyDescriptor(sd);
    LocalFree(sd); return valid;
}
Handle openLocked(const QString &path, bool directory, bool protect) {
    Handle h(CreateFileW(native(path).c_str(), GENERIC_READ | READ_CONTROL, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0), nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    if (!h.valid() || GetFileType(h.value) != FILE_TYPE_DISK || !GetFileInformationByHandle(h.value, &info) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
        bool(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != directory || (protect && !protectedHandle(h.value))) h.reset();
    return h;
}
bool localPath(const QString &path) {
    if (path.size() < 4 || !((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) || path[1] != ':' || (path[2] != '/' && path[2] != '\\') ||
        path.mid(2).contains(':') || path.contains(QChar(0))) return false;
    const QString normalized = QDir::fromNativeSeparators(path);
    // Reject lexical aliases before pinning: normalizing '..' before an OS open
    // could otherwise hide a junction traversed by ShellExecute/Core later.
    if (normalized != QDir::cleanPath(normalized)) return false;
    for (const auto &part : normalized.mid(3).split('/'))
        if (part.isEmpty() || part.endsWith('.') || part.endsWith(' ')) return false;
    const QString root = path.left(3);
    return GetDriveTypeW(native(root).c_str()) == DRIVE_FIXED;
}
bool lockPath(const QString &path, bool protect, std::vector<Handle> *files) {
    if (!localPath(path)) return false;
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    QString root = clean.left(3);
    if (protect) {
        PWSTR folder = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &folder))) return false;
        root = QDir::cleanPath(QDir::fromNativeSeparators(QString::fromWCharArray(folder)));
        CoTaskMemFree(folder);
        if (!clean.startsWith(root + '/', Qt::CaseInsensitive)) return false;
    }
    auto rootHandle = openLocked(root, true, protect);
    if (!rootHandle.valid()) return false;
    files->push_back(std::move(rootHandle));
    QString current = root;
    const auto components = clean.mid(root.size()).split('/', Qt::SkipEmptyParts);
    if (components.isEmpty()) return false;
    for (qsizetype i = 0; i < components.size(); ++i) {
        current = QDir(current).filePath(components[i]);
        auto h = openLocked(current, i + 1 < components.size(), protect);
        if (!h.valid()) return false;
        files->push_back(std::move(h));
    }
    return true;
}
QByteArray readFile(HANDLE h, qint64 maximum, const std::atomic_bool &cancel) {
    LARGE_INTEGER size{}, zero{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > maximum || !SetFilePointerEx(h, zero, nullptr, FILE_BEGIN)) return {};
    QByteArray bytes;
    char buffer[65536];
    while (!cancel.load()) {
        DWORD read = 0;
        if (!ReadFile(h, buffer, sizeof(buffer), &read, nullptr)) return {};
        if (!read) return bytes;
        bytes.append(buffer, read);
        if (bytes.size() > maximum) return {};
    }
    return {};
}
QString hashImage(HANDLE h, const std::atomic_bool &cancel) {
    LARGE_INTEGER size{}, zero{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || size.QuadPart > 128 * 1024 * 1024 ||
        !SetFilePointerEx(h, zero, nullptr, FILE_BEGIN)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;
    char chunk[65536];
    while (!cancel.load()) {
        DWORD read = 0;
        if (!ReadFile(h, chunk, sizeof(chunk), &read, nullptr)) return {};
        if (!read) return total == size.QuadPart ? QString::fromLatin1(hash.result().toHex()) : QString{};
        total += read;
        if (total > size.QuadPart) return {};
        hash.addData(QByteArrayView(chunk, read));
    }
    return {};
}
quint64 filetime(FILETIME t) { return (quint64(t.dwHighDateTime) << 32) | t.dwLowDateTime; }
bool inspect(DWORD pid, Pins *pins, const std::atomic_bool &cancel, const Transport::Identity *expected = nullptr) {
    pins->process = Handle(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid));
    FILETIME created{}, exit{}, kernel{}, user{};
    HANDLE tokenRaw = nullptr;
    if (!pins->process.valid() || !GetProcessTimes(pins->process.value, &created, &exit, &kernel, &user) ||
        !OpenProcessToken(pins->process.value, TOKEN_QUERY, &tokenRaw)) return false;
    Handle token(tokenRaw);
    DWORD size = 0;
    GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> storage(size);
    if (!size || !GetTokenInformation(token.value, TokenUser, storage.data(), size, &size)) return false;
    auto &id = pins->identity;
    id.pid = pid; id.created = filetime(created);
    id.sid = sidString(reinterpret_cast<TOKEN_USER *>(storage.data())->User.Sid);
    TOKEN_ELEVATION elevation{};
    if (!GetTokenInformation(token.value, TokenSessionId, &id.session, sizeof(id.session), &size) || !id.session ||
        !GetTokenInformation(token.value, TokenElevation, &elevation, sizeof(elevation), &size)) return false;
    id.elevated = elevation.TokenIsElevated != 0;
    wchar_t path[32768]; DWORD length = DWORD(std::size(path));
    if (!QueryFullProcessImageNameW(pins->process.value, 0, path, &length) ||
        !lockPath(QString::fromWCharArray(path, length), true, &pins->files)) return false;
    const HANDLE image = pins->files.back().value;
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(image, &info)) return false;
    id.imageVolume = info.dwVolumeSerialNumber;
    id.imageIndex = (quint64(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
    if (expected) {
        if (id.imageVolume != expected->imageVolume || id.imageIndex != expected->imageIndex) return false;
        id.imageSha256 = expected->imageSha256; // Original image remains pinned against writes/deletion.
    } else {
        id.imageSha256 = hashImage(image, cancel);
        if (id.imageSha256.isEmpty()) return false;
    }
    return !id.sid.isEmpty() && WaitForSingleObject(pins->process.value, 0) == WAIT_TIMEOUT;
}
bool exchangeIO(HANDLE pipe, bool write, char *data, DWORD size, DWORD *transferred,
                ULONGLONG deadline, const std::atomic_bool &cancel) {
    Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event.valid()) return false;
    OVERLAPPED operation{}; operation.hEvent = event.value;
    const BOOL done = write ? WriteFile(pipe, data, size, transferred, &operation) : ReadFile(pipe, data, size, transferred, &operation);
    if (done) return true;
    if (GetLastError() != ERROR_IO_PENDING) return false;
    while (!cancel.load() && GetTickCount64() < deadline) {
        const DWORD result = WaitForSingleObject(event.value, 25);
        if (result == WAIT_OBJECT_0) return GetOverlappedResult(pipe, &operation, transferred, FALSE);
        if (result != WAIT_TIMEOUT) break;
    }
    CancelIoEx(pipe, &operation);
    // The OVERLAPPED and its buffer must outlive cancellation completion.
    GetOverlappedResult(pipe, &operation, transferred, TRUE);
    return false;
}
}
#endif

#ifdef Q_OS_WIN
bool NativeIKEv2Transport::protectedDescriptor(const void *descriptor) {
    return verifyDescriptor(const_cast<void *>(descriptor));
}
#endif

struct NativeIKEv2Transport::State {
    std::atomic_bool cancel{false};
    bool busy = false; // QObject thread only
    bool success = false, launch = false;
    Failure failure = Failure::IdentityRejected;
    QJsonObject response;
#ifdef Q_OS_WIN
    Pins self, selected, peer;
    std::vector<Handle> policyPins, configPins;
    Handle launched;
    QString installation, instance;
    bool authenticated = false;
    Endpoint endpoint = Endpoint::Session;
#endif
};

#if defined(Q_OS_WIN) && defined(NATIVE_IKEV2_TRANSPORT_TESTING)
bool NativeIKEv2Transport::offlineWindowsBoundaries() {
    // Exercise the actual IO and pin helpers without authenticating a test image
    // or weakening the protected-install path. Never connect to a Core endpoint.
    QTemporaryDir directory;
    if (!directory.isValid()) return false;
    const auto path = directory.filePath("pinned.conf");
    { QFile file(path); if (!file.open(QIODevice::WriteOnly) || file.write("fixture") != 7) return false; }
    auto transport = std::make_unique<NativeIKEv2Transport>(Approval{});
    auto file = openLocked(path, false, false);
    if (!file.valid()) return false;
    transport->state_->configPins.push_back(std::move(file));
    auto retained = transport->state_;
    transport.reset(); // Destruction cancels, while a worker still owns the state.
    auto replacementRefused = [&] {
        Handle writer(CreateFileW(native(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, 0, nullptr));
        if (writer.valid() || GetLastError() != ERROR_SHARING_VIOLATION) return false;
        return !DeleteFileW(native(path).c_str()) && GetLastError() == ERROR_SHARING_VIOLATION;
    };
    if (!retained->cancel.load() || !replacementRefused()) return false;
    retained.reset();
    { QFile writable(path); if (!writable.open(QIODevice::WriteOnly)) return false; }

    const auto name = QStringLiteral("\\\\.\\pipe\\Bimeron.Desktop.OfflineTest.%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    Handle server(CreateNamedPipeW(native(name).c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 4096, 4096, 0, nullptr));
    if (!server.valid()) return false;
    Handle client(CreateFileW(native(name).c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
    if (!client.valid()) return false;
    Handle connected(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    OVERLAPPED connection{}; connection.hEvent = connected.value;
    if (!connected.valid() || (!ConnectNamedPipe(server.value, &connection) && GetLastError() != ERROR_PIPE_CONNECTED)) return false;
    std::atomic_bool cancel{false};
    std::thread canceller([&] { Sleep(50); cancel.store(true); });
    char buffer[16]{}; DWORD transferred = 0;
    const bool completed = exchangeIO(server.value, false, buffer, sizeof(buffer), &transferred, GetTickCount64() + 3000, cancel);
    const auto error = GetLastError();
    canceller.join();
    // exchangeIO must drain cancellation before its stack OVERLAPPED/buffer die.
    if (completed || !cancel.load() || error != ERROR_OPERATION_ABORTED) return false;
    cancel.store(false);
    DWORD written = 0;
    char sent = 'x';
    if (!WriteFile(client.value, &sent, 1, &written, nullptr) || written != 1) return false;
    return exchangeIO(server.value, false, buffer, sizeof(buffer), &transferred, GetTickCount64() + 3000, cancel) &&
        transferred == 1 && buffer[0] == sent; // Cancellation never closes the peer.
}
#endif

NativeIKEv2Transport::NativeIKEv2Transport(Approval approval, QObject *parent)
    : QObject(parent), approval_(std::move(approval)), state_(std::make_shared<State>()) {}
NativeIKEv2Transport::~NativeIKEv2Transport() { releaseHandles(); }
bool NativeIKEv2Transport::busy() const { return state_->busy; }
NativeIKEv2Transport::ProcessState NativeIKEv2Transport::processState() const {
#ifdef Q_OS_WIN
    if (busy()) return ProcessState::Unknown;
    const auto &handle = state_->launched.valid() ? state_->launched : state_->peer.process;
    if (!handle.valid()) return ProcessState::Unknown;
    switch (WaitForSingleObject(handle.value, 0)) {
    case WAIT_OBJECT_0: return ProcessState::Exited;
    case WAIT_TIMEOUT: return ProcessState::Running;
    default: break;
    }
#endif
    return ProcessState::Unknown;
}
void NativeIKEv2Transport::cancelExchange() { state_->cancel.store(true); }
void NativeIKEv2Transport::releaseHandles() {
    closed_ = true;
    ++generation_;
    cancelExchange();
    // A pending UAC prompt has no safe force-cancel primitive. The worker retains
    // its own pins until it returns; release/destruction must never wait on consent.
    state_ = std::make_shared<State>();
}
bool NativeIKEv2Transport::discover(Endpoint endpoint) { return request(endpoint, "hello"); }
bool NativeIKEv2Transport::request(Endpoint endpoint, const QString &command, const QJsonObject &fields) {
    if (command == "launch") return false;
    return start(endpoint, command, fields);
}
bool NativeIKEv2Transport::launchSession(const QString &config, const QString &digest) {
    return start(Endpoint::Session, "launch", {}, config, digest);
}
bool NativeIKEv2Transport::launchProvisioning(const QString &profile, const QString &server, bool remove) {
    return start(Endpoint::Provisioning, "launch", {{"profile", profile}, {"server", server}, {"remove", remove}});
}

bool NativeIKEv2Transport::start(Endpoint endpoint, QString command, QJsonObject fields,
                               QString config, QString configHash) {
    if (closed_ || busy()) return false;
    state_->busy = true;
    state_->cancel.store(false);
    const auto generation = ++generation_;
    const auto s = state_;
    const auto approval = approval_;
    auto *worker = QThread::create([s, approval, endpoint, command, fields, config, configHash] {
        QJsonObject response;
        Failure failure = Failure::IdentityRejected;
        bool success = false;
        const bool launch = command == "launch";
        auto work = [&]() -> bool {
#ifndef Q_OS_WIN
            failure = Failure::Unsupported;
            return false;
#else
            if (!approved(approval)) { failure = Failure::ApprovalRequired; return false; }
            if (s->cancel.load()) return false;
            if (!s->self.process.valid()) {
                Pins self, selected;
                std::vector<Handle> policy;
                if (!inspect(GetCurrentProcessId(), &self, s->cancel) ||
                    !lockPath(approval.executable, true, &selected.files)) return false;
                const HANDLE image = selected.files.back().value;
                if (hashImage(image, s->cancel) != approval.sha256) return false;
                BY_HANDLE_FILE_INFORMATION info{};
                if (!GetFileInformationByHandle(image, &info)) return false;
                selected.identity.imageSha256 = approval.sha256;
                selected.identity.imageVolume = info.dwVolumeSerialNumber;
                selected.identity.imageIndex = (quint64(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
                const auto manifest = QFileInfo(approval.executable).dir().filePath("bimeron-foreground.json");
                if (!lockPath(manifest, true, &policy)) return false;
                QJsonObject p;
                if (!parse(readFile(policy.back().value, 4096, s->cancel), &p) || p.size() != 3 ||
                    !keys(p, {"schema", "installation", "gui_sha256"}) || p.value("schema") != QJsonValue(1) ||
                    !hex(p.value("installation").toString(), 32) || p.value("gui_sha256") != self.identity.imageSha256) return false;
                s->installation = p.value("installation").toString();
                s->self = std::move(self); s->selected = std::move(selected); s->policyPins = std::move(policy);
            }
            if (WaitForSingleObject(s->self.process.value, 0) != WAIT_TIMEOUT) return false;
            if (launch) {
                // Transport cannot infer absence from failed discovery. The caller
                // owns managed-backend/protection admission. Core provides final exclusion.
                if (s->launched.valid() || s->peer.process.valid() || !s->instance.isEmpty()) {
                    failure = Failure::LaunchRejected; return false;
                }
                QStringList arguments{"ikev2"};
                std::vector<Handle> configPins;
                if (endpoint == Endpoint::Session) {
                    if (!hex(configHash, 64) || !lockPath(config, false, &configPins)) { failure = Failure::InvalidRequest; return false; }
                    const auto bytes = readFile(configPins.back().value, 64 * 1024, s->cancel);
                    if (bytes.isEmpty() || QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) != configHash) {
                        failure = Failure::InvalidRequest; return false;
                    }
                    // Selected-Core validation of native syntax is a prerequisite
                    // owned by ProfileStore/controller; Core revalidates pinned bytes.
                    arguments << "foreground" << "-config" << QDir::toNativeSeparators(config) << "-sha256" << configHash;
                } else {
                    const auto profile = fields.value("profile").toString();
                    const auto server = fields.value("server").toString();
                    if (profile.isEmpty() || profile.size() > 256 || server.isEmpty() || server.size() > 253 ||
                        profile.contains(QChar(0)) || server.contains(QChar(0))) { failure = Failure::InvalidRequest; return false; }
                    arguments << "profile-control" << "-name" << profile << "-server" << server << "-action" << (fields.value("remove").toBool() ? "remove" : "apply");
                }
                arguments << "-requester-pid" << QString::number(s->self.identity.pid)
                          << "-requester-created" << QString::number(s->self.identity.created);
                if (s->cancel.load()) return false;
                const auto exe = native(approval.executable);
                const auto parameters = TrustedCore::windowsCommandLine(arguments).toStdWString();
                const auto directory = native(QFileInfo(approval.executable).absolutePath());
                SHELLEXECUTEINFOW execute{};
                execute.cbSize = sizeof(execute); execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
                execute.lpVerb = L"runas"; execute.lpFile = exe.c_str(); execute.lpParameters = parameters.c_str();
                execute.lpDirectory = directory.c_str(); execute.nShow = SW_HIDE;
                // Shell execution occurs on this worker's STA, never the UI thread.
                const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                if (FAILED(initialized)) { failure = Failure::LaunchRejected; return false; }
                const BOOL launched = ShellExecuteExW(&execute);
                const DWORD error = launched ? ERROR_SUCCESS : GetLastError();
                CoUninitialize();
                if (!launched || !execute.hProcess) {
                    if (execute.hProcess) CloseHandle(execute.hProcess);
                    failure = error == ERROR_CANCELLED ? Failure::Cancelled : Failure::LaunchRejected;
                    return false;
                }
                s->launched = Handle(execute.hProcess); s->configPins = std::move(configPins);
                return true;
            }
            const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const QByteArray request = encodeRequest(endpoint, command, id, s->installation, approval.revision, fields);
            if (request.isEmpty()) { failure = Failure::InvalidRequest; return false; }
            const bool readOnlyHello = command == "hello" && !fields.contains("accept_revision");
            if ((!readOnlyHello && !s->authenticated) || (s->authenticated && endpoint != s->endpoint)) {
                failure = Failure::InvalidRequest; return false;
            }
            if (!s->instance.isEmpty() && command != "hello" && fields.value("instance") != s->instance) {
                failure = Failure::InvalidRequest; return false;
            }
            const auto name = native(endpointName(s->self.identity.sid, s->self.identity.session));
            const auto deadline = GetTickCount64() + 7000;
            Handle pipe;
            while (!s->cancel.load() && GetTickCount64() < deadline) {
                // Identification-only SQOS is mandatory before the server can
                // read any client bytes; impersonation/delegation is never enabled.
                pipe = Handle(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                    OPEN_EXISTING, FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
                if (pipe.valid()) break;
                if (GetLastError() != ERROR_PIPE_BUSY) { failure = Failure::Unavailable; return false; }
                WaitNamedPipeW(name.c_str(), 25);
            }
            if (!pipe.valid()) { failure = Failure::Timeout; return false; }
            ULONG pid = 0;
            if (!GetNamedPipeServerProcessId(pipe.value, &pid)) return false;
            {
                Pins peer;
                const auto *expected = s->peer.process.valid() ? &s->peer.identity : nullptr;
                if ((expected && (pid != expected->pid || WaitForSingleObject(s->peer.process.value, 0) != WAIT_TIMEOUT)) ||
                    !inspect(pid, &peer, s->cancel, expected ? expected : &s->selected.identity) || (expected && peer.identity != *expected) || !peer.identity.elevated ||
                    peer.identity.sid != s->self.identity.sid || peer.identity.session != s->self.identity.session ||
                    peer.identity.imageSha256 != approval.sha256 ||
                    peer.identity.imageVolume != s->selected.identity.imageVolume || peer.identity.imageIndex != s->selected.identity.imageIndex) return false;
                if (s->launched.valid()) {
                    FILETIME created{}, exit{}, kernel{}, user{};
                    if (GetProcessId(s->launched.value) != pid || !GetProcessTimes(s->launched.value, &created, &exit, &kernel, &user) ||
                        filetime(created) != peer.identity.created) return false;
                }
                if (!expected) s->peer = std::move(peer);
            }
            // No write can precede OS peer authentication above.
            const auto exchangeDeadline = GetTickCount64() + 7000;
            DWORD transferred = 0;
            if (!exchangeIO(pipe.value, true, const_cast<char *>(request.constData()), DWORD(request.size()), &transferred, exchangeDeadline, s->cancel) || transferred != DWORD(request.size())) {
                failure = Failure::Timeout; return false;
            }
            QByteArray frame;
            char chunk[8192];
            while (!frame.contains('\n') && frame.size() <= 256 * 1024) {
                if (!exchangeIO(pipe.value, false, chunk, sizeof(chunk), &transferred, exchangeDeadline, s->cancel) || !transferred) {
                    failure = Failure::Timeout; return false;
                }
                frame.append(chunk, transferred);
            }
            if (!decodeResponse(frame, id, s->installation, approval, endpoint, s->peer.identity, s->instance, &response) ||
                (command == "hello" && response.value("error").toString().isEmpty() &&
                    (!response.value("core").isObject() || !response.value("capabilities").isObject()))) {
                failure = Failure::InvalidResponse; return false;
            }
            s->instance = response.value("instance").toString();
            if (command == "hello" && response.value("error").toString().isEmpty()) {
                s->authenticated = true; s->endpoint = endpoint;
            }
            return true;
#endif
        };
        success = work();
        // Cancellation never rolls back an accepted owner operation. Preserve pins
        // and let the controller resolve ambiguous activation/confirmation via status.
        if (s->cancel.load()) { success = false; failure = Failure::Cancelled; }
        s->success = success; s->failure = failure; s->launch = launch; s->response = response;
    });
    connect(worker, &QThread::finished, this, [this, s, generation, command] {
        if (generation != generation_) return;
        state_->busy = false;
        if (s->cancel.load()) emit failed(command, Failure::Cancelled);
        else if (!s->success) emit failed(command, s->failure);
        else if (s->launch) emit launched();
        else emit replied(command, s->response);
    }, Qt::QueuedConnection);
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
    return true;
}
