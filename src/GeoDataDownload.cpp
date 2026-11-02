#include "GeoDataDownload.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSslError>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QThread>

namespace {
constexpr qint64 DataLimit = 64 * 1024 * 1024;
const QString Repository = QStringLiteral("https://github.com/Loyalsoldier/v2ray-rules-dat/releases/download/");
bool allowed(const QUrl &url) {
    return url.isValid() && url.scheme() == "https" && url.userInfo().isEmpty()
        && url.fragment().isEmpty() && (url.port(-1) == -1 || url.port() == 443)
        && (url.host() == "api.github.com" || url.host() == "github.com"
            || url.host() == "release-assets.githubusercontent.com");
}
}
struct GeoDataDownload::Job {
    QString kind, hash, tag;
    QUrl dataUrl;
    int phase = 0; // metadata, checksum, data, awaiting caller validation
    qint64 received = 0;
    QByteArray body;
    QCryptographicHash digest{QCryptographicHash::Sha256};
    QTemporaryFile file;
    QPointer<QNetworkReply> reply;
    QTimer deadline, inactivity;
    std::unique_ptr<QThread> publisher;
    qint64 limit() const { return phase == 0 ? 1024 * 1024 : phase == 1 ? 4096 : DataLimit; }
    ~Job() {
        if (publisher) { publisher->requestInterruption(); publisher->wait(); }
        if (reply) { reply->disconnect(); reply->abort(); reply->deleteLater(); }
    }
};
GeoDataDownload::GeoDataDownload(QObject *parent)
    : GeoDataDownload(new QNetworkAccessManager, parent) { network_->setParent(this); }
GeoDataDownload::GeoDataDownload(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent), network_(network) { Q_ASSERT(network_); }
GeoDataDownload::~GeoDataDownload() { jobs_.clear(); }
bool GeoDataDownload::busy(const QString &kind) const { return jobs_.contains(kind); }
QString GeoDataDownload::releaseTag(const QString &kind) const { const auto job = jobs_.value(kind); return job ? job->tag : QString(); }
bool GeoDataDownload::current(const std::shared_ptr<Job> &job) const { return jobs_.value(job->kind) == job; }
bool GeoDataDownload::start(const QString &kind) {
    if ((kind != "geoip" && kind != "geosite") || busy(kind)) return false;
    auto job = std::make_shared<Job>();
    job->kind = kind;
    jobs_.insert(kind, job);
    // QObject visibility permits deterministic timeout tests without changing deadlines.
    job->deadline.setParent(this);
    job->deadline.setObjectName(kind + "/deadline");
    job->inactivity.setParent(this);
    job->inactivity.setObjectName(kind + "/inactivity");
    job->deadline.setSingleShot(true);
    job->inactivity.setSingleShot(true);
    // Weak captures avoid a job/timer ownership cycle.
    const std::weak_ptr<Job> weak = job;
    connect(&job->deadline, &QTimer::timeout, this, [this, weak] {
        if (auto j = weak.lock()) fail(j, QStringLiteral("Download exceeded the 120 second deadline"));
    });
    connect(&job->inactivity, &QTimer::timeout, this, [this, weak] {
        if (auto j = weak.lock()) fail(j, QStringLiteral("Download inactive for 30 seconds"));
    });
    job->deadline.start(120000);
    request(job, QUrl(QStringLiteral("https://api.github.com/repos/Loyalsoldier/v2ray-rules-dat/releases/latest")));
    return true;
}
void GeoDataDownload::discard(const QString &kind) {
    auto job = jobs_.take(kind);
    if (!job) return;
    job->deadline.stop(); job->inactivity.stop();
    if (job->reply) {
        job->reply->disconnect(this); job->reply->abort(); job->reply->deleteLater(); job->reply = nullptr;
    }
    emit finished(kind);
}
void GeoDataDownload::cancel(const QString &kind) { discard(kind); }
void GeoDataDownload::fail(const std::shared_ptr<Job> &job, const QString &error) {
    if (!current(job)) return;
    const QString kind = job->kind;
    jobs_.remove(kind);
    job->deadline.stop(); job->inactivity.stop();
    if (job->reply) {
        job->reply->disconnect(this); job->reply->abort(); job->reply->deleteLater(); job->reply = nullptr;
    }
    const QString phase = job->phase == 0 ? "metadata" : job->phase == 1 ? "checksum"
        : job->phase == 2 ? "data" : job->phase == 3 ? "Core validation" : "publication";
    emit failed(kind, QStringLiteral("Source: Loyalsoldier/v2ray-rules-dat\nPhase: %1\n%2").arg(phase, error));
    emit finished(kind);
}
void GeoDataDownload::request(const std::shared_ptr<Job> &job, const QUrl &url, int redirects) {
    if (!current(job)) return;
    if (!allowed(url)) { fail(job, QStringLiteral("Refused an untrusted download URL")); return; }
    job->body.clear(); job->received = 0;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setRawHeader("User-Agent", "Bimeron-Desktop-GeoData");
    request.setRawHeader("Accept", job->phase == 0 ? "application/vnd.github+json" : "application/octet-stream");
    auto *reply = network_->get(request);
    job->reply = reply;
    reply->setReadBufferSize(64 * 1024);
    job->inactivity.start(30000);
    const std::weak_ptr<Job> weak = job;
    connect(reply, &QNetworkReply::readyRead, this, [this, weak] { if (auto j = weak.lock()) consume(j); });
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, weak] {
        if (auto j = weak.lock(); j && current(j)) {
            j->inactivity.start(30000);
            if (j->reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > j->limit())
                fail(j, QStringLiteral("Download exceeds the size limit"));
        }
    });
    connect(reply, &QNetworkReply::sslErrors, this, [this, weak](const QList<QSslError> &errors) {
        QStringList codes;
        for (const auto &error : errors) codes.append(QString::number(int(error.error())));
        if (auto j = weak.lock()) fail(j, QStringLiteral("TLS certificate verification failed; Qt TLS codes: %1").arg(codes.join(", ")));
    });
    connect(reply, &QNetworkReply::finished, this, [this, weak, reply, redirects] {
        auto j = weak.lock();
        if (!j || !current(j) || j->reply != reply) return;
        consume(j);
        if (!current(j)) return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QUrl target = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const auto error = reply->error();
        j->reply = nullptr;
        reply->disconnect(this);
        reply->deleteLater();
        if (status >= 400) { fail(j, QStringLiteral("Download failed (HTTP %1); Qt network code %2").arg(status).arg(int(error))); return; }
        if (error != QNetworkReply::NoError) { fail(j, QStringLiteral("Network download failed; Qt network code %1").arg(int(error))); return; }
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            if (target.isEmpty() || redirects >= 5) { fail(j, QStringLiteral("Download redirect limit exceeded or target missing")); return; }
            this->request(j, reply->url().resolved(target), redirects + 1);
            return;
        }
        if (status != 200) { fail(j, QStringLiteral("Download failed (HTTP %1)").arg(status)); return; }
        complete(j);
    });
}
void GeoDataDownload::consume(const std::shared_ptr<Job> &job) {
    if (!current(job) || !job->reply) return;
    auto *reply = job->reply.data();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    while (reply->bytesAvailable()) {
        const QByteArray chunk = reply->read(64 * 1024);
        if (chunk.isEmpty()) break;
        job->inactivity.start(30000);
        job->received += chunk.size();
        if (job->received > job->limit()) { fail(job, QStringLiteral("Download exceeds the size limit")); return; }
        // Redirect/error bodies are bounded but never staged as database bytes.
        if (status != 200) continue;
        if (job->phase == 2) {
            if (job->file.write(chunk) != chunk.size()) { fail(job, QStringLiteral("Cannot write temporary database")); return; }
            job->digest.addData(chunk);
        } else job->body += chunk;
    }
    if (job->phase == 2 && status == 200)
        emit progress(job->kind, job->received, reply->header(QNetworkRequest::ContentLengthHeader).isValid()
                      ? reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() : -1);
}
void GeoDataDownload::complete(const std::shared_ptr<Job> &job) {
    if (job->phase == 0) {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(job->body, &error);
        const auto object = document.object();
        const QString tag = object.value("tag_name").toString();
        if (error.error != QJsonParseError::NoError || !document.isObject() || tag.isEmpty()
            || tag == "latest" || tag.contains('/') || tag.contains('\\') || tag == "." || tag == "..") {
            fail(job, QStringLiteral("Invalid release metadata")); return;
        }
        job->tag = tag;
        const QString name = job->kind + ".dat";
        const QString prefix = Repository + QString::fromLatin1(QUrl::toPercentEncoding(tag)) + '/';
        QUrl checksum;
        int dataCount = 0, checksumCount = 0;
        for (const auto &entry : object.value("assets").toArray()) {
            const auto asset = entry.toObject();
            const QString assetName = asset.value("name").toString();
            if (assetName != name && assetName != name + ".sha256sum") continue;
            const QUrl url(asset.value("browser_download_url").toString());
            if (url.toString(QUrl::FullyEncoded) != prefix + assetName) {
                fail(job, QStringLiteral("Release asset does not match its exact tag")); return;
            }
            if (assetName == name) { job->dataUrl = url; ++dataCount; }
            else { checksum = url; ++checksumCount; }
        }
        if (dataCount != 1 || checksumCount != 1) { fail(job, QStringLiteral("Release database or checksum asset missing or duplicated")); return; }
        job->phase = 1;
        request(job, checksum);
    } else if (job->phase == 1) {
        const QString text = QString::fromLatin1(job->body).trimmed();
        const QRegularExpression pattern(QStringLiteral("^([A-Fa-f0-9]{64}) [ *]%1$").arg(QRegularExpression::escape(job->kind + ".dat")));
        const auto match = pattern.match(text);
        if (!match.hasMatch()) { fail(job, QStringLiteral("Invalid database SHA-256 checksum")); return; }
        job->hash = match.captured(1).toLower();
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/geodata";
        if (!QDir().mkpath(directory)) { fail(job, QStringLiteral("Cannot create database cache directory")); return; }
        job->file.setFileTemplate(directory + "/candidate-XXXXXX");
        if (!job->file.open()) { fail(job, QStringLiteral("Cannot create temporary database")); return; }
        job->phase = 2;
        request(job, job->dataUrl);
    } else {
        if (!job->received || QString::fromLatin1(job->digest.result().toHex()) != job->hash) {
            fail(job, QStringLiteral("Database SHA-256 checksum mismatch")); return;
        }
        if (!job->file.flush()) { fail(job, QStringLiteral("Cannot flush temporary database")); return; }
        job->file.close();
        job->phase = 3;
        job->deadline.stop(); job->inactivity.stop();
        emit candidateReady(job->kind, job->file.fileName(), job->hash);
    }
}
namespace {
bool interrupted() { return QThread::currentThread()->isInterruptionRequested(); }
bool hashFile(QFile &file, QCryptographicHash &hash) {
    qint64 read = 0;
    while (!file.atEnd()) {
        if (interrupted()) return false;
        const auto chunk = file.read(64 * 1024);
        read += chunk.size();
        if (chunk.isEmpty() || read > DataLimit) return false;
        hash.addData(chunk);
    }
    return true;
}
// Only immutable values cross into the worker; no Job or GUI QObject is accessed.
QString publishFile(const QString &candidate, const QString &expectedHash, const QString &kind, QString *error) {
    auto reject = [&](const QString &message) { if (error) *error = message; return QString(); };
    QFile input(candidate);
    if (!input.open(QIODevice::ReadOnly)) return reject(QStringLiteral("Cannot read database candidate"));
    if (input.size() <= 0 || input.size() > DataLimit) return reject(QStringLiteral("Database candidate size changed"));
    // Recheck bytes after asynchronous caller validation, before publication.
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hashFile(input, hash) || QString::fromLatin1(hash.result().toHex()) != expectedHash || !input.seek(0))
        return reject(QStringLiteral("Database candidate changed after download"));
    const QString directory = QFileInfo(input).absolutePath() + '/' + expectedHash;
    const QString path = directory + '/' + kind + ".dat";
    if (!QDir().mkpath(directory)) return reject(QStringLiteral("Cannot create immutable database directory"));
    // Serialize cooperating publishers so QSaveFile never replaces a cache hit.
    QLockFile lock(directory + "/.publish.lock");
    if (!lock.tryLock(0)) return reject(QStringLiteral("Database cache is busy"));
    if (QFileInfo(path).isSymLink()) return reject(QStringLiteral("Immutable database path is a symbolic link"));
    if (QFileInfo::exists(path)) {
        QFile existing(path);
        QCryptographicHash existingHash(QCryptographicHash::Sha256);
        if (!existing.open(QIODevice::ReadOnly) || existing.size() != input.size() || !hashFile(existing, existingHash)
            || QString::fromLatin1(existingHash.result().toHex()) != expectedHash)
            return reject(QStringLiteral("Existing immutable database is corrupt"));
    } else {
        QSaveFile output(path);
        output.setDirectWriteFallback(false);
        if (!output.open(QIODevice::WriteOnly)) return reject(QStringLiteral("Cannot create immutable database"));
        QCryptographicHash copiedHash(QCryptographicHash::Sha256);
        qint64 copied = 0;
        while (!input.atEnd()) {
            if (interrupted()) return reject(QStringLiteral("Publication canceled"));
            const QByteArray chunk = input.read(64 * 1024);
            copied += chunk.size();
            if (chunk.isEmpty() || copied > DataLimit || output.write(chunk) != chunk.size())
                return reject(QStringLiteral("Cannot write immutable database"));
            copiedHash.addData(chunk);
        }
        if (QString::fromLatin1(copiedHash.result().toHex()) != expectedHash)
            return reject(QStringLiteral("Database candidate changed during publication"));
        if (interrupted()) return reject(QStringLiteral("Publication canceled"));
        if (!output.commit()) return reject(QStringLiteral("Cannot commit immutable database"));
    }
    return path;
}
}
QString GeoDataDownload::publish(const QString &kind, QString *error) {
    if (error) error->clear();
    auto job = jobs_.value(kind);
    if (!job || job->phase != 3) {
        if (error) *error = QStringLiteral("No validated download candidate");
        return {};
    }
    const QString path = publishFile(job->file.fileName(), job->hash, kind, error);
    if (!path.isEmpty()) discard(kind);
    return path;
}
bool GeoDataDownload::publishAsync(const QString &kind) {
    auto job = jobs_.value(kind);
    if (!job || job->phase != 3) return false;
    job->phase = 4;
    struct Result { QString path, error; };
    auto result = std::make_shared<Result>();
    job->publisher.reset(QThread::create([candidate = job->file.fileName(), hash = job->hash, kind, result] {
        result->path = publishFile(candidate, hash, kind, &result->error);
    }));
    const std::weak_ptr<Job> weak = job;
    connect(job->publisher.get(), &QThread::finished, this, [this, weak, result] {
        auto j = weak.lock();
        if (!j || !current(j)) return;
        j->publisher->wait();
        const QString kind = j->kind;
        if (result->path.isEmpty()) { fail(j, result->error); return; }
        j.reset();
        QPointer<GeoDataDownload> guard(this);
        discard(kind);
        if (guard && !busy(kind)) emit published(kind, result->path);
    }, Qt::QueuedConnection);
    job->publisher->start();
    return true;
}
