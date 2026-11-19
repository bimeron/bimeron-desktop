#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

struct Profile {
    QString id;
    QString name;
    QString path;
};

struct ProfileConnectionInfo {
    enum class Backend { Packet, NativeIKEv2, Unknown };
    Backend backend = Backend::Unknown;
    QString protocol;
    QString interfaceMode;
    QString keyFile;
    bool connectable = false; // Packet start eligibility; native sessions use a separate controller.
};

class ProfileStore {
public:
    explicit ProfileStore(const QString &root = {});
    QList<Profile> load(QString *error = nullptr) const;
    bool save(const QList<Profile> &profiles, QString *error = nullptr) const;
    static bool normalizeImport(QByteArray &data);
    QString importConfig(const QString &source, QString *error = nullptr) const;
    QString createConfig(const QString &name, const QString &key, QString *error = nullptr,
                         const QString &identityFile = {}, const QString &knownHostsFile = {}) const;
    static bool sshFilesReadable(const QString &configText, const QString &profilePath, QString *error = nullptr);
    static QString sectionName(const QString &line);
    static QString uriSourceField(const QString &uri);
    static QString identity(const QString &absolutePath);
    static QString displayName(const QString &configPath);
    static ProfileConnectionInfo connectionInfo(const QString &configText);
    static bool writeConfig(const QString &path, const QByteArray &data, QString *error = nullptr);
    static bool importReferencesSafe(const QByteArray &data, QString *error = nullptr);

    // WireGuard/AmneziaWG profiles are pairs: the Bimeron profile references a
    // key file through [AccessKey] File. Desktop owns key files named
    // "<profile base name>.<revision>.wg.conf" beside the profile; a save writes
    // the new key file under a fresh revision before the profile points to it,
    // so a failure at any step leaves the previous pair intact.
    static QString keyFileReference(const QString &profileText);
    static QString keyFilePath(const QString &profilePath, const QString &reference);
    static bool managedKeyFile(const QString &profilePath, const QString &reference);
    static QString nextKeyFileName(const QString &profilePath);
    // Writes profile (and, when keyFile is non-null, a new key file revision).
    // The profile's File reference is set to the written revision; the previous
    // managed revision is removed only after the profile replacement succeeds.
    static bool writeProfilePair(const QString &profilePath, const QByteArray &profile,
                                 const QByteArray *keyFile, QString *error = nullptr);
    // Creates a new stored pair from a profile template and key file bytes.
    QString createPair(const QByteArray &profile, const QByteArray &keyFile, QString *error = nullptr) const;
    // Removes a profile and its managed key file; never a file outside the pair.
    static bool removeProfileFiles(const QString &profilePath, QString *error = nullptr);
    // UTF-8 text with a leading BOM kept as U+FEFF, so toUtf8() restores the exact bytes.
    // QString::fromUtf8 drops the BOM, which would rewrite key files on save.
    static QString keyFileText(const QByteArray &data);
    static bool readKeyFile(const QString &profilePath, const QString &reference, QByteArray *data, QString *error = nullptr);

private:
    QString root_;
    QString index_;
};

