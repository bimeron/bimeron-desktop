#pragma once

#include "CoreUpdate.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTemporaryDir>
#include <QUrl>
#include <functional>
#include <memory>

class CoreUpdater final : public QObject {
    Q_OBJECT
public:
    using FetchCallback = std::function<void(QByteArray, QString)>;

    explicit CoreUpdater(QObject *parent = nullptr);
    ~CoreUpdater() override;
    bool available() const;
    bool busy() const { return busy_; }
    void setInstalling(bool installing) { installing_ = installing; }
    // A downloaded and verified executable stays staged for the rest of the
    // session until it is installed or no longer applies.
    bool hasStaged() const { return staged_ != nullptr; }
    QString stagedVersion() const;
    void discardStaged();
    // Discards the staged executable when it was prepared for another installed
    // Core version or service instance, or is a beta while betas are disabled.
    // Returns true when it was discarded.
    bool stagedApplies(const QString &installedVersion, const QString &instance, bool betaAllowed) const;
    bool discardStagedUnless(const QString &installedVersion, const QString &instance, bool betaAllowed);
    // Turning beta updates off discards a staged beta release at once.
    void applyBetaSetting(bool betaAllowed);
    // Re-hashes the staged executable right before installation. On a mismatch
    // or a read failure it discards the staged executable and returns false.
    bool verifyStaged(QString *path, QString *sha256);
    void check(const QString &installedVersion, int ipcVersion, bool betaAllowed, const QString &instance);

#ifdef BIMERON_CORE_UPDATER_TESTING
    using Fetcher = std::function<void(const QUrl &, qint64, FetchCallback)>;
    void setTestHooks(const QList<QByteArray> &publicKeys, Fetcher fetcher, const QString &platform,
                      const QString &architecture, const QUrl &releaseList) {
        publicKeys_ = publicKeys;
        testFetcher_ = std::move(fetcher);
        testPlatform_ = platform;
        testArchitecture_ = architecture;
        testReleaseList_ = releaseList;
    }
#endif

signals:
    void statusChanged(QString message);
    // A newly staged update is ready; a kept postponed update does not emit this again.
    void ready(QString version);
    void stagedChanged();

private:
    struct Release {
        QString tag;
        QHash<QString, QUrl> assets;
    };
    struct Staged {
        std::unique_ptr<QTemporaryDir> directory;
        CoreUpdateRelease release;
        QString path;
        QString sha256;
        QString installedVersion;
        QString instance;
    };
    void fetch(const QUrl &url, qint64 limit, FetchCallback callback);
    void inspectNext();
    void downloadArchive(const CoreUpdateRelease &release, const QUrl &url);
    void finish(const QString &message);
    void discard(const QString &message);
    QString platform() const;
    QString architecture() const;

    QNetworkAccessManager network_;
    QList<QByteArray> publicKeys_;
    QList<Release> releases_;
    std::unique_ptr<Staged> staged_;
    QString installedVersion_;
    QString instance_;
    int ipcVersion_ = 1;
    bool betaAllowed_ = false;
    bool busy_ = false;
    bool installing_ = false;
#ifdef BIMERON_CORE_UPDATER_TESTING
    Fetcher testFetcher_;
    QString testPlatform_;
    QString testArchitecture_;
    QUrl testReleaseList_;
#endif
};
