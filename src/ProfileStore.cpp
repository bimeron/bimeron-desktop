#include "ProfileStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

ProfileStore::ProfileStore() {
    root_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/profiles";
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
    QSaveFile file(index_);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(array).toJson()) < 0 || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    QFile::setPermissions(index_, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

QString ProfileStore::importConfig(const QString &source, QString *error) const {
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) *error = input.errorString();
        return {};
    }
    const QByteArray data = input.read(64 * 1024 + 1);
    if (data.size() > 64 * 1024 || !data.contains("[Profile]") || !data.contains("[AccessKey]")) {
        if (error) *error = QObject::tr("The file does not appear to be a supported Bimeron configuration");
        return {};
    }
    const QString destination = QDir(root_).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".conf");
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) {
        if (error) *error = output.errorString();
        return {};
    }
    QFile::setPermissions(destination, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return QFileInfo(destination).absoluteFilePath();
}

QString ProfileStore::createConfig(const QString &name, const QString &key, QString *error) const {
    const QString trimmed = key.trimmed();
    const QString field = trimmed.startsWith("ssconf://") ? "SSConf" : trimmed.startsWith("ss://") ? "SS" : trimmed.startsWith("socks5://") ? "SOCKS5" : trimmed.startsWith("vless://") ? "VLESS" : QString();
    if (field.isEmpty()) {
        if (error) *error = QObject::tr("The key must begin with ss://, ssconf://, socks5://, or vless://");
        return {};
    }
    QString safeName = name.trimmed().left(128);
    safeName.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1f\\x7f]")));
    const QByteArray data = QString("[Profile]\nVersion = 1\nName = %1\n\n[AccessKey]\n%2 = %3\n\n[Interface]\nMode = vpn\nName = Bimeron\nMTU = 1420\nDNS = 1.1.1.1\nIPv6 = disable\nKillSwitch = false\n\n[Routing]\nDefault = proxy\n")
                                .arg(safeName, field, trimmed).toUtf8();
    const QString destination = QDir(root_).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".conf");
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) {
        if (error) *error = output.errorString();
        return {};
    }
    QFile::setPermissions(destination, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
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
    for (const QString &line : configText.split('\n')) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            section = trimmed.mid(1, trimmed.size() - 2);
            continue;
        }
        if (trimmed.startsWith('#') || trimmed.startsWith(';')) continue;
        const qsizetype equal = trimmed.indexOf('=');
        if (equal < 0) continue;
        const QString key = trimmed.left(equal).trimmed();
        const QString field = trimmed.mid(equal + 1).trimmed();
        if (section == "Interface" && key.compare("Mode", Qt::CaseInsensitive) == 0)
            info.interfaceMode = field.toLower();
        if (section != "AccessKey") continue;
        if (key.compare("Type", Qt::CaseInsensitive) == 0) type = field.toLower();
        else if (QStringList{"SS", "SSConf", "SOCKS5", "VLESS", "Key", "File"}.contains(key, Qt::CaseInsensitive)) {
            source = key;
            value = field;
        }
    }
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
        const QUrl uri(value);
        info.connectable = !info.protocol.isEmpty() && uri.isValid()
            && uri.scheme().compare(scheme, Qt::CaseInsensitive) == 0
            && (scheme == "ss" || (!uri.host().isEmpty() && (scheme == "ssconf" || uri.port() > 0)));
        if (scheme == "vless")
            info.connectable = info.connectable && QRegularExpression(
                QStringLiteral("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$"))
                .match(uri.userName()).hasMatch();
    } else if (type == "shadowsocks" || type == "ss") {
        info.protocol = QStringLiteral("Shadowsocks");
        info.connectable = true;
    } else if (type == "socks5") {
        info.protocol = QStringLiteral("SOCKS5");
        info.connectable = true;
    } else if (type == "vless") {
        info.protocol = QStringLiteral("VLESS");
        info.connectable = true;
    }
    return info;
}
