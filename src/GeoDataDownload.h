#pragma once

#include <QObject>
#include <QHash>
#include <memory>

class QNetworkAccessManager;
class QUrl;

// Owns candidates until publish/discard/cancel. Caller must validate with Core
// before publish; candidateReady alone never changes the application selection.
class GeoDataDownload final : public QObject {
    Q_OBJECT
public:
    explicit GeoDataDownload(QObject *parent = nullptr);
    explicit GeoDataDownload(QNetworkAccessManager *network, QObject *parent = nullptr);
    ~GeoDataDownload() override;
    bool start(const QString &kind);
    bool busy(const QString &kind) const;
    QString releaseTag(const QString &kind) const;
    void cancel(const QString &kind);
    void discard(const QString &kind);
    // Starts filesystem publication; completion is delivered on this object's thread.
    bool publishAsync(const QString &kind);
    QString publish(const QString &kind, QString *error = nullptr);
signals:
    void published(QString kind, QString path);
    void progress(QString kind, qint64 received, qint64 total);
    void candidateReady(QString kind, QString temporaryPath, QString hash);
    void failed(QString kind, QString error);
    void finished(QString kind);
private:
    struct Job;
    void request(const std::shared_ptr<Job> &job, const QUrl &url, int redirects = 0);
    void consume(const std::shared_ptr<Job> &job);
    void complete(const std::shared_ptr<Job> &job);
    void fail(const std::shared_ptr<Job> &job, const QString &error);
    bool current(const std::shared_ptr<Job> &job) const;
    QNetworkAccessManager *network_;
    QHash<QString, std::shared_ptr<Job>> jobs_;
};
