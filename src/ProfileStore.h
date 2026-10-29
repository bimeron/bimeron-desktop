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
    QString protocol;
    QString interfaceMode;
    QString keyFile;
    bool connectable = false;
};

class ProfileStore {
public:
    explicit ProfileStore(const QString &root = {});
    QList<Profile> load(QString *error = nullptr) const;
    bool save(const QList<Profile> &profiles, QString *error = nullptr) const;
    QString importConfig(const QString &source, QString *error = nullptr) const;
    QString createConfig(const QString &name, const QString &key, QString *error = nullptr,
                         const QString &identityFile = {}, const QString &knownHostsFile = {}) const;
    static bool sshFilesReadable(const QString &configText, const QString &profilePath, QString *error = nullptr);
    static QString identity(const QString &absolutePath);
    static QString displayName(const QString &configPath);
    static ProfileConnectionInfo connectionInfo(const QString &configText);
    static bool writeConfig(const QString &path, const QByteArray &data, QString *error = nullptr);
    static bool importReferencesSafe(const QByteArray &data, QString *error = nullptr);

private:
    QString root_;
    QString index_;
};

