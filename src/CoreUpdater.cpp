#include "CoreUpdater.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSysInfo>
#include <algorithm>

CoreUpdater::CoreUpdater(QObject *parent) : QObject(parent) {
    // CMake validates the list at configure time; an invalid list still disables updates.
    CoreUpdate::parseTrustedKeys(QByteArray(BIMERON_CORE_UPDATE_PUBLIC_KEYS), &publicKeys_);
}

CoreUpdater::~CoreUpdater() {
    // An elevated process may outlive the GUI; keep its verified source until it exits.
    if (installing_ && staged_) staged_->directory->setAutoRemove(false);
}

bool CoreUpdater::available() const { return !publicKeys_.isEmpty(); }

QString CoreUpdater::platform() const {
#ifdef BIMERON_CORE_UPDATER_TESTING
    if (!testPlatform_.isEmpty()) return testPlatform_;
#endif
#ifdef Q_OS_WIN
    return QStringLiteral("windows");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return {};
#endif
}

QString CoreUpdater::architecture() const {
#ifdef BIMERON_CORE_UPDATER_TESTING
    if (!testArchitecture_.isEmpty()) return testArchitecture_;
#endif
#if defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("amd64");
#elif defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("arm64");
#else
    return {};
#endif
}

void CoreUpdater::fetch(const QUrl &url, qint64 limit, FetchCallback callback) {
#ifdef BIMERON_CORE_UPDATER_TESTING
    if (testFetcher_) { testFetcher_(url, limit, std::move(callback)); return; }
#endif
#ifdef BIMERON_LOCAL_UPDATE_TESTING
    const QFileInfo list(qEnvironmentVariable("BIMERON_TEST_RELEASES_JSON"));
    const QString root = list.dir().canonicalPath();
    const QString path = QFileInfo(url.toLocalFile()).canonicalFilePath();
    if (!url.isLocalFile() || root.isEmpty() || path.isEmpty() ||
        (path != list.canonicalFilePath() && !path.startsWith(root + QLatin1Char('/')))) {
        callback({}, tr("Untrusted local update file"));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > limit) {
        callback({}, tr("Local update file is unavailable or exceeds the size limit"));
        return;
    }
    const QByteArray bytes = file.read(limit + 1);
    if (bytes.size() > limit) callback({}, tr("Local update file exceeds the size limit"));
    else callback(bytes, {});
#else
    if (url.scheme() != "https" || !(url.host() == "api.github.com" || url.host() == "github.com" || url.host() == "objects.githubusercontent.com" || url.host().endsWith(".githubusercontent.com"))) {
        callback({}, tr("Untrusted update URL"));
        return;
    }
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Bimeron-Desktop-Core-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = network_.get(request);
    auto data = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this, [reply, data, limit] {
        data->append(reply->readAll());
        if (data->size() > limit) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [reply, data, limit, callback = std::move(callback)]() mutable {
        data->append(reply->readAll());
        QString error;
        if (data->size() > limit) error = QObject::tr("Update download exceeds the size limit");
        else if (reply->error() != QNetworkReply::NoError) error = reply->errorString();
        else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) error = QObject::tr("Update server returned an error");
        reply->deleteLater();
        callback(error.isEmpty() ? *data : QByteArray{}, error);
    });
#endif
}

QString CoreUpdater::stagedVersion() const { return staged_ ? staged_->release.version : QString{}; }

void CoreUpdater::discardStaged() { discard({}); }

void CoreUpdater::discard(const QString &message) {
    // The elevated helper may still read the source; the destructor keeps it.
    if (!staged_ || installing_) return;
    staged_.reset();
    emit stagedChanged();
    if (!message.isEmpty()) emit statusChanged(message);
}

bool CoreUpdater::stagedApplies(const QString &installedVersion, const QString &instance, bool betaAllowed) const {
    return staged_ && staged_->installedVersion == installedVersion && staged_->instance == instance &&
           (betaAllowed || !staged_->release.version.contains(QLatin1Char('-')));
}

bool CoreUpdater::discardStagedUnless(const QString &installedVersion, const QString &instance, bool betaAllowed) {
    if (!staged_ || stagedApplies(installedVersion, instance, betaAllowed)) return false;
    discard(tr("Downloaded Core %1 no longer applies and was discarded").arg(staged_->release.version));
    return !staged_;
}

void CoreUpdater::applyBetaSetting(bool betaAllowed) {
    if (!staged_ || betaAllowed || !staged_->release.version.contains(QLatin1Char('-'))) return;
    discard(tr("Downloaded Core %1 no longer applies and was discarded").arg(staged_->release.version));
}

bool CoreUpdater::verifyStaged(QString *path, QString *sha256) {
    if (!staged_) return false;
    QFile file(staged_->path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file) ||
        QString::fromLatin1(hash.result().toHex()) != staged_->sha256) {
        file.close();
        discard(tr("Downloaded Core %1 changed after verification and was discarded. Check for updates again.")
                    .arg(staged_->release.version));
        return false;
    }
    if (path) *path = staged_->path;
    if (sha256) *sha256 = staged_->sha256;
    return true;
}

void CoreUpdater::finish(const QString &message) {
    busy_ = false;
    emit statusChanged(staged_ ? message + QLatin1Char('\n') + tr("Core %1 is ready to install").arg(staged_->release.version)
                               : message);
}

void CoreUpdater::check(const QString &installedVersion, int ipcVersion, bool betaAllowed, const QString &instance) {
    if (busy_ || installing_) return;
    if (!available()) { emit statusChanged(tr("Signed updates are not available yet")); return; }
    // An unknown version or instance (IPC not synchronized) is no evidence of a change.
    if (!installedVersion.isEmpty() && !instance.isEmpty()) discardStagedUnless(installedVersion, instance, betaAllowed);
    if (platform().isEmpty() || architecture().isEmpty() || instance.isEmpty() || installedVersion.contains("-dev") ||
        !CoreUpdate::newerVersion("999999.0.0", installedVersion)) {
        finish(tr("Installed core version is unavailable; automatic updates are paused"));
        return;
    }
    busy_ = true;
    installedVersion_ = installedVersion;
    instance_ = instance;
    ipcVersion_ = ipcVersion;
    betaAllowed_ = betaAllowed;
    emit statusChanged(tr("Checking for signed core updates…"));
#ifdef BIMERON_LOCAL_UPDATE_TESTING
    const QString listPath = qEnvironmentVariable("BIMERON_TEST_RELEASES_JSON");
    if (!QFileInfo(listPath).isAbsolute()) {
        finish(tr("Local update release list is not configured"));
        return;
    }
    const QUrl releaseListUrl = QUrl::fromLocalFile(listPath);
#else
#ifdef BIMERON_CORE_UPDATER_TESTING
    const QUrl releaseListUrl = testReleaseList_;
#else
    const QUrl releaseListUrl(QStringLiteral("https://api.github.com/repos/bimeron/bimeron-core/releases?per_page=100"));
#endif
#endif
    fetch(releaseListUrl, 1024 * 1024,
          [this](QByteArray bytes, QString error) {
        if (!error.isEmpty()) { finish(tr("Core update check failed: %1").arg(error)); return; }
        const auto doc = QJsonDocument::fromJson(bytes);
        if (!doc.isArray()) { finish(tr("Invalid GitHub release list")); return; }
        releases_.clear();
        for (const auto &item : doc.array()) {
            const auto object = item.toObject();
            if (object.value("draft").toBool()) continue;
            const QString tag = object.value("tag_name").toString();
            if (!tag.startsWith('v') || !CoreUpdate::newerVersion(tag.mid(1), installedVersion_) ||
                (!betaAllowed_ && object.value("prerelease").toBool())) continue;
            Release release;
            release.tag = tag;
            for (const auto &asset : object.value("assets").toArray()) {
                const auto entry = asset.toObject();
                const QString name = entry.value("name").toString();
                const QUrl url(entry.value("browser_download_url").toString());
                if (!release.assets.contains(name)) release.assets.insert(name, url);
                else release.assets.insert(name, QUrl());
            }
            releases_.append(release);
        }
        std::sort(releases_.begin(), releases_.end(), [](const Release &a, const Release &b) {
            return CoreUpdate::newerVersion(a.tag.mid(1), b.tag.mid(1));
        });
        // A release that is no longer published (or no longer offered) cannot back the staged file.
        if (staged_ && std::none_of(releases_.cbegin(), releases_.cend(),
                                    [this](const Release &release) { return release.tag == staged_->release.tag; }))
            discard({});
        inspectNext();
    });
}

void CoreUpdater::inspectNext() {
    if (releases_.isEmpty()) {
        finish(tr("No compatible signed core update is available"));
        return;
    }
    const Release release = releases_.takeFirst();
    // Releases are sorted newest first, so the staged release is reached only when
    // no newer release verified; every release before it is newer than it.
    const bool stagedRelease = staged_ && release.tag == staged_->release.tag;
    const QUrl manifestUrl = release.assets.value("update-manifest.json");
    const QUrl signatureUrl = release.assets.value("update-manifest.json.sig");
    if (!manifestUrl.isValid() || !signatureUrl.isValid()) {
        if (stagedRelease) discard({});
        inspectNext();
        return;
    }
    fetch(manifestUrl, 64 * 1024, [this, release, signatureUrl, stagedRelease](QByteArray manifest, QString error) {
        // A transient failure does not invalidate an already verified staged file.
        if (!error.isEmpty()) {
            if (stagedRelease && staged_) finish(tr("Core update check failed: %1").arg(error));
            else inspectNext();
            return;
        }
        fetch(signatureUrl, 1024, [this, release, manifest, stagedRelease](QByteArray signature, QString error) {
            if (!error.isEmpty()) {
                if (stagedRelease && staged_) finish(tr("Core update check failed: %1").arg(error));
                else inspectNext();
                return;
            }
            CoreUpdateRelease selected;
            QString reason;
            if (!CoreUpdate::verifyManifest(manifest, signature, publicKeys_, release.tag,
                                            QCoreApplication::applicationVersion(), ipcVersion_, 1,
                                            platform(), architecture(), betaAllowed_, &selected, &reason)) {
                if (stagedRelease) discard({});
                inspectNext();
                return;
            }
            if (stagedRelease && staged_ && selected.assetName == staged_->release.assetName &&
                selected.assetSize == staged_->release.assetSize && selected.assetSha256 == staged_->release.assetSha256) {
                // The postponed update is still the newest release: keep it, download nothing.
                busy_ = false;
                emit statusChanged(tr("Core %1 is ready to install").arg(staged_->release.version));
                return;
            }
            const QUrl archiveUrl = release.assets.value(selected.assetName);
            if (!archiveUrl.isValid()) {
                if (stagedRelease) discard({});
                inspectNext();
                return;
            }
            // A newer verified release, or changed signed metadata, replaces the staged file.
            discard({});
            downloadArchive(selected, archiveUrl);
        });
    });
}

void CoreUpdater::downloadArchive(const CoreUpdateRelease &release, const QUrl &url) {
    emit statusChanged(tr("Downloading verified Core %1…").arg(release.version));
    fetch(url, 100 * 1024 * 1024, [this, release](QByteArray zip, QString error) {
        if (!error.isEmpty()) { finish(tr("Core update download failed: %1").arg(error)); return; }
        QByteArray executable;
        QString reason;
        if (!CoreUpdate::extractExecutable(zip, release, platform(), architecture(), &executable, &reason)) {
            finish(tr("Core update rejected: %1").arg(reason));
            return;
        }
        auto staged = std::make_unique<Staged>();
        staged->directory = std::make_unique<QTemporaryDir>();
        if (!staged->directory->isValid()) { finish(tr("Cannot create a private update directory")); return; }
        const QString path = staged->directory->filePath(platform() == "windows" ? "bimeron.exe" : "bimeron");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(executable) != executable.size() || !file.flush()) {
            finish(tr("Cannot stage the verified core executable"));
            return;
        }
        file.close();
        if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
            finish(tr("Cannot set core executable permissions"));
            return;
        }
        staged->release = release;
        staged->path = path;
        staged->sha256 = QString::fromLatin1(QCryptographicHash::hash(executable, QCryptographicHash::Sha256).toHex());
        staged->installedVersion = installedVersion_;
        staged->instance = instance_;
        staged_ = std::move(staged);
        busy_ = false;
        emit stagedChanged();
        emit statusChanged(tr("Core %1 is ready to install").arg(release.version));
        emit ready(release.version);
    });
}

