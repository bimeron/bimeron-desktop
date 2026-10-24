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
    const QByteArray encoded(BIMERON_CORE_UPDATE_PUBLIC_KEY);
    publicKey_ = QByteArray::fromBase64(encoded);
    if (publicKey_.size() != 32 || publicKey_.toBase64() != encoded)
        publicKey_.clear();
}

CoreUpdater::~CoreUpdater() {
    // An elevated process may outlive the GUI; keep its verified source until it exits.
    if (installing_ && staged_) staged_->setAutoRemove(false);
}

bool CoreUpdater::available() const { return publicKey_.size() == 32; }

QString CoreUpdater::platform() const {
#ifdef Q_OS_WIN
    return QStringLiteral("windows");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return {};
#endif
}

QString CoreUpdater::architecture() const {
#if defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("amd64");
#elif defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("arm64");
#else
    return {};
#endif
}

void CoreUpdater::fetch(const QUrl &url, qint64 limit, FetchCallback callback) {
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

void CoreUpdater::check(const QString &installedVersion, int ipcVersion, bool betaAllowed) {
    if (busy_ || installing_ || ready_) return;
    if (!available()) { emit statusChanged(tr("Signed updates are not available yet")); return; }
    if (platform().isEmpty() || architecture().isEmpty() || installedVersion.contains("-dev") ||
        !CoreUpdate::newerVersion("999999.0.0", installedVersion)) {
        emit statusChanged(tr("Installed core version is unavailable; automatic updates are paused"));
        return;
    }
    busy_ = true;
    staged_.reset();
    installedVersion_ = installedVersion;
    ipcVersion_ = ipcVersion;
    betaAllowed_ = betaAllowed;
    emit statusChanged(tr("Checking for signed core updates…"));
#ifdef BIMERON_LOCAL_UPDATE_TESTING
    const QString listPath = qEnvironmentVariable("BIMERON_TEST_RELEASES_JSON");
    if (!QFileInfo(listPath).isAbsolute()) {
        busy_ = false;
        emit statusChanged(tr("Local update release list is not configured"));
        return;
    }
    const QUrl releaseListUrl = QUrl::fromLocalFile(listPath);
#else
    const QUrl releaseListUrl(QStringLiteral("https://api.github.com/repos/bimeron/bimeron-core/releases?per_page=100"));
#endif
    fetch(releaseListUrl, 1024 * 1024,
          [this](QByteArray bytes, QString error) {
        if (!error.isEmpty()) { busy_ = false; emit statusChanged(tr("Core update check failed: %1").arg(error)); return; }
        const auto doc = QJsonDocument::fromJson(bytes);
        if (!doc.isArray()) { busy_ = false; emit statusChanged(tr("Invalid GitHub release list")); return; }
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
        inspectNext();
    });
}

void CoreUpdater::inspectNext() {
    if (releases_.isEmpty()) {
        busy_ = false;
        emit statusChanged(tr("No compatible signed core update is available"));
        return;
    }
    const Release release = releases_.takeFirst();
    const QUrl manifestUrl = release.assets.value("update-manifest.json");
    const QUrl signatureUrl = release.assets.value("update-manifest.json.sig");
    if (!manifestUrl.isValid() || !signatureUrl.isValid()) { inspectNext(); return; }
    fetch(manifestUrl, 64 * 1024, [this, release, signatureUrl](QByteArray manifest, QString error) {
        if (!error.isEmpty()) { inspectNext(); return; }
        fetch(signatureUrl, 1024, [this, release, manifest](QByteArray signature, QString error) {
            if (!error.isEmpty()) { inspectNext(); return; }
            CoreUpdateRelease selected;
            QString reason;
            if (!CoreUpdate::verifyManifest(manifest, signature, publicKey_, release.tag,
                                            QCoreApplication::applicationVersion(), ipcVersion_, 1,
                                            platform(), architecture(), betaAllowed_, &selected, &reason)) {
                inspectNext();
                return;
            }
            const QUrl archiveUrl = release.assets.value(selected.assetName);
            if (!archiveUrl.isValid()) { inspectNext(); return; }
            downloadArchive(selected, archiveUrl);
        });
    });
}

void CoreUpdater::downloadArchive(const CoreUpdateRelease &release, const QUrl &url) {
    emit statusChanged(tr("Downloading verified Core %1…").arg(release.version));
    fetch(url, 100 * 1024 * 1024, [this, release](QByteArray zip, QString error) {
        if (!error.isEmpty()) { busy_ = false; emit statusChanged(tr("Core update download failed: %1").arg(error)); return; }
        QByteArray executable;
        QString reason;
        if (!CoreUpdate::extractExecutable(zip, release, platform(), architecture(), &executable, &reason)) {
            busy_ = false;
            emit statusChanged(tr("Core update rejected: %1").arg(reason));
            return;
        }
        staged_ = std::make_unique<QTemporaryDir>();
        if (!staged_->isValid()) { busy_ = false; emit statusChanged(tr("Cannot create a private update directory")); return; }
        const QString path = staged_->filePath(platform() == "windows" ? "bimeron.exe" : "bimeron");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(executable) != executable.size() || !file.flush()) {
            staged_.reset(); busy_ = false; emit statusChanged(tr("Cannot stage the verified core executable")); return;
        }
        file.close();
        if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
            staged_.reset(); busy_ = false; emit statusChanged(tr("Cannot set core executable permissions")); return;
        }
        const QString digest = QString::fromLatin1(QCryptographicHash::hash(executable, QCryptographicHash::Sha256).toHex());
        busy_ = false;
        ready_ = true;
        emit statusChanged(tr("Core %1 is ready to install").arg(release.version));
        emit ready(release.version, path, digest);
    });
}
