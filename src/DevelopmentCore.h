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
    bool needsCleanup() const { return !interruptedRecovery_ && (local() || restoreService_); }
    bool failed() const { return failed_; }
    QString path() const { return path_; }
    QString endpoint() const { return endpoint_; }
    qint64 processId() const { return processId_; }
    QString status() const { return status_; }
    void select(const QString &path);
    bool orphanAvailable() const { return orphanAvailable_; }
    void stopOrphan(bool force = false);
    bool recoveryRestoresRunning() const { return journalOriginalState_ == "running"; }
    bool interruptedCleanupRequired() const { return interruptedRecovery_ && interruptedCleanupNeeded_; }
    bool interruptedRecovery() const { return interruptedRecovery_; }
    QString recoveryDiagnostic() const { return recoveryDiagnostic_; }
    void recoverInterrupted();
    void recoverInterruptedNetwork();
    // Call only after authenticated broker readiness and successful process exit.
    // Never stops a process or launches another privileged operation.
    bool brokerRestoredSystem();
    static QString recoveryJournalPath();
    enum class StartBlock { None, LifecycleConflict, EndpointOccupied, EndpointAccessDenied,
        EndpointUnknown, LockOccupied, LockAccessDenied, LockUnknown, CleanupUnconfirmed, TimedOut };
    struct StartInspection {
        StartBlock reason = StartBlock::None;
        QString diagnostic;
        bool allowed() const { return reason == StartBlock::None; }
    };
    void inspectSystemStart(std::function<void(StartInspection)> done);
    void inspectSystemStart(std::function<void(bool, QString)> done);

signals:
    void changed();
    void completed(bool success);
    void forcedExitReady();

private:
    friend struct DevelopmentRecoveryTest;
    friend struct ConnectionManagementTest;
    void inspectSystemStartAtEndpoint(const QString &endpoint, std::function<void(StartInspection)> done);
    using Result = std::function<void(int, QString)>;
    void command(const QString &file, const QStringList &arguments, bool elevated, Result result);
    void prepare();
    void launch();
    void shutdown();
    void verifyExit();
    void verifyLock();
    void verifiedLock(bool free);
    void restore();
    void confirmServiceStarted(int attempts);
    void completeRestore();
    void tick();
    void finish(bool success, const QString &message);
    void releaseProcess();
    bool processRunning() const;
    void readJournal();
    bool writeJournal(const QString &stage);
    bool clearJournal();
    void inspectInterruptedEndpoint();
    void reconcileLinuxService(qint64 pid, const QString &endpoint = CoreClient::systemEndpoint(),
        std::function<void(std::function<void(qint64)>)> query = {});
    void queryLinuxService(std::function<void(qint64)> result);
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
    bool interruptedRecovery_ = false;
    bool journalActive_ = false;
    QString journalOriginalState_;
    QString journalTransition_;
    QString recoveryDiagnostic_;
    bool interruptedCleanupConfirmed_ = false;
    bool interruptedCleanupNeeded_ = false;
};
