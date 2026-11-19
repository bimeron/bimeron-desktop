#include "CoreUpdate.h"
#include "CoreUpdater.h"
#include "TrustedCore.h"
#include "RepairBootstrap.h"
#include "RepairBootstrap_p.h"
#include "monocypher-ed25519.h"
#include "miniz.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <algorithm>
#include <cstring>
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
    require(!RepairBootstrap::available(), "test binary must not embed privileged payload");
    require(!RepairBootstrap::authenticatedChild(0, 0), "bootstrap rejects absent parent/process identity");
    require(!TrustedCore::protectedDirectory("relative/path"), "bootstrap rejects relative output directory");
    require(!TrustedCore::protectedDirectory(QCoreApplication::applicationFilePath()), "bootstrap rejects regular file as directory");
    {
        QTemporaryDir fixture;
        require(fixture.isValid(), "bootstrap fixture directory");
        const QString target = fixture.filePath("repair/bimeron-repair");
        const auto directoryPolicy = [&](const QString &path) {
            return path.startsWith(fixture.path()) && QFileInfo(path).isDir() && !QFileInfo(path).isSymLink();
        };
        const auto filePolicy = [&](const QString &path) {
            return directoryPolicy(QFileInfo(path).absolutePath()) && QFileInfo(path).isFile() && !QFileInfo(path).isSymLink();
        };
        QString error;
        const auto provision = [&](const QByteArray &bytes) {
            return RepairBootstrap::detail::provision(target, bytes, directoryPolicy, filePolicy, &error);
        };
        require(provision("embedded fixture"), "missing package broker restored");
        const auto modified = QFileInfo(target).lastModified();
        require(provision("embedded fixture") && QFileInfo(target).lastModified() == modified,
            "identical package broker untouched");
        require(!provision("different") && error.contains(target), "differing package broker reported");
        QFile actual(target);
        require(actual.open(QIODevice::ReadOnly) && actual.readAll() == "embedded fixture", "package bytes retained");
        actual.close();
        require(!RepairBootstrap::detail::provision(fixture.filePath("unsafe/broker"), "fixture",
            [](const QString &) { return false; }, filePolicy, &error), "unsafe parent refused");
        require(!QFileInfo::exists(fixture.filePath("unsafe")), "unsafe parent not created");
        const QString raced = fixture.filePath("repair/raced");
        const auto racingPolicy = [&](const QString &path) {
            if (QFileInfo(path).fileName().startsWith(".repair-")) {
                QFile competitor(raced);
                require(competitor.open(QIODevice::WriteOnly) && competitor.write("package-owned") == 13,
                    "create concurrent package file");
            }
            return filePolicy(path);
        };
        require(!RepairBootstrap::detail::provision(raced, "embedded fixture", directoryPolicy, racingPolicy, &error),
            "atomic publication refuses concurrent differing file");
        QFile concurrent(raced);
        require(concurrent.open(QIODevice::ReadOnly) && concurrent.readAll() == "package-owned",
            "concurrent package file never overwritten");
        require(QDir(fixture.filePath("repair")).entryList({".repair-*"}, QDir::Files | QDir::Hidden).isEmpty(),
            "failed publication removes temporary file");
        for (const QString &name : {QString("wintun.dll"), QString("WINTUN-LICENSE.txt")}) {
            const QString adjacent = fixture.filePath("repair/" + name);
            require(RepairBootstrap::detail::provision(adjacent, "runtime fixture", directoryPolicy, filePolicy, &error),
                "missing adjacent runtime file restored");
            require(RepairBootstrap::detail::provision(adjacent, "runtime fixture", directoryPolicy, filePolicy, &error),
                "identical adjacent runtime retained");
            require(!RepairBootstrap::detail::provision(adjacent, "different runtime", directoryPolicy, filePolicy, &error),
                "different adjacent runtime refused");
        }

        require(!RepairBootstrap::detail::provision(target, "embedded fixture", directoryPolicy,
            [](const QString &) { return false; }, &error), "unsafe existing leaf refused");
#ifndef Q_OS_WIN
        require(QFile::remove(target) && QFile::link("missing-destination", target), "dangling symlink fixture");
        require(!provision("embedded fixture"), "dangling symlink refused");
        const QString redirected = fixture.filePath("redirected");
        require(QFile::link(fixture.filePath("repair"), redirected), "symlink parent fixture");
        require(!RepairBootstrap::detail::provision(redirected + "/broker", "fixture", directoryPolicy, filePolicy, &error),
            "actual symlink parent refused");

        require(!QFileInfo::exists(fixture.filePath("repair/missing-destination")), "symlink not followed");
        require(RepairBootstrap::detail::packageDirectory("/usr/bin/bimeron-desktop") ==
            "/usr/libexec/bimeron/repair", "Linux package path derived from GUI");
        require(RepairBootstrap::detail::packageDirectory("/tmp/bimeron-desktop").isEmpty(), "unsupported layout refused");
#else
        require(RepairBootstrap::detail::packageDirectory("C:/Program Files/Bimeron/bimeron-desktop.exe") ==
            "C:/Program Files/Bimeron/repair", "Windows package path derived from GUI");
#endif
    }
    CoreUpdater configured;
    QList<QByteArray> configuredKeys;
    require(CoreUpdate::parseTrustedKeys(BIMERON_CORE_UPDATE_PUBLIC_KEYS, &configuredKeys),
            "configured signing keys are malformed");
    {
        // CMake derived these IDs at configure time; both derivations must agree with Core's.
        const QByteArray configuredIds(BIMERON_CORE_UPDATE_KEY_IDS);
        const QList<QByteArray> expectedIds = configuredIds.isEmpty() ? QList<QByteArray>() : configuredIds.split(',');
        require(expectedIds.size() == configuredKeys.size(), "configured key ID count differs from key count");
        for (qsizetype i = 0; i < configuredKeys.size(); ++i)
            require(CoreUpdate::keyId(configuredKeys[i]) == expectedIds[i], "CMake key ID differs from the verifier's key ID");
    }
    require(configured.available() == !configuredKeys.isEmpty(), "configured signing key availability is wrong");
    if (!configured.available()) {
        QString unavailable;
        QObject::connect(&configured, &CoreUpdater::statusChanged, &app, [&](const QString &message) { unavailable = message; });
        configured.check("0.1.0", 1, false, "instance");
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
    // Key IDs follow Core's derivation (release/applemanifest KeyID and scripts/release-manifest.go).
    require(CoreUpdate::keyId(QByteArray::fromBase64("bk6SfABmDG88uhUupYT+ia6dtSrpHbnGaBCfYvihJF0=")) == "7370a2d8cf890e5e",
            "production key ID differs");
    require(CoreUpdate::keyId(QByteArray::fromBase64("O2onvM62pC1io6jQKm8Nc2UyFXcd4kOmOsBIoYtZ2ik=")) == "139e3940e64b5491",
            "key ID differs from Core's test vector");
    require(CoreUpdate::keyId(QByteArray(32, '\0')) == "66687aadf862bd77", "zero key ID differs");
    // Ephemeral signing keys: random seeds exist only in this process and are wiped by Monocypher.
    struct TestKey {
        uint8_t secret[64]{};
        QByteArray publicKey;
    };
    const auto ephemeralKey = [] {
        TestKey key;
        quint32 seed[8];
        QRandomGenerator::system()->fillRange(seed);
        uint8_t publicBytes[32]{};
        crypto_ed25519_key_pair(key.secret, publicBytes, reinterpret_cast<uint8_t *>(seed));
        key.publicKey = QByteArray(reinterpret_cast<char *>(publicBytes), 32);
        return key;
    };
    const TestKey current = ephemeralKey();
    const uint8_t *secret = current.secret;
    const QByteArray publicKey = current.publicKey;
    const QString keyId = QString::fromLatin1(CoreUpdate::keyId(publicKey));
    QJsonArray archives;
    for (const QString &os : {QStringLiteral("windows"), QStringLiteral("linux")})
        for (const QString &arch : {QStringLiteral("amd64"), QStringLiteral("arm64")})
            archives.append(QJsonObject{{"os", os}, {"arch", arch}, {"name", QStringLiteral("bimeron-v1.2.0-%1-%2.zip").arg(os, arch)},
                                        {"size", 100}, {"sha256", QString(64, 'a')}});
    QJsonObject manifest{{"schema", 1}, {"tag", "v1.2.0"}, {"core_version", "1.2.0"}, {"channel", "stable"},
                         {"key_id", keyId}, {"compatibility", QJsonObject{{"schema", 1}, {"ipc_min", 1}, {"ipc_max", 1},
                         {"gui_min", "0.1.0"}, {"gui_max_exclusive", "0.1.2"}, {"gui_reviewed", true}, {"profile_format", 1}}},
                         {"archives", archives}};
    auto signWith = [](const QJsonObject &object, const uint8_t *secretKey) {
        const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
        uint8_t signature[64];
        crypto_ed25519_sign(signature, secretKey, reinterpret_cast<const uint8_t *>(bytes.constData()), size_t(bytes.size()));
        return std::pair(bytes, QByteArray(reinterpret_cast<char *>(signature), 64).toBase64() + '\n');
    };
    auto sign = [&](const QJsonObject &object) { return signWith(object, secret); };
    auto [bytes, signature] = sign(manifest);
    auto verify = [&](const QByteArray &body, const QByteArray &sig, const QList<QByteArray> &keys = {}) {
        CoreUpdateRelease selected;
        QString error;
        return CoreUpdate::verifyManifest(body, sig, keys.isEmpty() ? QList<QByteArray>{publicKey} : keys, "v1.2.0",
                                          "0.1.1", 1, 1, "windows", "amd64", false, &selected, &error);
    };
    require(verify(bytes, signature), "valid signed manifest rejected");
    CoreUpdateRelease nextGuiSelected;
    QString nextGuiError;
    require(!CoreUpdate::verifyManifest(bytes, signature, {publicKey}, "v1.2.0", "0.1.2", 1, 1,
                                        "windows", "amd64", false, &nextGuiSelected, &nextGuiError),
            "GUI 0.1.2 accepted the existing exclusive compatibility bound");
    auto nextGuiManifest = manifest;
    auto nextGuiCompatibility = nextGuiManifest.value("compatibility").toObject();
    nextGuiCompatibility.insert("gui_max_exclusive", "0.1.3");
    nextGuiManifest.insert("compatibility", nextGuiCompatibility);
    auto [nextGuiBytes, nextGuiSignature] = sign(nextGuiManifest);
    require(CoreUpdate::verifyManifest(nextGuiBytes, nextGuiSignature, {publicKey}, "v1.2.0", "0.1.2", 1, 1,
                                       "windows", "amd64", false, &nextGuiSelected, &nextGuiError),
            "GUI 0.1.2 rejected reviewed signed future compatibility bounds");
    CoreUpdateRelease previousGuiSelected;
    QString previousGuiError;
    require(CoreUpdate::verifyManifest(bytes, signature, {publicKey}, "v1.2.0", "0.1.0", 1, 1,
                                       "windows", "amd64", false, &previousGuiSelected, &previousGuiError),
            "previous GUI version lost compatibility");
    QByteArray tampered = bytes; tampered[tampered.indexOf("1.2.0")] = '9';
    require(!verify(tampered, signature), "tampered manifest accepted");
    require(!verify(bytes, signature, {QByteArray(32, '\1')}), "unknown key accepted");
    {
        // Key rotation: the current key and two successors are pinned. The manifest key_id
        // selects one pinned key and only that key verifies.
        const TestKey successor = ephemeralKey();
        const TestKey secondSuccessor = ephemeralKey();
        const TestKey unpinned = ephemeralKey();
        const QList<const TestKey *> signers{&current, &successor, &secondSuccessor};
        const QList<QByteArray> rotation{publicKey, successor.publicKey, secondSuccessor.publicKey};
        const auto naming = [&](const QByteArray &key) {
            QJsonObject object = manifest;
            object.insert("key_id", QString::fromLatin1(CoreUpdate::keyId(key)));
            return object;
        };
        QString rotationError;
        const auto check = [&](const std::pair<QByteArray, QByteArray> &candidate, const QList<QByteArray> &keys) {
            CoreUpdateRelease selected;
            rotationError.clear();
            return CoreUpdate::verifyManifest(candidate.first, candidate.second, keys, "v1.2.0", "0.1.1", 1, 1,
                                              "windows", "amd64", false, &selected, &rotationError) &&
                   selected.assetName == "bimeron-v1.2.0-windows-amd64.zip";
        };
        const auto byCurrent = signWith(naming(publicKey), current.secret);
        const auto bySuccessor = signWith(naming(successor.publicKey), successor.secret);
        const auto bySecondSuccessor = signWith(naming(secondSuccessor.publicKey), secondSuccessor.secret);
        require(check(byCurrent, rotation), "current key rejected while successors are pinned");
        require(check(bySuccessor, rotation), "second pinned key rejected");
        require(check(bySecondSuccessor, rotation), "third pinned key rejected");
        const QList<QList<QByteArray>> orders{
            {rotation[1], rotation[2], rotation[0]}, {rotation[2], rotation[0], rotation[1]},
            {rotation[2], rotation[1], rotation[0]}, {rotation[1], rotation[0], rotation[2]}};
        for (const auto &order : orders)
            require(check(byCurrent, order) && check(bySuccessor, order) && check(bySecondSuccessor, order),
                    "pinned key order changed selection");
        require(!check(bySuccessor, {publicKey}) && rotationError == "Unknown update signing key",
                "successor accepted before it was pinned");
        require(!check(bySecondSuccessor, {publicKey, successor.publicKey}) && rotationError == "Unknown update signing key",
                "unpinned second successor accepted");
        require(check(bySuccessor, {successor.publicKey, secondSuccessor.publicKey}) &&
                !check(byCurrent, {successor.publicKey, secondSuccessor.publicKey}) &&
                rotationError == "Unknown update signing key", "retired current key still accepted");
        require(!check(signWith(naming(unpinned.publicKey), unpinned.secret), rotation) &&
                rotationError == "Unknown update signing key", "unknown key ID accepted");
        QJsonObject withoutKeyId = manifest;
        withoutKeyId.remove("key_id");
        require(!check(signWith(withoutKeyId, current.secret), rotation) && rotationError == "Unknown update signing key",
                "manifest without key_id accepted");
        // key_id names one pinned key, but the signature is valid only under another pinned key.
        const auto signedOnlyBy = [](const std::pair<QByteArray, QByteArray> &candidate, const QByteArray &key) {
            const QByteArray raw = QByteArray::fromBase64(candidate.second.chopped(1));
            return crypto_ed25519_check(reinterpret_cast<const uint8_t *>(raw.constData()),
                                        reinterpret_cast<const uint8_t *>(key.constData()),
                                        reinterpret_cast<const uint8_t *>(candidate.first.constData()),
                                        size_t(candidate.first.size())) == 0;
        };
        for (const TestKey *named : signers) {
            for (const TestKey *signer : signers) {
                if (named == signer) continue;
                const auto crossed = signWith(naming(named->publicKey), signer->secret);
                require(signedOnlyBy(crossed, signer->publicKey), "cross-signed fixture is invalid");
                require(!check(crossed, rotation) && rotationError == "Update signature verification failed",
                        "signature under another pinned key accepted");
            }
        }
        // An invalid trust list fails closed before any manifest field is read.
        require(!check(bySuccessor, {}) && rotationError == "Invalid update signature or signing key", "empty trust list accepted");
        require(!check(bySuccessor, {publicKey, successor.publicKey.left(31)}) &&
                rotationError == "Invalid update signature or signing key", "malformed pinned key accepted");
        require(!check(bySuccessor, {publicKey, successor.publicKey, successor.publicKey}) &&
                rotationError == "Ambiguous update signing key", "duplicate pinned key accepted");
        // Malformed signatures are still rejected before key selection.
        require(!check({bySuccessor.first, ' ' + bySuccessor.second}, rotation) &&
                rotationError == "Invalid update signature encoding", "non-canonical signature encoding accepted");
        require(!check({bySuccessor.first, QByteArray::fromBase64(bySuccessor.second).left(63).toBase64() + '\n'}, rotation) &&
                rotationError == "Invalid update signature encoding", "short signature accepted");
        require(!check({bySuccessor.first, bySuccessor.second.chopped(1)}, rotation) &&
                rotationError == "Invalid update signature or signing key", "signature without LF accepted");

        // Build-time list format: comma-separated canonical base64, no duplicates.
        const QByteArray a = publicKey.toBase64(), b = successor.publicKey.toBase64(), c = secondSuccessor.publicKey.toBase64();
        QList<QByteArray> parsed;
        require(CoreUpdate::parseTrustedKeys(a + ',' + b + ',' + c, &parsed) && parsed == rotation,
                "three-key list was not parsed");
        require(CoreUpdate::parseTrustedKeys(b, &parsed) && parsed == QList<QByteArray>{successor.publicKey},
                "single-key list was not parsed");
        require(CoreUpdate::parseTrustedKeys(QByteArray(), &parsed) && parsed.isEmpty(), "empty list must disable updates");
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        QByteArray nonCanonical = a;  // nonzero padding bits decode to the same bytes
        nonCanonical[42] = alphabet[(std::strchr(alphabet, a[42]) - alphabet) ^ 1];
        QByteArray urlSafe = a;
        urlSafe[0] = '-';
        const QList<QByteArray> malformedLists{
            QByteArray(a + ',' + a), QByteArray(a + ',' + b + ',' + a), QByteArray(a + ',' + b + ',' + b),
            QByteArray(a + ',' + b + ',' + c + ','), QByteArray(a + ',' + b + ",," + c),
            QByteArray(a + ','), QByteArray(',' + a), QByteArray(a + ",," + b),
            QByteArray(' ' + a), a.chopped(1), publicKey.left(31).toBase64(), QByteArray(publicKey + 'x').toBase64(),
            nonCanonical, urlSafe, QByteArray(a + ';' + b)};
        for (const QByteArray &list : malformedLists) {
            parsed = rotation;
            require(!CoreUpdate::parseTrustedKeys(list, &parsed) && parsed.isEmpty(), "malformed trusted key list accepted");
        }
    }
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
        require(configuredKeys.contains(pub), "manifest fixture key differs from pinned GUI key");
        const QByteArray manifest = manifestFile.readAll(), signature = signatureFile.readAll();
        require(CoreUpdate::verifyManifest(manifest, signature, configuredKeys, "v0.1.0-beta.3",
                                           "0.1.1", 1, 1, "windows", "amd64", true, &selected, &error),
                "core-generated manifest is incompatible with GUI verifier");
        require(selected.version == "0.1.0-beta.3" && selected.assetName == "bimeron-v0.1.0-beta.3-windows-amd64.zip",
                "reviewed manifest selected the wrong core archive");
        QByteArray tampered = manifest;
        tampered[tampered.indexOf("0.1.0-beta.3")] = '9';
        require(!CoreUpdate::verifyManifest(tampered, signature, configuredKeys, "v0.1.0-beta.3",
                                            "0.1.1", 1, 1, "windows", "amd64", true, &selected, &error) &&
                error == "Update signature verification failed", "tampered reviewed manifest was accepted");
    }
    if (argc == 4 && QString::fromLocal8Bit(argv[1]) == "--candidate-manifest") {
        QFile manifestFile(QString::fromLocal8Bit(argv[2]));
        QFile signatureFile(QString::fromLocal8Bit(argv[3]));
        require(manifestFile.open(QIODevice::ReadOnly) && signatureFile.open(QIODevice::ReadOnly), "cannot open candidate manifest");
        const QByteArray candidate = manifestFile.readAll(), candidateSignature = signatureFile.readAll();
        require(std::any_of(configuredKeys.cbegin(), configuredKeys.cend(),
                            [](const QByteArray &key) { return CoreUpdate::keyId(key) == "7370a2d8cf890e5e"; }),
                "candidate key ID does not match a configured key");
        CoreUpdateRelease selected;
        require(!CoreUpdate::verifyManifest(candidate, candidateSignature, configuredKeys, "v0.1.0-beta.3",
                                            "0.1.1", 1, 1, "windows", "amd64", true, &selected, &error) &&
                error == "Unreviewed or incomplete update compatibility", "candidate signature or review gate failed");
        QByteArray tampered = candidate;
        tampered[tampered.indexOf("0.1.0-beta.3")] = '9';
        require(!CoreUpdate::verifyManifest(tampered, candidateSignature, configuredKeys, "v0.1.0-beta.3",
                                            "0.1.1", 1, 1, "windows", "amd64", true, &selected, &error) &&
                error == "Update signature verification failed", "tampered candidate was not rejected by signature");
        require(!CoreUpdate::verifyManifest(candidate, candidateSignature, {QByteArray(32, '\1')}, "v0.1.0-beta.3",
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
        require(CoreUpdate::verifyManifest(manifestFile.readAll(), signatureFile.readAll(), configuredKeys,
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
