#include "CoreUpdate.h"
#include "CoreUpdater.h"
#include "TrustedCore.h"
#include "monocypher-ed25519.h"
#include "miniz.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#endif

static void require(bool condition, const char *message) {
    if (!condition) { fprintf(stderr, "%s\n", message); fflush(stderr); qFatal("%s", message); }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == "--record-arguments") {
        QFile record(QString::fromLocal8Bit(argv[2]));
        QJsonArray received;
        for (int i = 3; i < argc; ++i) received.append(QString::fromLocal8Bit(argv[i]));
        return record.open(QIODevice::WriteOnly) && record.write(QJsonDocument(received).toJson()) > 0 ? 0 : 1;
    }
    CoreUpdater configured;
    const QByteArray configuredKey = QByteArray::fromBase64(BIMERON_CORE_UPDATE_PUBLIC_KEY);
    require(configured.available() == (configuredKey.size() == 32), "configured signing key availability is wrong");
    if (!configured.available()) {
        QString unavailable;
        QObject::connect(&configured, &CoreUpdater::statusChanged, &app, [&](const QString &message) { unavailable = message; });
        configured.check("0.1.0", 1, false);
        require(unavailable == "Signed updates are not available yet", "missing-key state is unclear");
    }
    QTemporaryDir writablePackage;
    require(writablePackage.isValid(), "cannot create writable package fixture");
    QFile fakeCore(writablePackage.filePath("bimeron-core"));
    require(fakeCore.open(QIODevice::WriteOnly) && fakeCore.write("fixture") == 7, "cannot create core fixture");
    fakeCore.close();
    require(!TrustedCore::protectedExecutable(fakeCore.fileName()), "writable package accepted for elevation");
#ifdef Q_OS_WIN
    wchar_t systemDirectory[MAX_PATH]{};
    require(GetSystemDirectoryW(systemDirectory, MAX_PATH) > 0 &&
            TrustedCore::protectedExecutable(QString::fromWCharArray(systemDirectory) + QStringLiteral("/kernel32.dll")),
            "protected Windows system file was rejected");
#else
    require(TrustedCore::protectedExecutable(QStringLiteral("/usr/bin/bash")), "protected Linux system file was rejected");
#endif
    const QString bundled = QCoreApplication::applicationDirPath() +
#ifdef Q_OS_WIN
        QStringLiteral("/bimeron-core.exe");
#else
        QStringLiteral("/bimeron-core");
#endif
    if (qEnvironmentVariableIsSet("BIMERON_TEST_EXPECT_SOURCE_HELPER"))
        require(QFileInfo::exists(bundled) && TrustedCore::supportsSourceUpdate(bundled),
                "packaged core does not support the source update protocol");
    require(TrustedCore::updateArguments("relative", "1.2.0", QString(64, 'a')).isEmpty(), "relative update source accepted");
    const QString healthFailure = QStringLiteral("updated service IPC health check failed; rollback completed");
    require(TrustedCore::failureMessage(1, healthFailure) == healthFailure,
            "IPC hello and rollback diagnostic was lost");
    require(TrustedCore::failureMessage(1, {}).contains("Checking service state"),
            "Windows update failure does not explain the service recheck");
    const QString source = QDir::tempPath() + QStringLiteral("/update with spaces/bimeron.exe");
    const QStringList command = TrustedCore::updateArguments(source, "1.2.0", QString(64, 'a'));
    require(command.size() == 8 && command[2] == "-source" && command[3] == source, "update source argument missing");
    const QString recordPath = writablePackage.filePath("arguments.json");
    QProcess recorder;
    QStringList recordArguments{"--record-arguments", recordPath};
    recordArguments += command;
    recorder.start(QCoreApplication::applicationFilePath(), recordArguments);
    require(recorder.waitForFinished(5000) && recorder.exitStatus() == QProcess::NormalExit && recorder.exitCode() == 0,
            "full update CLI invocation failed");
    QFile record(recordPath);
    require(record.open(QIODevice::ReadOnly) && QJsonDocument::fromJson(record.readAll()).array().toVariantList() ==
            QJsonArray::fromStringList(command).toVariantList(), "full update CLI arguments changed in transit");
    require(TrustedCore::windowsCommandLine({"-source", QStringLiteral("C:\\Update Files\\bimeron.exe")}) ==
            QStringLiteral("\"-source\" \"C:\\Update Files\\bimeron.exe\""), "Windows source path quoting failed");
    const QString tricky = TrustedCore::windowsCommandLine({QStringLiteral("a\\\"b\\")});
    require(tricky == QStringLiteral("\"a") + QString(3, '\\') + QStringLiteral("\"b") +
            QString(2, '\\') + QLatin1Char('"'), "Windows quote and trailing slash quoting failed");
#ifdef Q_OS_WIN
    int parsedCount = 0;
    const std::wstring line = TrustedCore::windowsCommandLine(command).toStdWString();
    LPWSTR *parsed = CommandLineToArgvW(line.c_str(), &parsedCount);
    require(parsed && parsedCount == command.size(), "Windows update command line was not parsed correctly");
    for (int i = 0; i < parsedCount; ++i)
        require(QString::fromWCharArray(parsed[i]) == command[i], "Windows update argument changed during quoting");
    LocalFree(parsed);
#endif
    const QByteArray rfcPublic = QByteArray::fromHex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    const QByteArray rfcSignature = QByteArray::fromHex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
                                                    "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    require(crypto_ed25519_check(reinterpret_cast<const uint8_t *>(rfcSignature.constData()),
                                 reinterpret_cast<const uint8_t *>(rfcPublic.constData()), nullptr, 0) == 0,
            "RFC 8032 Ed25519 signature rejected");
    uint8_t seed[32]{};
    uint8_t secret[64]{};
    uint8_t publicBytes[32]{};
    crypto_ed25519_key_pair(secret, publicBytes, seed);
    const QByteArray publicKey(reinterpret_cast<char *>(publicBytes), 32);
    const QString keyId = QString::fromLatin1(QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).left(8).toHex());
    QJsonArray archives;
    for (const QString &os : {QStringLiteral("windows"), QStringLiteral("linux")})
        for (const QString &arch : {QStringLiteral("amd64"), QStringLiteral("arm64")})
            archives.append(QJsonObject{{"os", os}, {"arch", arch}, {"name", QStringLiteral("bimeron-v1.2.0-%1-%2.zip").arg(os, arch)},
                                        {"size", 100}, {"sha256", QString(64, 'a')}});
    QJsonObject manifest{{"schema", 1}, {"tag", "v1.2.0"}, {"core_version", "1.2.0"}, {"channel", "stable"},
                         {"key_id", keyId}, {"compatibility", QJsonObject{{"schema", 1}, {"ipc_min", 1}, {"ipc_max", 1},
                         {"gui_min", "0.1.0"}, {"gui_max_exclusive", "0.1.2"}, {"gui_reviewed", true}, {"profile_format", 1}}},
                         {"archives", archives}};
    auto sign = [&](const QJsonObject &object) {
        const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
        uint8_t signature[64];
        crypto_ed25519_sign(signature, secret, reinterpret_cast<const uint8_t *>(bytes.constData()), size_t(bytes.size()));
        return std::pair(bytes, QByteArray(reinterpret_cast<char *>(signature), 64).toBase64() + '\n');
    };
    auto [bytes, signature] = sign(manifest);
    auto verify = [&](const QByteArray &body, const QByteArray &sig, const QByteArray &key = QByteArray()) {
        CoreUpdateRelease selected;
        QString error;
        return CoreUpdate::verifyManifest(body, sig, key.isEmpty() ? publicKey : key, "v1.2.0", "0.1.1", 1, 1,
                                          "windows", "amd64", false, &selected, &error);
    };
    require(verify(bytes, signature), "valid signed manifest rejected");
    CoreUpdateRelease nextGuiSelected;
    QString nextGuiError;
    require(!CoreUpdate::verifyManifest(bytes, signature, publicKey, "v1.2.0", "0.1.2", 1, 1,
                                        "windows", "amd64", false, &nextGuiSelected, &nextGuiError),
            "GUI 0.1.2 accepted the existing exclusive compatibility bound");
    auto nextGuiManifest = manifest;
    auto nextGuiCompatibility = nextGuiManifest.value("compatibility").toObject();
    nextGuiCompatibility.insert("gui_max_exclusive", "0.1.3");
    nextGuiManifest.insert("compatibility", nextGuiCompatibility);
    auto [nextGuiBytes, nextGuiSignature] = sign(nextGuiManifest);
    require(CoreUpdate::verifyManifest(nextGuiBytes, nextGuiSignature, publicKey, "v1.2.0", "0.1.2", 1, 1,
                                       "windows", "amd64", false, &nextGuiSelected, &nextGuiError),
            "GUI 0.1.2 rejected reviewed signed future compatibility bounds");
    CoreUpdateRelease previousGuiSelected;
    QString previousGuiError;
    require(CoreUpdate::verifyManifest(bytes, signature, publicKey, "v1.2.0", "0.1.0", 1, 1,
                                       "windows", "amd64", false, &previousGuiSelected, &previousGuiError),
            "previous GUI version lost compatibility");
    QByteArray tampered = bytes; tampered[tampered.indexOf("1.2.0")] = '9';
    require(!verify(tampered, signature), "tampered manifest accepted");
    require(!verify(bytes, signature, QByteArray(32, '\1')), "unknown key accepted");
    auto invalid = manifest;
    auto compatibility = invalid.value("compatibility").toObject();
    compatibility.insert("gui_reviewed", false);
    invalid.insert("compatibility", compatibility);
    auto [unreviewed, unreviewedSig] = sign(invalid);
    require(!verify(unreviewed, unreviewedSig), "unreviewed release accepted");
    invalid = manifest;
    auto duplicate = invalid.value("archives").toArray();
    duplicate[1] = duplicate[0]; invalid.insert("archives", duplicate);
    auto [duplicateBytes, duplicateSig] = sign(invalid);
    require(!verify(duplicateBytes, duplicateSig), "duplicate target accepted");
    invalid = manifest;
    auto unsafe = invalid.value("archives").toArray();
    auto entry = unsafe[0].toObject(); entry.insert("name", "../bad.zip"); unsafe[0] = entry; invalid.insert("archives", unsafe);
    auto [unsafeBytes, unsafeSig] = sign(invalid);
    require(!verify(unsafeBytes, unsafeSig), "unsafe asset name accepted");
    invalid = manifest;
    compatibility = invalid.value("compatibility").toObject();
    compatibility.insert("gui_max_exclusive", "0.1.1");
    invalid.insert("compatibility", compatibility);
    auto [oldBounds, oldBoundsSig] = sign(invalid);
    require(!verify(oldBounds, oldBoundsSig), "GUI 0.1.1 accepted the old exclusive compatibility bound");
    invalid = manifest;
    compatibility = invalid.value("compatibility").toObject();
    compatibility.insert("gui_max_exclusive", "0.1.0");
    invalid.insert("compatibility", compatibility);
    auto [excludedBytes, excludedSig] = sign(invalid);
    require(!verify(excludedBytes, excludedSig), "excluded GUI version accepted");
    invalid = manifest;
    compatibility = invalid.value("compatibility").toObject();
    compatibility.insert("ipc_min", 2);
    invalid.insert("compatibility", compatibility);
    auto [ipcBytes, ipcSig] = sign(invalid);
    require(!verify(ipcBytes, ipcSig), "unsupported IPC version accepted");
    require(CoreUpdate::newerVersion("1.2.0", "1.2.0-beta.2"), "stable should follow beta");
    require(!CoreUpdate::newerVersion("1.2.0-beta.2", "1.2.0"), "beta should precede stable");

    mz_zip_archive writer{};
    require(mz_zip_writer_init_heap(&writer, 0, 0), "cannot create ZIP fixture");
    const QByteArray payload("executable fixture");
    require(mz_zip_writer_add_mem(&writer, "windows-amd64/bimeron.exe", payload.data(), size_t(payload.size()), MZ_BEST_COMPRESSION), "cannot add ZIP member");
    void *zipData = nullptr; size_t zipSize = 0;
    require(mz_zip_writer_finalize_heap_archive(&writer, &zipData, &zipSize), "cannot finalize ZIP fixture");
    const QByteArray zip(reinterpret_cast<char *>(zipData), qsizetype(zipSize));
    mz_free(zipData); mz_zip_writer_end(&writer);
    CoreUpdateRelease release{"v1.2.0", "1.2.0", "fixture.zip", zip.size(), QCryptographicHash::hash(zip, QCryptographicHash::Sha256)};
    QByteArray extracted; QString error;
    require(CoreUpdate::extractExecutable(zip, release, "windows", "amd64", &extracted, &error) && extracted == payload, "valid ZIP rejected");
    release.assetSize++;
    require(!CoreUpdate::extractExecutable(zip, release, "windows", "amd64", &extracted, &error), "wrong ZIP size accepted");
    release.assetSize--;
    release.assetSha256 = QByteArray(32, 0);
    require(!CoreUpdate::extractExecutable(zip, release, "windows", "amd64", &extracted, &error), "wrong ZIP hash accepted");
    mz_zip_archive badWriter{};
    require(mz_zip_writer_init_heap(&badWriter, 0, 0), "cannot create unsafe ZIP fixture");
    require(mz_zip_writer_add_mem(&badWriter, "../escape", payload.data(), size_t(payload.size()), MZ_BEST_COMPRESSION), "cannot add unsafe ZIP member");
    require(mz_zip_writer_add_mem(&badWriter, "windows-amd64/bimeron.exe", payload.data(), size_t(payload.size()), MZ_BEST_COMPRESSION), "cannot add executable member");
    void *badData = nullptr; size_t badSize = 0;
    require(mz_zip_writer_finalize_heap_archive(&badWriter, &badData, &badSize), "cannot finalize unsafe ZIP fixture");
    const QByteArray unsafeZip(reinterpret_cast<char *>(badData), qsizetype(badSize));
    mz_free(badData); mz_zip_writer_end(&badWriter);
    release.assetSize = unsafeZip.size();
    release.assetSha256 = QCryptographicHash::hash(unsafeZip, QCryptographicHash::Sha256);
    require(!CoreUpdate::extractExecutable(unsafeZip, release, "windows", "amd64", &extracted, &error), "unsafe ZIP member accepted");
    if (argc == 4 && QString::fromLocal8Bit(argv[1]) != "--candidate-manifest") {
        QFile archiveFile(QString::fromLocal8Bit(argv[1]));
        require(archiveFile.open(QIODevice::ReadOnly), "cannot open real core archive");
        const QByteArray realZip = archiveFile.readAll();
        CoreUpdateRelease realRelease{"v0.1.0-beta.2", "0.1.0-beta.2", "local-fixture.zip", realZip.size(),
                                      QCryptographicHash::hash(realZip, QCryptographicHash::Sha256)};
        require(CoreUpdate::extractExecutable(realZip, realRelease, QString::fromLocal8Bit(argv[2]),
                                             QString::fromLocal8Bit(argv[3]), &extracted, &error),
                "real core archive extraction failed");
        require(!extracted.isEmpty(), "real core executable is empty");
    }
    if (argc == 5 && QString::fromLocal8Bit(argv[1]) == "--manifest") {
        QFile manifestFile(QString::fromLocal8Bit(argv[2]));
        QFile signatureFile(QString::fromLocal8Bit(argv[3]));
        require(manifestFile.open(QIODevice::ReadOnly) && signatureFile.open(QIODevice::ReadOnly), "cannot open core-generated manifest fixture");
        const QByteArray pub = QByteArray::fromBase64(QByteArray(argv[4]));
        CoreUpdateRelease selected;
        require(pub == configuredKey, "manifest fixture key differs from pinned GUI key");
        const QByteArray manifest = manifestFile.readAll(), signature = signatureFile.readAll();
        require(CoreUpdate::verifyManifest(manifest, signature, configuredKey, "v0.1.0-beta.3",
                                           "0.1.1", 1, 1, "windows", "amd64", true, &selected, &error),
                "core-generated manifest is incompatible with GUI verifier");
        require(selected.version == "0.1.0-beta.3" && selected.assetName == "bimeron-v0.1.0-beta.3-windows-amd64.zip",
                "reviewed manifest selected the wrong core archive");
        QByteArray tampered = manifest;
        tampered[tampered.indexOf("0.1.0-beta.3")] = '9';
        require(!CoreUpdate::verifyManifest(tampered, signature, configuredKey, "v0.1.0-beta.3",
                                            "0.1.1", 1, 1, "windows", "amd64", true, &selected, &error) &&
                error == "Update signature verification failed", "tampered reviewed manifest was accepted");
    }
    if (argc == 4 && QString::fromLocal8Bit(argv[1]) == "--candidate-manifest") {
        QFile manifestFile(QString::fromLocal8Bit(argv[2]));
        QFile signatureFile(QString::fromLocal8Bit(argv[3]));
        require(manifestFile.open(QIODevice::ReadOnly) && signatureFile.open(QIODevice::ReadOnly), "cannot open candidate manifest");
        const QByteArray candidate = manifestFile.readAll(), candidateSignature = signatureFile.readAll();
        require(QCryptographicHash::hash(configuredKey, QCryptographicHash::Sha256).left(8).toHex() == "7370a2d8cf890e5e",
                "candidate key ID does not match the configured key");
        CoreUpdateRelease selected;
        require(!CoreUpdate::verifyManifest(candidate, candidateSignature, configuredKey, "v0.1.0-beta.3",
                                            "0.1.1", 1, 1, "windows", "amd64", true, &selected, &error) &&
                error == "Unreviewed or incomplete update compatibility", "candidate signature or review gate failed");
        QByteArray tampered = candidate;
        tampered[tampered.indexOf("0.1.0-beta.3")] = '9';
        require(!CoreUpdate::verifyManifest(tampered, candidateSignature, configuredKey, "v0.1.0-beta.3",
                                            "0.1.1", 1, 1, "windows", "amd64", true, &selected, &error) &&
                error == "Update signature verification failed", "tampered candidate was not rejected by signature");
        require(!CoreUpdate::verifyManifest(candidate, candidateSignature, QByteArray(32, '\1'), "v0.1.0-beta.3",
                                            "0.1.1", 1, 1, "windows", "amd64", true, &selected, &error) &&
                error == "Unknown update signing key", "unknown candidate key was accepted");
    }
    if (argc == 9 && QString::fromLocal8Bit(argv[1]) == "--verify-release") {
        QFile manifestFile(QString::fromLocal8Bit(argv[2]));
        QFile signatureFile(QString::fromLocal8Bit(argv[3]));
        QFile archiveFile(QString::fromLocal8Bit(argv[4]));
        require(manifestFile.open(QIODevice::ReadOnly) && signatureFile.open(QIODevice::ReadOnly) &&
                archiveFile.open(QIODevice::ReadOnly), "cannot open prepared release files");
        CoreUpdateRelease selected;
        require(CoreUpdate::verifyManifest(manifestFile.readAll(), signatureFile.readAll(), configuredKey,
                                           QString::fromLocal8Bit(argv[5]), QString::fromLocal8Bit(argv[6]), 1, 1,
                                           QString::fromLocal8Bit(argv[7]), QString::fromLocal8Bit(argv[8]), true,
                                           &selected, &error), "prepared release rejected by GUI verifier");
        require(QFileInfo(archiveFile).fileName() == selected.assetName, "prepared archive name differs from signed target");
        require(CoreUpdate::extractExecutable(archiveFile.readAll(), selected, QString::fromLocal8Bit(argv[7]),
                                              QString::fromLocal8Bit(argv[8]), &extracted, &error),
                "prepared archive rejected by GUI extractor");
        require(!extracted.isEmpty(), "prepared core executable is empty");
    }
    return 0;
}
