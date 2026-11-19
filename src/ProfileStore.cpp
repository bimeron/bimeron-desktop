#include "ProfileStore.h"
#include "ProfileTextEdit.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QMap>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <aclapi.h>
#include <io.h>
#endif

namespace {
bool protectFile(QFileDevice &file) {
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    QByteArray user(int(size), '\0');
    const bool haveUser = GetTokenInformation(token, TokenUser, user.data(), size, &size);
    CloseHandle(token);
    BYTE system[SECURITY_MAX_SID_SIZE];
    DWORD systemSize = sizeof(system);
    if (!haveUser || !CreateWellKnownSid(WinLocalSystemSid, nullptr, system, &systemSize)) return false;
    EXPLICIT_ACCESSW entries[2]{};
    for (auto &entry : entries) {
        entry.grfAccessMode = SET_ACCESS;
        entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    }
    entries[0].grfAccessPermissions = FILE_ALL_ACCESS;
    entries[0].Trustee.ptstrName = reinterpret_cast<LPWSTR>(reinterpret_cast<TOKEN_USER *>(user.data())->User.Sid);
    entries[1].grfAccessPermissions = FILE_GENERIC_READ;
    entries[1].Trustee.ptstrName = reinterpret_cast<LPWSTR>(system);
    PACL acl = nullptr;
    if (SetEntriesInAclW(2, entries, nullptr, &acl) != ERROR_SUCCESS) return false;
    const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(int(file.handle())));
    // QFile's data handle does not request WRITE_DAC. Reopen the same file object
    // (not a path susceptible to replacement) for the security operation.
    HANDLE security = ReOpenFile(handle, WRITE_DAC | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    const DWORD status = security == INVALID_HANDLE_VALUE ? GetLastError() : SetSecurityInfo(security, SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr, nullptr, acl, nullptr);
    const bool ok = status == ERROR_SUCCESS;
    if (security != INVALID_HANDLE_VALUE) CloseHandle(security);
    LocalFree(acl);
    return ok;
#else
    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
}

bool absoluteReference(const QString &value, bool ssh = false) {
    return value.isEmpty() || (!value.startsWith('"') && (QFileInfo(value).isAbsolute()
        || (ssh && (value == "~" || value.startsWith("~/") || value.startsWith("~\\")))));
}

// Go strconv.Unquote for a double-quoted literal, which Core applies to quoted
// option values. Valid Go syntax can still produce bytes that are not UTF-8
// (for example \xff); such a name is a real path for Core but cannot be
// represented here, so it is reported separately from malformed syntax.
enum class Unquoted { Ok, Malformed, Unrepresentable };
Unquoted goUnquote(const QString &quoted, QString *out) {
    const QByteArray in = quoted.toUtf8();
    if (in.size() < 2 || in.front() != '"' || in.back() != '"') return Unquoted::Malformed;
    QByteArray bytes;
    const auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
    const auto rune = [&bytes](uint code) {
        if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
        bytes += QString::fromUcs4(reinterpret_cast<const char32_t *>(&code), 1).toUtf8();
        return true;
    };
    for (qsizetype i = 1; i < in.size() - 1; ++i) {
        const char c = in.at(i);
        if (c == '"' || c == '\n') return Unquoted::Malformed;
        if (c != '\\') { bytes += c; continue; }
        if (++i >= in.size() - 1) return Unquoted::Malformed;
        const char e = in.at(i);
        static const QByteArray simple("abfnrtv\\\""), mapped("\a\b\f\n\r\t\v\\\"");
        if (simple.contains(e)) { bytes += mapped.at(simple.indexOf(e)); continue; }
        int digits = e == 'x' ? 2 : e == 'u' ? 4 : e == 'U' ? 8 : 0;
        if (digits) {
            uint value = 0;
            for (int d = 0; d < digits; ++d) {
                if (++i >= in.size() - 1 || hex(in.at(i)) < 0) return Unquoted::Malformed;
                value = value * 16 + uint(hex(in.at(i)));
            }
            if (e == 'x') bytes += char(value);
            else if (!rune(value)) return Unquoted::Malformed;
            continue;
        }
        if (e >= '0' && e <= '7') {
            uint value = uint(e - '0');
            for (int d = 0; d < 2; ++d) {
                if (++i >= in.size() - 1 || in.at(i) < '0' || in.at(i) > '7') return Unquoted::Malformed;
                value = value * 8 + uint(in.at(i) - '0');
            }
            if (value > 255) return Unquoted::Malformed;
            bytes += char(value);
            continue;
        }
        return Unquoted::Malformed;
    }
    // Keep a leading U+FEFF: Core treats it as part of the file name.
    const QString decoded = ProfileStore::keyFileText(bytes);
    if (decoded.toUtf8() != bytes) return Unquoted::Unrepresentable;
    *out = decoded;
    return Unquoted::Ok;
}

bool safeUriReferences(const QString &value) {
    if (value.startsWith('"')) return false; // Core quoting is not URI percent encoding.
    const QUrl uri(value);
    if (uri.scheme() != "ssh" && uri.scheme() != "masque") return true;
    if (!uri.isValid()) return false;
    for (const auto &option : QUrlQuery(uri).queryItems(QUrl::FullyDecoded)) {
        if ((option.first == "identity" || option.first == "known_hosts")
            && !absoluteReference(option.second, true)) return false;
        if (option.first == "ca" && !absoluteReference(option.second)) return false;
    }
    return true;
}
}

bool ProfileStore::writeConfig(const QString &path, const QByteArray &data, QString *error) {
    const QFileInfo existing(path);
    if (existing.exists() && (!existing.isWritable() || !(existing.permissions()
        & (QFileDevice::WriteOwner | QFileDevice::WriteGroup | QFileDevice::WriteOther)))) {
        if (error) *error = QObject::tr("The profile is read-only; the previous file was retained.");
        return false;
    }
    QSaveFile output(path);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) *error = QObject::tr("Cannot open the profile for atomic replacement.");
        return false;
    }
    if (!protectFile(output)) {
        if (error) *error = QObject::tr("Cannot protect the profile file; no data was written.");
        return false;
    }
    if (output.write(data) != data.size() || !output.commit()) {
        if (error) *error = QObject::tr("Cannot atomically replace the profile; the previous file was retained.");
        return false;
    }
    return true;
}

bool ProfileStore::importReferencesSafe(const QByteArray &data, QString *error) {
    QString section;
    QString text = QString::fromUtf8(data);
    if (text.startsWith(QChar(0xfeff))) text.remove(0, 1);
    for (const QString &line : text.split('\n')) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith('#') || trimmed.startsWith(';')) continue;
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            section = sectionName(trimmed);
            continue;
        }
        if (section == "GeoData") {
            const int equal = trimmed.indexOf('=');
            if (equal < 0) continue;
            const QString key = trimmed.left(equal).trimmed();
            QString reference = trimmed.mid(equal + 1).trimmed();
            bool decoded = true;
            if (reference.startsWith('"')) {
                QJsonParseError parse;
                const auto document = QJsonDocument::fromJson(('[' + reference + ']').toUtf8(), &parse);
                decoded = parse.error == QJsonParseError::NoError && document.isArray()
                    && document.array().size() == 1 && document.array().at(0).isString();
                if (decoded) reference = document.array().at(0).toString();
            }
            if ((key == "GeoIPFile" || key == "GeoSiteFile")
                && (!decoded || !absoluteReference(reference))) {
                if (error) *error = QObject::tr("Import would relocate a GeoData file reference. Transfer the profile and databases together or correct the paths; no files were copied.");
                return false;
            }
            continue;
        }
        if (section != "AccessKey") continue;
        const auto equal = trimmed.indexOf('=');
        if (equal < 0) continue;
        const QString key = trimmed.left(equal).trimmed(), value = trimmed.mid(equal + 1).trimmed();
        bool safe = true;
        if (key == "IdentityFile" || key == "KnownHostsFile") safe = absoluteReference(value, true);
        else if (key == "CAFile") safe = absoluteReference(value);
        else if (key == "File") {
            safe = absoluteReference(value) && (value.isEmpty() || localRegularFile(value));
            if (safe && !value.isEmpty()) {
                QFile file(value);
                safe = file.open(QIODevice::ReadOnly);
                if (safe) {
                    const QByteArray contents = file.read(64 * 1024 + 1);
                    QString uri = QString::fromUtf8(contents);
                    if (uri.startsWith(QChar(0xfeff))) uri.remove(0, 1);
                    safe = contents.size() <= 64 * 1024 && safeUriReferences(uri.trimmed());
                }
            }
        } else if (QStringList{"SS", "SSConf", "SOCKS5", "VLESS", "Hysteria2", "VMess", "Trojan", "AnyTLS", "TUIC", "SSH", "HTTP", "Naive", "MASQUE", "Key"}.contains(key))
            safe = safeUriReferences(value);
        if (!safe) {
            if (error) *error = QObject::tr("Import would relocate a relative or ambiguous file reference. Use explicit absolute paths in the source profile first; no dependency files were copied.");
            return false;
        }
    }
    return true;
}

QString ProfileStore::keyFileReference(const QString &profileText) {
    // Unlike connectionInfo(), which drops U+FEFF anywhere for labels, keep every
    // character except a document BOM: Core reads the reference exactly.
    QString text = profileText;
    if (text.startsWith(QChar(0xfeff))) text.remove(0, 1);
    QString section, reference;
    for (const QString &line : text.split('\n')) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith('#') || trimmed.startsWith(';')) continue;
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) { section = sectionName(trimmed); continue; }
        const qsizetype equal = trimmed.indexOf('=');
        if (section != "AccessKey" || equal < 0 || trimmed.left(equal).trimmed() != "File") continue;
        // Core decodes quoted values with Go escapes; an invalid literal stays as written
        // (Core then rejects the profile). A name Desktop cannot represent is not
        // looked up at all: the profile is treated as having no readable key file.
        const QString raw = trimmed.mid(equal + 1).trimmed();
        QString value;
        const Unquoted result = raw.startsWith('"') ? goUnquote(raw, &value) : Unquoted::Malformed;
        reference = result == Unquoted::Ok ? value : result == Unquoted::Unrepresentable ? QString() : raw;
    }
    return reference;
}

QString ProfileStore::keyFilePath(const QString &profilePath, const QString &reference) {
    if (reference.isEmpty()) return {};
    return QDir::cleanPath(QFileInfo(reference).isAbsolute() ? reference
        : QFileInfo(profilePath).absoluteDir().filePath(reference));
}

bool ProfileStore::managedKeyFile(const QString &profilePath, const QString &reference) {
    const QRegularExpression pattern(QStringLiteral("^%1\\.[0-9a-f]{8}\\.wg\\.conf$")
        .arg(QRegularExpression::escape(QFileInfo(profilePath).completeBaseName())));
    return !reference.contains('/') && !reference.contains('\\') && pattern.match(reference).hasMatch();
}

QString ProfileStore::nextKeyFileName(const QString &profilePath) {
    const QDir directory = QFileInfo(profilePath).absoluteDir();
    for (int attempt = 0; attempt < 16; ++attempt) {
        const QString name = QStringLiteral("%1.%2.wg.conf").arg(QFileInfo(profilePath).completeBaseName(),
            QString::number(QRandomGenerator::system()->generate(), 16).rightJustified(8, '0'));
        if (!QFileInfo::exists(directory.filePath(name))) return name;
    }
    return {};
}

QString ProfileStore::keyFileText(const QByteArray &data) {
    const QByteArray bom = QByteArray::fromHex("efbbbf");
    return data.startsWith(bom) ? QString(QChar::ByteOrderMark) + QString::fromUtf8(data.mid(3)) : QString::fromUtf8(data);
}

bool ProfileStore::localRegularFile(const QString &path) {
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) return false;
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(path);
    // UNC, device and extended-length paths, and mapped network drives.
    if (native.startsWith(QStringLiteral("\\\\")) || native.size() < 3 || native.at(1) != ':') return false;
    const std::wstring root = native.left(3).toStdWString();
    if (GetDriveTypeW(root.c_str()) == DRIVE_REMOTE) return false;
    // A symbolic link or junction can redirect a local-looking path to a share.
    // GetFileAttributesW reports the link itself and does not follow it.
    QString prefix = native.left(2);
    for (const QString &part : native.mid(3).split(QLatin1Char('\\'), Qt::SkipEmptyParts)) {
        prefix += QLatin1Char('\\') + part;
        const DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(prefix.utf16()));
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    }
#endif
    return QFileInfo(path).isFile();
}

bool ProfileStore::readKeyFile(const QString &profilePath, const QString &reference, QByteArray *data, QString *error) {
    const QString path = keyFilePath(profilePath, reference);
    QFile file(path);
    if (reference.isEmpty() || !localRegularFile(path) || !file.open(QIODevice::ReadOnly)) {
        if (error) *error = QObject::tr("The WireGuard configuration file of this profile is unavailable.");
        return false;
    }
    *data = file.read(64 * 1024 + 1);
    if (data->size() > 64 * 1024 || keyFileText(*data).toUtf8() != *data) {
        if (error) *error = QObject::tr("The WireGuard configuration must be valid UTF-8 and at most 64 KiB.");
        return false;
    }
    return true;
}

bool ProfileStore::writeProfilePair(const QString &profilePath, const QByteArray &profile,
                                    const QByteArray *keyFile, QString *error) {
    QString previous;
    {
        QFile current(profilePath);
        if (current.open(QIODevice::ReadOnly)) previous = keyFileReference(QString::fromUtf8(current.read(64 * 1024 + 1)));
    }
    if (!keyFile) return writeConfig(profilePath, profile, error);
    const QString name = nextKeyFileName(profilePath);
    const QString keyPath = QFileInfo(profilePath).absoluteDir().filePath(name);
    if (name.isEmpty() || keyFile->size() > 64 * 1024) {
        if (error) *error = QObject::tr("Cannot create the WireGuard configuration file; the previous profile was retained.");
        return false;
    }
    if (!writeConfig(keyPath, *keyFile, error)) { QFile::remove(keyPath); return false; }
    const QByteArray text = ProfileTextEdit::field(QString::fromUtf8(profile), "AccessKey", "File", name).toUtf8();
    if (text.size() > 64 * 1024 || !writeConfig(profilePath, text, error)) {
        if (error && text.size() > 64 * 1024) *error = QObject::tr("Profile exceeds 64 KiB.");
        QFile::remove(keyPath);
        return false;
    }
    // The profile now references the new revision. A stale previous revision is
    // harmless; it is removed best-effort and never outside the managed pair.
    if (previous != name && managedKeyFile(profilePath, previous))
        QFile::remove(keyFilePath(profilePath, previous));
    return true;
}

QString ProfileStore::createPair(const QByteArray &profile, const QByteArray &keyFile, QString *error) const {
    const QString destination = QDir(root_).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".conf");
    if (QFileInfo::exists(destination)) {
        if (error) *error = QObject::tr("Cannot create the profile file.");
        return {};
    }
    if (!writeProfilePair(destination, profile, &keyFile, error)) {
        QFile::remove(destination);
        return {};
    }
    return QFileInfo(destination).absoluteFilePath();
}

bool ProfileStore::removeProfileFiles(const QString &profilePath, QString *error) {
    QString reference;
    {
        QFile current(profilePath);
        if (current.open(QIODevice::ReadOnly)) reference = keyFileReference(QString::fromUtf8(current.read(64 * 1024 + 1)));
    }
    if (QFileInfo::exists(profilePath) && !QFile::remove(profilePath)) {
        if (error) *error = QObject::tr("Cannot remove the profile file.");
        return false;
    }
    // The managed key file is removed after its profile: a failure leaves an
    // unreferenced file, never a profile without its configuration.
    if (managedKeyFile(profilePath, reference) && QFileInfo::exists(keyFilePath(profilePath, reference))
        && !QFile::remove(keyFilePath(profilePath, reference))) {
        if (error) *error = QObject::tr("The profile was removed, but its WireGuard configuration file could not be deleted.");
        return false;
    }
    return true;
}

ProfileStore::ProfileStore(const QString &root) {
    root_ = root.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/profiles" : root;
    QDir().mkpath(root_);
    index_ = QFileInfo(root_).dir().filePath("profiles.json");
}

QList<Profile> ProfileStore::load(QString *error) const {
    QFile file(index_);
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return {};
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    QList<Profile> result;
    for (const auto &value : document.array()) {
        const auto object = value.toObject();
        const QString path = object.value("path").toString();
        if (!path.isEmpty())
            result.push_back({identity(path), object.value("name").toString(), path});
    }
    return result;
}

bool ProfileStore::save(const QList<Profile> &profiles, QString *error) const {
    QJsonArray array;
    for (const auto &profile : profiles)
        array.append(QJsonObject{{"name", profile.name}, {"path", profile.path}});
    return writeConfig(index_, QJsonDocument(array).toJson(), error);
}

bool ProfileStore::normalizeImport(QByteArray &data) {
    if (data.size() > 64 * 1024) return false;
    if (data.startsWith(QByteArray::fromHex("efbbbf"))) data.remove(0, 3);
    return QString::fromUtf8(data).toUtf8() == data;
}

QString ProfileStore::importConfig(const QString &source, QString *error) const {
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) *error = input.errorString();
        return {};
    }
    QByteArray data = input.read(64 * 1024 + 1);
    if (!normalizeImport(data)) {
        if (error) *error = QObject::tr("The profile must be valid UTF-8 and at most 64 KiB.");
        return {};
    }
    QSet<QString> sections;
    for (const auto &line : QString::fromUtf8(data).split('\n')) sections.insert(sectionName(line));
    if (data.size() > 64 * 1024 || !sections.contains("Profile")
        || (!sections.contains("AccessKey") && !sections.contains("IKEv2"))) {
        if (error) *error = QObject::tr("The file does not appear to be a supported Bimeron configuration");
        return {};
    }
    if (QFileInfo(source).absolutePath() != QDir(root_).absolutePath()
        && !importReferencesSafe(data, error)) return {};
    const QString destination = QDir(root_).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".conf");
    if (!writeConfig(destination, data, error)) return {};
    return QFileInfo(destination).absoluteFilePath();
}

QString ProfileStore::sectionName(const QString &line) {
    QString text = line;
    if (text.startsWith(QChar(0xfeff))) text.remove(0, 1);
    text = text.trimmed();
    return text.startsWith('[') && text.endsWith(']') ? text.mid(1, text.size() - 2).trimmed() : QString();
}

QString ProfileStore::uriSourceField(const QString &uri) {
    const QString scheme = uri.section("://", 0, 0);
    static const QMap<QString, QString> sources{{"ss", "SS"}, {"ssh", "SSH"}, {"ssconf", "SSConf"},
        {"socks5", "SOCKS5"}, {"vless", "VLESS"}, {"hysteria2", "Hysteria2"}, {"hy2", "Hysteria2"},
        {"vmess", "VMess"}, {"trojan", "Trojan"}, {"tuic", "TUIC"}, {"http", "HTTP"},
        {"https", "HTTP"}, {"anytls", "AnyTLS"}, {"naive+https", "Naive"}, {"naive+quic", "Naive"}, {"masque", "MASQUE"}};
    if (!uri.contains("://") || uri.contains(QRegularExpression("[\\x00-\\x20\\x7f]"))) return {};
    if (scheme == "http" || scheme == "https") {
        const QUrl url(uri);
        if (!url.isValid() || url.host().isEmpty() || url.port() < 1 || url.port() > 65535
            || !url.path().isEmpty() || url.hasQuery() || url.hasFragment()
            || (url.userName().isEmpty() != url.password().isEmpty())) return {};
    }
    return sources.value(scheme);
}

QString ProfileStore::createConfig(const QString &name, const QString &key, QString *error, const QString &identityFile, const QString &knownHostsFile) const {
    const QString trimmed = key.trimmed();
    const QString field = uriSourceField(trimmed);
    if (field.isEmpty()) {
        if (error) *error = QObject::tr("Unsupported or malformed protocol URI");
        return {};
    }
    QString safeName = name.trimmed().left(128);
    safeName.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1f\\x7f]")));
    QString access = field + " = " + trimmed;
    if (field == "SSH") {
        if (!identityFile.isEmpty()) access += "\nIdentityFile = " + identityFile;
        if (!knownHostsFile.isEmpty()) access += "\nKnownHostsFile = " + knownHostsFile;
    }
    const QString interfaceSettings = field == "SSH" ? "Mode = proxy\nListen = 127.0.0.1:1080"
        : "Mode = vpn\nName = Bimeron\nMTU = 1420\nDNS = 1.1.1.1\nIPv6 = disable\nKillSwitch = false";
    const QByteArray data = QString("[Profile]\nVersion = 1\nName = %1\n\n[AccessKey]\n%2\n\n[Interface]\n%3\n\n[Routing]\nDefault = proxy\n")
        .arg(safeName, access, interfaceSettings).toUtf8();
    if (data.size() > 64 * 1024) {
        if (error) *error = QObject::tr("Profile exceeds 64 KiB.");
        return {};
    }
    if (!importReferencesSafe(data, error)) return {};
    const QString destination = QDir(root_).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".conf");
    if (!writeConfig(destination, data, error)) return {};
    return QFileInfo(destination).absoluteFilePath();
}

QString ProfileStore::identity(const QString &absolutePath) {
    QString normalized = QDir::cleanPath(QFileInfo(absolutePath).absoluteFilePath());
#ifdef Q_OS_WIN
    normalized = QDir::toNativeSeparators(normalized).toLower();
#endif
    return QString::fromLatin1(QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString ProfileStore::displayName(const QString &configPath) {
    QFile file(configPath);
    if (file.open(QIODevice::ReadOnly)) {
        const QString text = QString::fromUtf8(file.read(64 * 1024));
        const auto match = QRegularExpression(QStringLiteral("(?m)^Name\\s*=\\s*(.+)$")).match(text);
        if (match.hasMatch()) return match.captured(1).trimmed();
    }
    return QFileInfo(configPath).completeBaseName();
}

ProfileConnectionInfo ProfileStore::connectionInfo(const QString &configText) {
    ProfileConnectionInfo info;
    QString section;
    QString source;
    QString value;
    QString type;
    info.interfaceMode = QStringLiteral("vpn");
    bool native = false, packet = false;
    for (const QString &line : configText.split('\n')) {
        const QString trimmed = QString(line).remove(QChar(0xfeff)).trimmed();
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            section = sectionName(trimmed);
            native = native || section == "IKEv2";
            packet = packet || section == "AccessKey";
            continue;
        }
        if (trimmed.startsWith('#') || trimmed.startsWith(';')) continue;
        const qsizetype equal = trimmed.indexOf('=');
        if (equal < 0) continue;
        const QString key = trimmed.left(equal).trimmed();
        QString field = trimmed.mid(equal + 1).trimmed();
        if (field.startsWith('"')) {
            QJsonParseError parse;
            const auto decoded = QJsonDocument::fromJson(('[' + field + ']').toUtf8(), &parse);
            if (parse.error == QJsonParseError::NoError && decoded.isArray() && decoded.array().size() == 1 && decoded.array().at(0).isString()) field = decoded.array().at(0).toString();
        }
        if (section == "Interface" && key.compare("Mode", Qt::CaseInsensitive) == 0)
            info.interfaceMode = field.toLower();
        if (section != "AccessKey") continue;
        if (key.compare("Type", Qt::CaseInsensitive) == 0) type = field.toLower();
        else if (QStringList{"SS", "SSConf", "SOCKS5", "VLESS", "Hysteria2", "VMess", "Trojan", "TUIC", "HTTP", "AnyTLS", "Naive", "MASQUE", "SSH", "Key", "File"}.contains(key, Qt::CaseInsensitive)) {
            source = key;
            value = field;
        }
    }
    if (native) {
        if (!packet) {
            info.backend = ProfileConnectionInfo::Backend::NativeIKEv2;
            info.protocol = QStringLiteral("IKEv2");
        }
        return info; // connectable retains its packet-only meaning.
    }
    if (packet) info.backend = ProfileConnectionInfo::Backend::Packet;
    if (source.compare("File", Qt::CaseInsensitive) == 0) {
        info.keyFile = value;
        info.connectable = !value.isEmpty();
    } else if (!source.isEmpty()) {
        const QString scheme = source.compare("Key", Qt::CaseInsensitive) == 0
            ? value.section("://", 0, 0).toLower() : source.toLower();
        if (scheme == "ss") info.protocol = QStringLiteral("Shadowsocks");
        else if (scheme == "ssconf") info.protocol = QStringLiteral("Shadowsocks ⟳");
        else if (scheme == "socks5") info.protocol = QStringLiteral("SOCKS5");
        else if (scheme == "vless") info.protocol = QStringLiteral("VLESS");
        else if (scheme == "hysteria2" || scheme == "hy2") info.protocol = QStringLiteral("Hysteria2");
        else if (scheme == "ssh") info.protocol = QStringLiteral("SSH");
        const QString newSource = uriSourceField(value);
        if (QStringList{"VMess", "Trojan", "TUIC", "HTTP", "AnyTLS", "Naive", "MASQUE"}.contains(newSource)
            && (source.compare("Key", Qt::CaseInsensitive) == 0 || source.compare(newSource, Qt::CaseInsensitive) == 0)) {
            info.protocol = newSource;
            info.connectable = true; // The selected CLI validates before start.
            return info;
        }
        const QUrl uri(value);
        const bool hysteria2 = scheme == "hysteria2" || scheme == "hy2";
        info.connectable = !info.protocol.isEmpty() && uri.isValid()
            && (hysteria2 ? (uri.scheme() == "hysteria2" || uri.scheme() == "hy2") : uri.scheme().compare(scheme, Qt::CaseInsensitive) == 0)
            && (scheme == "ss" || (!uri.host().isEmpty() && (scheme == "ssconf" || scheme == "ssh" || uri.port() > 0)));
        if (scheme == "ssh") {
            info.connectable = info.connectable && (info.interfaceMode == "proxy" || info.interfaceMode == "vpn")
                && !uri.userName().isEmpty() && !uri.userInfo(QUrl::FullyEncoded).contains(':') && uri.path().isEmpty()
                && (uri.port() == -1 || (uri.port() > 0 && uri.port() <= 65535));
            QSet<QString> seen;
            for (const auto &option : QUrlQuery(uri).queryItems(QUrl::FullyDecoded)) {
                if (seen.contains(option.first) || (option.first != "identity" && option.first != "known_hosts") || option.second.isEmpty())
                    info.connectable = false;
                seen.insert(option.first);
            }
        }
        if (scheme == "vless")
            info.connectable = info.connectable && QRegularExpression(
                QStringLiteral("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$"))
                .match(uri.userName()).hasMatch();
        if (hysteria2) {
            const QUrlQuery query(uri);
            const bool auth = !query.queryItemValue("auth", QUrl::FullyDecoded).isEmpty();
            info.connectable = info.connectable && (auth != !uri.userName().isEmpty())
                && uri.password().isEmpty() && uri.path().isEmpty() && uri.port() <= 65535;
            QSet<QString> seen;
            for (const auto &option : query.queryItems(QUrl::FullyDecoded)) {
                const QString &key = option.first, &setting = option.second;
                if (seen.contains(key) || (key != "auth" && key != "sni" && key != "insecure" && key != "alpn" && key != "fp")
                    || ((key == "auth" || key == "sni") && setting.isEmpty())
                    || (key == "insecure" && setting != "0") || (key == "alpn" && setting != "h3")
                    || (key == "fp" && !QStringList{"firefox", "firefox-116", "firefox-116b", "firefox-116c", "chrome", "chrome-115", "chrome-146"}.contains(setting)))
                    info.connectable = false;
                seen.insert(key);
            }
        }
    } else if (type == "shadowsocks" || type == "ss") {
        info.protocol = QStringLiteral("Shadowsocks");
        info.connectable = true;
    } else if (type == "socks5") {
        info.protocol = QStringLiteral("SOCKS5");
        info.connectable = true;
    } else if (type == "vless") {
        info.protocol = QStringLiteral("VLESS");
        info.connectable = true;
    } else if (QStringList{"hysteria2", "tuic", "anytls", "vmess"}.contains(type)) {
        // Like their links, the selected CLI validates these before start.
        info.protocol = type == "hysteria2" ? "Hysteria2" : type == "tuic" ? "TUIC" : type == "anytls" ? "AnyTLS" : "VMess";
        info.connectable = true;
    } else if (QStringList{"trojan", "http", "https", "naive", "masque"}.contains(type)) {
        info.protocol = type == "trojan" ? "Trojan" : type == "naive" ? "Naive" : type == "masque" ? "MASQUE" : "HTTP";
        info.connectable = info.interfaceMode == "proxy" || info.interfaceMode == "vpn";
    } else if (type == "ssh") {
        info.protocol = QStringLiteral("SSH");
        info.connectable = info.interfaceMode == "proxy" || info.interfaceMode == "vpn";
    }
    return info;
}

bool ProfileStore::sshFilesReadable(const QString &configText, const QString &profilePath, QString *error) {
    const auto fields = [](const QString &text) {
        QMap<QString, QString> result;
        bool access = false;
        for (const QString &line : text.split('\n')) {
            const QString value = QString(line).remove(QChar(0xfeff)).trimmed();
            if (value.startsWith('[')) { access = sectionName(value) == "AccessKey"; continue; }
            const auto equal = value.indexOf('=');
            if (access && equal > 0 && !value.startsWith('#') && !value.startsWith(';'))
                result.insert(value.left(equal).trimmed(), value.mid(equal + 1).trimmed());
        }
        return result;
    };
    const auto values = fields(configText);
    QString source = values.value("SSH", values.value("Key"));
    const QDir base = QFileInfo(profilePath).absoluteDir();
    if (values.contains("File")) {
        // Core reports a non-local or irregular file itself; the GUI never opens it.
        if (!localRegularFile(base.filePath(values.value("File")))) return true;
        QFile file(base.filePath(values.value("File")));
        if (!file.open(QIODevice::ReadOnly)) return false;
        source = QString::fromUtf8(file.read(64 * 1024 + 1)).trimmed();
        if (source.startsWith(QChar(0xfeff))) source.remove(0, 1);
    }
    const QUrl uri(source);
    if (values.value("Type") != "ssh" && uri.scheme() != "ssh") return true;
    const QUrlQuery query(uri);
    const QString hosts = values.value("KnownHostsFile", query.queryItemValue("known_hosts", QUrl::FullyDecoded));
    const QString identity = values.value("IdentityFile", query.queryItemValue("identity", QUrl::FullyDecoded));
    const auto readable = [&base](const QString &path) {
        if (path.startsWith('~') || !QFileInfo(base.filePath(path)).isFile()) return false;
        QFile file(base.filePath(path));
        return file.open(QIODevice::ReadOnly);
    };
    if (hosts.isEmpty() || !readable(hosts) || (!identity.isEmpty() && !readable(identity))) {
        if (error) *error = QObject::tr("SSH requires a readable known_hosts file and, when selected, a readable identity file. Use explicit paths accessible to Core; service access can differ from GUI access.");
        return false;
    }
    return true;
}
