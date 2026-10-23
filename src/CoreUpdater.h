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
    explicit CoreUpdater(QObject *parent = nullptr);
    ~CoreUpdater() override;
    bool available() const;
    bool busy() const { return busy_; }
    void setInstalling(bool installing) { installing_ = installing; }
    void discardStaged() { staged_.reset(); ready_ = false; }
    void check(const QString &installedVersion, int ipcVersion, bool betaAllowed);

signals:
    void statusChanged(QString message);
    void ready(QString version, QString executablePath, QString sha256);

private:
    struct Release {
        QString tag;
        QHash<QString, QUrl> assets;
    };
    using FetchCallback = std::function<void(QByteArray, QString)>;
    void fetch(const QUrl &url, qint64 limit, FetchCallback callback);
    void inspectNext();
    void downloadArchive(const CoreUpdateRelease &release, const QUrl &url);
    QString platform() const;
    QString architecture() const;

    QNetworkAccessManager network_;
    QByteArray publicKey_;
    QList<Release> releases_;
    std::unique_ptr<QTemporaryDir> staged_;
    QString installedVersion_;
    int ipcVersion_ = 1;
    bool betaAllowed_ = false;
    bool busy_ = false;
    bool installing_ = false;
    bool ready_ = false;
};
