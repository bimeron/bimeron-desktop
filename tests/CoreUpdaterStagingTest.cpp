// A downloaded and verified Core update stays staged after the install prompt is
// declined, installs later without another download, and is discarded only when
// it no longer applies or fails its re-check (bimeron-desktop #68).
#include "CoreUpdater.h"
#include "CoreUpdateStaging.h"
#include "monocypher-ed25519.h"
#include "miniz.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSet>

#include <cstdio>
#include <utility>

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

QByteArray zipWith(const QByteArray &payload) {
    mz_zip_archive writer{};
    mz_zip_writer_init_heap(&writer, 0, 0);
    mz_zip_writer_add_mem(&writer, "windows-amd64/bimeron.exe", payload.constData(), size_t(payload.size()), MZ_BEST_COMPRESSION);
    void *data = nullptr;
    size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&writer, &data, &size);
    const QByteArray zip(static_cast<char *>(data), qsizetype(size));
    mz_free(data);
    mz_zip_writer_end(&writer);
    return zip;
}

// An in-memory GitHub-shaped release feed signed with an ephemeral key.
class Feed {
public:
    Feed() {
        quint32 seed[8];
        QRandomGenerator::system()->fillRange(seed);
        uint8_t publicBytes[32]{};
        crypto_ed25519_key_pair(secret_, publicBytes, reinterpret_cast<uint8_t *>(seed));
        publicKey = QByteArray(reinterpret_cast<char *>(publicBytes), 32);
    }

    QByteArray publicKey;
    QHash<QString, int> fetches;
    QSet<QString> failing;

    static QString url(const QString &name) { return QStringLiteral("https://github.com/fixture/%1").arg(name); }

    // Publishes (or re-publishes) a release whose Windows amd64 executable is `payload`.
    void publish(const QString &version, const QByteArray &payload) {
        const QString tag = QLatin1Char('v') + version;
        const QByteArray zip = zipWith(payload);
        const QString archive = QStringLiteral("bimeron-%1-windows-amd64.zip").arg(tag);
        QJsonArray archives;
        for (const QString &os : {QStringLiteral("windows"), QStringLiteral("linux")})
            for (const QString &arch : {QStringLiteral("amd64"), QStringLiteral("arm64")}) {
                const bool selected = os == "windows" && arch == "amd64";
                archives.append(QJsonObject{{"os", os}, {"arch", arch},
                    {"name", QStringLiteral("bimeron-%1-%2-%3.zip").arg(tag, os, arch)},
                    {"size", selected ? zip.size() : 100},
                    {"sha256", selected ? QString::fromLatin1(QCryptographicHash::hash(zip, QCryptographicHash::Sha256).toHex())
                                        : QString(64, 'a')}});
            }
        const QJsonObject manifest{{"schema", 1}, {"tag", tag}, {"core_version", version},
            {"channel", version.contains('-') ? "beta" : "stable"},
            {"key_id", QString::fromLatin1(CoreUpdate::keyId(publicKey))},
            {"compatibility", QJsonObject{{"schema", 1}, {"ipc_min", 1}, {"ipc_max", 1}, {"gui_min", "0.3.2"},
                                          {"gui_max_exclusive", "0.4.0"}, {"gui_reviewed", true}, {"profile_format", 1}}},
            {"archives", archives}};
        const QByteArray bytes = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
        uint8_t signature[64];
        crypto_ed25519_sign(signature, secret_, reinterpret_cast<const uint8_t *>(bytes.constData()), size_t(bytes.size()));
        files_.insert(url(tag + "/update-manifest.json"), bytes);
        files_.insert(url(tag + "/update-manifest.json.sig"),
                      QByteArray(reinterpret_cast<char *>(signature), 64).toBase64() + '\n');
        files_.insert(url(tag + '/' + archive), zip);
        QJsonArray assets;
        for (const QString &name : {QStringLiteral("update-manifest.json"), QStringLiteral("update-manifest.json.sig"), archive})
            assets.append(QJsonObject{{"name", name}, {"browser_download_url", url(tag + '/' + name)}});
        releases_.insert(tag, QJsonObject{{"tag_name", tag}, {"draft", false}, {"prerelease", version.contains('-')},
                                          {"assets", assets}});
    }

    void withdraw(const QString &version) { releases_.remove(QLatin1Char('v') + version); }

    int archiveFetches(const QString &version) const {
        const QString tag = QLatin1Char('v') + version;
        return fetches.value(url(QStringLiteral("%1/bimeron-%1-windows-amd64.zip").arg(tag)));
    }

    void attach(CoreUpdater &updater) {
        updater.setTestHooks({publicKey}, [this](const QUrl &requested, qint64 limit, CoreUpdater::FetchCallback callback) {
            const QString key = requested.toString();
            ++fetches[key];
            if (failing.contains(key)) { callback({}, QStringLiteral("fixture network failure")); return; }
            if (key == url("releases")) {
                QJsonArray list;
                for (const auto &release : std::as_const(releases_)) list.append(release);
                callback(QJsonDocument(list).toJson(), {});
                return;
            }
            const QByteArray bytes = files_.value(key);
            if (bytes.isEmpty() || bytes.size() > limit) callback({}, QStringLiteral("fixture not found"));
            else callback(bytes, {});
        }, QStringLiteral("windows"), QStringLiteral("amd64"), QUrl(url("releases")));
    }

private:
    uint8_t secret_[64]{};
    QHash<QString, QByteArray> files_;
    QHash<QString, QJsonObject> releases_;
};

struct Observer {
    QStringList ready;
    QString status;
    int stagedChanges = 0;

    explicit Observer(CoreUpdater &updater) {
        QObject::connect(&updater, &CoreUpdater::ready, &updater, [this](const QString &version) { ready.append(version); });
        QObject::connect(&updater, &CoreUpdater::statusChanged, &updater, [this](const QString &message) { status = message; });
        QObject::connect(&updater, &CoreUpdater::stagedChanged, &updater, [this] { ++stagedChanges; });
    }
};

const QString installed = QStringLiteral("0.4.0-beta.2");
const QString instance = QStringLiteral("instance-a");
const QString beta3 = QStringLiteral("0.4.0-beta.3");
const QByteArray payload3("fixture core 0.4.0-beta.3");

// Downloads beta.3 for the default context; the user then declines the prompt.
void stageBeta3(Feed &feed, CoreUpdater &updater, Observer &observer) {
    feed.publish(beta3, payload3);
    feed.attach(updater);
    updater.check(installed, 1, true, instance);
    expect(observer.ready == QStringList{beta3}, "first check stages the verified release");
    expect(updater.hasStaged() && updater.stagedVersion() == beta3, "verified release is staged");
    expect(feed.archiveFetches(beta3) == 1, "first check downloads the archive once");
}

void postponeAndInstall() {
    Feed feed;
    CoreUpdater updater;
    Observer observer(updater);
    stageBeta3(feed, updater, observer);
    // Declining the prompt is no updater call: nothing discards the staged file.
    expect(updater.hasStaged(), "declined update stays staged");

    // Periodic or manual checks keep the postponed file without another download or prompt.
    updater.check(installed, 1, true, instance);
    expect(updater.hasStaged() && updater.stagedVersion() == beta3, "same release keeps the staged file");
    expect(feed.archiveFetches(beta3) == 1, "same release is not downloaded again");
    expect(observer.ready.size() == 1, "kept postponed update does not prompt again");
    expect(observer.status == "Core 0.4.0-beta.3 is ready to install", "status reports the staged update");
    expect(!updater.busy(), "check finished");

    // A check while IPC is not synchronized (unknown version or instance) keeps the file.
    updater.check({}, 1, true, {});
    expect(updater.hasStaged(), "unknown installed version keeps the staged file");
    updater.check(installed, 1, true, {});
    expect(updater.hasStaged(), "unknown service instance keeps the staged file");
    expect(observer.status == "Installed core version is unavailable; automatic updates are paused\nCore 0.4.0-beta.3 is ready to install",
           "unsynchronized check keeps the staged state visible");
    expect(feed.archiveFetches(beta3) == 1 && feed.fetches.value(Feed::url("releases")) == 2,
           "unsynchronized checks fetch nothing");

    // A transient failure of the check keeps the verified file and says so.
    feed.failing.insert(Feed::url("v0.4.0-beta.3/update-manifest.json"));
    updater.check(installed, 1, true, instance);
    expect(updater.hasStaged(), "transient manifest failure keeps the staged file");
    expect(observer.status.startsWith("Core update check failed: fixture network failure") &&
           observer.status.endsWith("Core 0.4.0-beta.3 is ready to install"), "failure status keeps the staged state visible");
    feed.failing.clear();
    feed.failing.insert(Feed::url("releases"));
    updater.check(installed, 1, true, instance);
    expect(updater.hasStaged(), "release list failure keeps the staged file");
    feed.failing.clear();

    // Install after postponing: the re-check passes and no download happens.
    QString path, digest;
    expect(updater.verifyStaged(&path, &digest), "unchanged staged file passes the re-check");
    QFile staged(path);
    expect(staged.open(QIODevice::ReadOnly) && staged.readAll() == payload3, "staged file holds the verified executable");
    staged.close();
    expect(digest == QString::fromLatin1(QCryptographicHash::hash(payload3, QCryptographicHash::Sha256).toHex()),
           "install digest is the verified executable hash");
    expect(feed.archiveFetches(beta3) == 1, "install after postponing does not download again");

    // While the elevated helper may read the source, nothing removes it.
    updater.setInstalling(true);
    updater.discardStaged();
    expect(!updater.discardStagedUnless("0.4.0-beta.3", "instance-b", false), "invalidation waits for the helper");
    expect(updater.hasStaged(), "staged source survives during installation");
    updater.check(installed, 1, true, instance);
    expect(!updater.busy() && feed.fetches.value(Feed::url("releases")) == 4, "no check runs during installation");
    updater.setInstalling(false);
    updater.discardStaged();
    expect(!updater.hasStaged() && !QFile::exists(path), "finished installation removes the staged source");
}

void hashRecheckFailure() {
    Feed feed;
    CoreUpdater updater;
    Observer observer(updater);
    stageBeta3(feed, updater, observer);
    QString path, digest;
    expect(updater.verifyStaged(&path, &digest), "baseline re-check");
    const int changes = observer.stagedChanges;
    QFile file(path);
    expect(file.open(QIODevice::WriteOnly | QIODevice::Append) && file.write("x") == 1, "tamper with the staged file");
    file.close();
    QString rejectedPath = QStringLiteral("unchanged"), rejectedDigest = QStringLiteral("unchanged");
    expect(!updater.verifyStaged(&rejectedPath, &rejectedDigest), "changed staged file fails the re-check");
    expect(rejectedPath == "unchanged" && rejectedDigest == "unchanged", "failed re-check returns no install arguments");
    expect(!updater.hasStaged() && !QFile::exists(path), "changed staged file is discarded");
    expect(observer.stagedChanges == changes + 1, "discard is announced");
    expect(observer.status == "Downloaded Core 0.4.0-beta.3 changed after verification and was discarded. Check for updates again.",
           "re-check failure status");

    Feed missingFeed;
    CoreUpdater missingUpdater;
    Observer missingObserver(missingUpdater);
    stageBeta3(missingFeed, missingUpdater, missingObserver);
    expect(missingUpdater.verifyStaged(&path, nullptr) && QFile::remove(path), "remove the staged file");
    expect(!missingUpdater.verifyStaged(nullptr, nullptr) && !missingUpdater.hasStaged(), "missing staged file is discarded");
}

void invalidation() {
    const QString discarded = QStringLiteral("Downloaded Core 0.4.0-beta.3 no longer applies and was discarded");
    {
        Feed feed;
        CoreUpdater updater;
        Observer observer(updater);
        stageBeta3(feed, updater, observer);
        expect(!updater.discardStagedUnless(installed, instance, true), "same installed version and instance keep the file");
        expect(updater.discardStagedUnless("0.4.0-beta.3", instance, true), "changed installed Core version discards");
        expect(!updater.hasStaged() && observer.status == discarded, "installed version change status");
    }
    {
        Feed feed;
        CoreUpdater updater;
        Observer observer(updater);
        stageBeta3(feed, updater, observer);
        expect(updater.discardStagedUnless(installed, "instance-b", true), "changed service instance discards");
        expect(!updater.hasStaged(), "instance change removes the file");
    }
    {
        Feed feed;
        CoreUpdater updater;
        Observer observer(updater);
        stageBeta3(feed, updater, observer);
        updater.applyBetaSetting(true);
        expect(updater.hasStaged(), "enabling betas keeps the file");
        updater.applyBetaSetting(false);
        expect(!updater.hasStaged() && observer.status == discarded, "turning beta updates off discards a staged beta");
    }
    {
        Feed feed;
        CoreUpdater updater;
        Observer observer(updater);
        stageBeta3(feed, updater, observer);
        updater.check(installed, 1, false, instance);
        expect(!updater.hasStaged(), "a stable-only check discards a staged beta");
        expect(feed.archiveFetches(beta3) == 1, "a stable-only check downloads nothing");
    }
    {
        Feed feed;
        CoreUpdater updater;
        Observer observer(updater);
        feed.publish("0.4.0", "fixture core 0.4.0");
        feed.attach(updater);
        updater.check("0.3.9", 1, false, instance);
        expect(updater.stagedVersion() == "0.4.0", "stable release staged");
        updater.applyBetaSetting(false);
        expect(updater.hasStaged(), "beta setting keeps a staged stable release");
    }
    {
        Feed feed;
        CoreUpdater updater;
        Observer observer(updater);
        stageBeta3(feed, updater, observer);
        feed.publish("0.4.0-beta.4", "fixture core 0.4.0-beta.4");
        updater.check(installed, 1, true, instance);
        expect(updater.stagedVersion() == "0.4.0-beta.4", "a newer release replaces the staged file");
        expect(observer.ready == (QStringList{beta3, "0.4.0-beta.4"}), "the newer release prompts");
        expect(feed.archiveFetches("0.4.0-beta.4") == 1 && feed.archiveFetches(beta3) == 1, "only the newer archive is downloaded");
    }
    {
        Feed feed;
        CoreUpdater updater;
        Observer observer(updater);
        stageBeta3(feed, updater, observer);
        feed.withdraw(beta3);
        updater.check(installed, 1, true, instance);
        expect(!updater.hasStaged(), "a withdrawn release discards the staged file");
        expect(observer.status == "No compatible signed core update is available", "withdrawn release status");
    }
    {
        Feed feed;
        CoreUpdater updater;
        Observer observer(updater);
        stageBeta3(feed, updater, observer);
        const QByteArray republished("fixture core 0.4.0-beta.3 rebuilt");
        feed.publish(beta3, republished);
        updater.check(installed, 1, true, instance);
        QString path, digest;
        expect(updater.verifyStaged(&path, &digest) &&
               digest == QString::fromLatin1(QCryptographicHash::hash(republished, QCryptographicHash::Sha256).toHex()),
               "changed signed metadata replaces the staged file");
        expect(feed.archiveFetches(beta3) == 2, "changed signed metadata downloads again");
    }
}
}

// The install flow's postpone/discard decisions (CoreUpdateStaging, used by MainWindow).
void decisions() {
    using CoreUpdateStaging::Action;
    using namespace CoreUpdateStaging;
    expect(beforePrompt(true, false, true) == Action::Proceed, "ready update is offered");
    expect(beforePrompt(false, false, true) == Action::Postpone, "prerequisite failure postpones");
    expect(beforePrompt(true, true, true) == Action::Postpone, "busy connection postpones before the prompt");
    expect(beforePrompt(true, true, false) == Action::Postpone, "unsynchronized connection is no evidence of a change");
    expect(beforePrompt(true, false, false) == Action::Discard, "changed version, instance or channel discards before the prompt");
    expect(afterPrompt(false, false, true, true) == Action::Postpone, "answering No postpones");
    expect(afterPrompt(false, false, false, false) == Action::Postpone, "answering No never discards by itself");
    expect(afterPrompt(true, false, true, true) == Action::Proceed, "confirmed unchanged update installs");
    expect(afterPrompt(true, true, true, true) == Action::Postpone, "busy after Yes postpones");
    expect(afterPrompt(true, false, true, false) == Action::Postpone, "changed session after Yes postpones");
    expect(afterPrompt(true, false, false, false) == Action::Discard, "changed instance or version after Yes discards");
    expect(afterStartFailure(true) == Action::Postpone, "canceled elevation postpones");
    expect(afterStartFailure(false) == Action::Discard, "unbuildable arguments discard");
    expect(afterHelper(126, true) == Action::Postpone, "canceled pkexec postpones");
    expect(afterHelper(0, true) == Action::Discard && afterHelper(0, false) == Action::Discard, "installed source is discarded");
    expect(afterHelper(1, true) == Action::Discard && afterHelper(126, false) == Action::Discard, "helper failure discards");
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationVersion("0.3.2");
    postponeAndInstall();
    hashRecheckFailure();
    invalidation();
    decisions();
    if (failures) std::fprintf(stderr, "%d core updater staging check(s) failed\n", failures);
    return failures ? 1 : 0;
}
