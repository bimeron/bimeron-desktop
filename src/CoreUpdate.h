#pragma once

#include <QByteArray>
#include <QString>

struct CoreUpdateRelease {
    QString tag;
    QString version;
    QString assetName;
    qint64 assetSize = 0;
    QByteArray assetSha256;
};

class CoreUpdate final {
public:
    static bool newerVersion(const QString &candidate, const QString &installed);
    static bool verifyManifest(const QByteArray &bytes, const QByteArray &signature,
                               const QByteArray &publicKey, const QString &releaseTag,
                               const QString &guiVersion, int ipcVersion, int profileFormat,
                               const QString &os, const QString &arch, bool betaAllowed,
                               CoreUpdateRelease *result, QString *error);
    static bool extractExecutable(const QByteArray &zip, const CoreUpdateRelease &release,
                                  const QString &os, const QString &arch,
                                  QByteArray *executable, QString *error);
};
