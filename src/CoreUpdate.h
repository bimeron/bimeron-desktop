#pragma once

#include <QByteArray>
#include <QList>
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
    // Core's key ID: lowercase hex of the first 8 bytes of SHA-256 over the raw public key.
    static QByteArray keyId(const QByteArray &publicKey);
    // Parses comma-separated canonical standard-base64 32-byte keys. An empty string is an
    // empty list. Malformed entries or duplicate keys or key IDs leave *keys empty and fail.
    static bool parseTrustedKeys(const QByteArray &encoded, QList<QByteArray> *keys);
    // Selects the trusted key whose key ID equals the manifest key_id and verifies the
    // signature under that key only.
    static bool verifyManifest(const QByteArray &bytes, const QByteArray &signature,
                               const QList<QByteArray> &trustedKeys, const QString &releaseTag,
                               const QString &guiVersion, int ipcVersion, int profileFormat,
                               const QString &os, const QString &arch, bool betaAllowed,
                               CoreUpdateRelease *result, QString *error);
    static bool extractExecutable(const QByteArray &zip, const CoreUpdateRelease &release,
                                  const QString &os, const QString &arch,
                                  QByteArray *executable, QString *error);
};
