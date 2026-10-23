#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

struct Profile {
    QString id;
    QString name;
    QString path;
};

class ProfileStore {
public:
    ProfileStore();
    QList<Profile> load(QString *error = nullptr) const;
    bool save(const QList<Profile> &profiles, QString *error = nullptr) const;
    QString importConfig(const QString &source, QString *error = nullptr) const;
    QString createConfig(const QString &name, const QString &key, QString *error = nullptr) const;
    static QString identity(const QString &absolutePath);
    static QString displayName(const QString &configPath);

private:
    QString root_;
    QString index_;
};

