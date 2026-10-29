#pragma once

#include "CoreClient.h"
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <functional>

class DevelopmentCore final : public QObject {
    Q_OBJECT
public:
    explicit DevelopmentCore(QString helper, QObject *parent = nullptr);
    ~DevelopmentCore() override;
    bool busy() const { return busy_; }
    bool local() const { return !path_.isEmpty(); }
    bool needsCleanup() const { return local() || restoreService_; }
    bool failed() const { return failed_; }
    QString path() const { return path_; }
    QString endpoint() const { return endpoint_; }
    qint64 processId() const { return processId_; }
    QString status() const { return status_; }
    void select(const QString &path);
    bool orphanAvailable() const { return orphanAvailable_; }
    void stopOrphan(bool force = false);

signals:
    void changed();
    void completed(bool success);
    void forcedExitReady();

private:
    using Result = std::function<void(int, QString)>;
    void command(const QString &file, const QStringList &arguments, bool elevated, Result result);
    void prepare();
    void launch();
    void shutdown();
    void verifyExit();
    void verifyLock();
    void restore();
    void confirmServiceStarted(int attempts);
    void completeRestore();
    void tick();
    void finish(bool success, const QString &message);
    void releaseProcess();
    bool processRunning() const;
    QString helper_;
    QString path_;
    QString requestedPath_;
    QString endpoint_;
    QString status_;
    QString failure_;
    qint64 processId_ = 0;
    quintptr processHandle_ = 0;
    QProcess *process_ = nullptr;
    CoreClient control_{this};
    QTimer timer_{this};
    QElapsedTimer deadline_;
    enum class Phase { Idle, Starting, Active, Stopping, Verifying, Failed };
    Phase phase_ = Phase::Idle;
    Phase exitPhase_ = Phase::Idle;
    bool orphanAvailable_ = false;
    bool busy_ = false;
    bool failed_ = false;
    bool restoreService_ = false;
    bool shutdownSent_ = false;
    bool stopConfirmed_ = false;
};
