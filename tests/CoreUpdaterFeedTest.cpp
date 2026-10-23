#include "CoreUpdater.h"
#include "monocypher-ed25519.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <cstring>

namespace {
void require(bool ok, const char *message) {
    if (!ok) { qCritical("%s", message); std::abort(); }
}

bool write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "cannot read fixture");
    return file.readAll();
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationVersion("0.2.0");
    require(argc == 5, "usage: core-updater-feed-test <candidate-manifest> <windows-amd64-zip> <test-seed-file> <trace-file>");
    QFile trace(QString::fromLocal8Bit(argv[4]));
    require(trace.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot open trace");
    const auto log = [&](const QString &line) { trace.write((line + '\n').toUtf8()); trace.flush(); };
    QTemporaryDir dir;
    require(dir.isValid(), "cannot create fixture directory");
    const QString archiveName = QStringLiteral("bimeron-v0.2.0-beta.1-windows-amd64.zip");
    const QString archivePath = dir.filePath(archiveName);
    const QByteArray originalZip = read(QString::fromLocal8Bit(argv[2]));
    require(write(archivePath, originalZip), "cannot write fixture ZIP");

    const QByteArray seedBytes = read(QString::fromLocal8Bit(argv[3]));
    require(seedBytes.size() == 32, "test signing seed must be 32 bytes");
    uint8_t seed[32]{}, secret[64]{}, publicBytes[32]{};
    std::memcpy(seed, seedBytes.constData(), sizeof seed);
    crypto_ed25519_key_pair(secret, publicBytes, seed);
    const QByteArray publicKey(reinterpret_cast<const char *>(publicBytes), 32);
    require(publicKey.toBase64() == QByteArray(BIMERON_CORE_UPDATE_PUBLIC_KEY), "test key mismatch");
    const QString keyId = QString::fromLatin1(QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).left(8).toHex());
    QJsonObject manifest = QJsonDocument::fromJson(read(QString::fromLocal8Bit(argv[1]))).object();
    require(manifest.value("tag").toString() == "v0.2.0-beta.1" && !manifest.isEmpty(), "wrong candidate manifest");
    manifest.insert("key_id", keyId);
    QJsonObject compatibility = manifest.value("compatibility").toObject();
    require(!compatibility.value("gui_reviewed").toBool(), "candidate is already reviewed");

    QJsonArray assets;
    for (const QString &name : {QStringLiteral("update-manifest.json"), QStringLiteral("update-manifest.json.sig"), archiveName})
        assets.append(QJsonObject{{"name", name}, {"browser_download_url", QUrl::fromLocalFile(dir.filePath(name)).toString()}});
    const QJsonArray releases{QJsonObject{{"tag_name", "v0.2.0-beta.1"}, {"draft", false},
                                          {"prerelease", true}, {"assets", assets}}};
    const QString listPath = dir.filePath("releases.json");
    require(write(listPath, QJsonDocument(releases).toJson()), "cannot write release list");
    qputenv("BIMERON_TEST_RELEASES_JSON", QFile::encodeName(listPath));

    const auto run = [&](bool reviewed, bool badSignature, bool badZip) {
        compatibility.insert("gui_reviewed", reviewed);
        manifest.insert("compatibility", compatibility);
        const QByteArray bytes = QJsonDocument(manifest).toJson();
        uint8_t signatureBytes[64]{};
        crypto_ed25519_sign(signatureBytes, secret, reinterpret_cast<const uint8_t *>(bytes.constData()), size_t(bytes.size()));
        QByteArray signature = QByteArray(reinterpret_cast<const char *>(signatureBytes), 64).toBase64() + '\n';
        if (badSignature) signature[0] = signature[0] == 'A' ? 'B' : 'A';
        CoreUpdateRelease selected;
        QString error;
        const bool valid = CoreUpdate::verifyManifest(bytes, signature, publicKey, "v0.2.0-beta.1", "0.2.0",
                                                      1, 1, "windows", "amd64", true, &selected, &error);
        if (!reviewed) require(!valid && error == "Unreviewed or incomplete update compatibility", "unreviewed gate was not reached");
        else if (badSignature) require(!valid && error == "Update signature verification failed", "signature gate was not reached");
        else require(valid, "reviewed signed fixture failed manifest verification");
        log(QStringLiteral("Manifest verification: valid=%1 error=%2").arg(valid).arg(error));
        require(write(dir.filePath("update-manifest.json"), bytes) &&
                write(dir.filePath("update-manifest.json.sig"), signature), "cannot write signed fixture");
        QByteArray zip = originalZip;
        if (badZip) zip[zip.size() - 1] ^= 1;
        require(write(archivePath, zip), "cannot write archive fixture");
        CoreUpdater updater;
        bool ready = false;
        QString status;
        QObject::connect(&updater, &CoreUpdater::ready, &app, [&](QString version, QString path, QString digest) {
            ready = version == "0.2.0-beta.1" && QFileInfo(path).exists() && digest.size() == 64;
        });
        QObject::connect(&updater, &CoreUpdater::statusChanged, &app, [&](QString value) { status = value; });
        updater.check("0.1.0-beta.4", 1, true);
        return qMakePair(ready, status);
    };

    const auto unreviewed = run(false, false, false);
    log(QStringLiteral("Unreviewed result: ready=%1 status=%2").arg(unreviewed.first).arg(unreviewed.second));
    require(!unreviewed.first, "signed unreviewed manifest accepted");
    const auto badSignature = run(true, true, false);
    log(QStringLiteral("Bad signature result: ready=%1 status=%2").arg(badSignature.first).arg(badSignature.second));
    require(!badSignature.first, "wrong manifest signature accepted");
    const auto corrupted = run(true, false, true);
    log(QStringLiteral("Changed ZIP result: ready=%1 status=%2").arg(corrupted.first).arg(corrupted.second));
    require(!corrupted.first && corrupted.second.contains("archive size or hash mismatch"), "changed ZIP accepted");
    require(run(true, false, false).first, "reviewed signed archive rejected");
    return 0;
}
