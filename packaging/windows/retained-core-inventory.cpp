#include "CoreUpdate.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cstdio>

namespace {
QByteArray readBounded(const QString &path, qint64 limit) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > limit) return {};
    const QByteArray data = file.read(limit + 1);
    return file.error() == QFileDevice::NoError && data.size() <= limit ? data : QByteArray{};
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 3) return 1;
    const QFileInfo archive(args[1]);
    const auto manifestPath = archive.dir().filePath("update-manifest.json");
    const auto manifest = readBounded(manifestPath, 1024 * 1024);
    const auto signature = readBounded(manifestPath + ".sig", 1024);
    const auto tag = QJsonDocument::fromJson(manifest).object().value("tag").toString();
    QList<QByteArray> keys;
    CoreUpdateRelease release;
    QString error;
    if (!CoreUpdate::parseTrustedKeys(BIMERON_CORE_UPDATE_PUBLIC_KEYS, &keys)
        || !CoreUpdate::verifyManifest(manifest, signature, keys, tag, BIMERON_GUI_VERSION, 1, 1,
                                       "windows", BIMERON_INVENTORY_ARCH, BIMERON_INVENTORY_ALLOW_BETA,
                                       &release, &error)) {
        std::fprintf(stderr, "Retained Core manifest rejected: %s\n", qPrintable(error));
        return 1;
    }
    QByteArray image;
    if (archive.fileName() != release.assetName || !CoreUpdate::extractExecutable(
            readBounded(archive.absoluteFilePath(), release.assetSize), release, "windows",
            BIMERON_INVENTORY_ARCH, &image, &error)) {
        std::fprintf(stderr, "Retained Core archive rejected: %s\n", qPrintable(error));
        return 1;
    }
    const auto digest = QCryptographicHash::hash(image, QCryptographicHash::Sha256).toHex();
    const QByteArray pin = "#define BIMERON_COMPATIBLE_RETAINED_CORE_SHA256 \"" + digest + "\"\n";
    if (readBounded(args[2], pin.size()) == pin) return 0;
    QSaveFile output(args[2]);
    return output.open(QIODevice::WriteOnly) && output.write(pin) == pin.size() && output.commit() ? 0 : 1;
}
