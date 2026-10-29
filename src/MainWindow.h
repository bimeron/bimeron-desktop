#pragma once

#include "CoreClient.h"
#include "CoreUpdater.h"
#include "ProfileStore.h"
#include "DevelopmentCore.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QMainWindow>
#include <QPointer>
#include <QSet>
#include <QJsonArray>
#include <QDialog>

class QLabel;
class QAction;
class QCheckBox;
class QIcon;
class QMenu;
class QProcess;
class QSystemTrayIcon;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class QTimer;
class QToolButton;
class QComboBox;
class QLineEdit;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void showWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend struct ConnectionManagementTest;
    QWidget *profilesPage();
    QWidget *settingsPage();
    QWidget *logPage();
    QWidget *aboutPage();
    QWidget *developmentPage();
    void restartApplication(bool development = false);
    void updateDevelopment();
    void refreshProfiles();
    ProfileConnectionInfo profileInfo(int row) const;
    bool activeProxyMode() const;
    void addProfile();
    void importProfile();
    void removeProfile();
    void moveProfile(int delta);
    void toggleConnection();
    void poll();
    void clearSSHAuthentication();
    void handleSSHAuthentication(const QJsonObject &auth);
    QPointer<QDialog> sshDialog_;
    QTimer *sshAuthExpiry_ = nullptr;
    QString sshChallenge_;
    QSet<QString> sshSeen_;
    QJsonArray sshAnswers_;
    bool sshReplyReady_ = false;
    bool sshAuthSupported_ = true;
    QString sshUnsupportedInstance_;
    qint64 nextSSHAuth_ = 0;
    void handleReply(const QString &command, const QJsonObject &response);
    void handleStatus(const QJsonObject &status);
    void updateProfileAppearance();
    void updateConnectionButton();
    void initTray();
    void rebuildTrayMenu();
    bool startProfile(int row);
    bool controlBlocked() const;
    bool cleanDisconnected() const;
    bool canActivateProfile(int row) const;
    void invalidateConnection();
    void resetMeasurements();
    void connectionFailure(const QString &command, CoreClient::Failure reason, const QString &message);
    void completeSynchronization();
    void probeValidationCli();
    void updateCoreIdentity();
    void requestShutdown();
    void saveDiagnostics();
    bool runServiceCommand(const QStringList &arguments, const QString &description);
    bool coreUpdatePrerequisite();
    void pollServiceCommand();
    void finishServiceCommand(int exitCode, const QString &details = {});
    void refreshServiceStatus();
    void refreshCoreAfterUpdate();
    void reportCoreUpdateResult(const QString &message);
    void recoverNetwork();
    void applyStartupSetting(bool enabled);
    void editProfile(int row);
    QIcon trayIcon(bool connected) const;
    void showCoreError(const QJsonObject &response);
    void appendLog(const QString &message);
    bool validateConfig(const QString &path, QString *error = nullptr) const;
    QString humanBytes(qint64 bytes) const;

    ProfileStore store_;
    QList<Profile> profiles_;
    CoreClient core_;
    CoreUpdater updater_{this};
    QListWidget *profileList_ = nullptr;
    QLabel *connectionLabel_ = nullptr;
    QLabel *trafficLabel_ = nullptr;
    QLabel *sessionStatsLabel_ = nullptr;
    QLabel *coreVersionLabel_ = nullptr;
    QLabel *serviceStatusLabel_ = nullptr;
    QLabel *coreUpdateLabel_ = nullptr;
    QPushButton *serviceInstallButton_ = nullptr;
    QPushButton *serviceStartButton_ = nullptr;
    QPushButton *serviceStopButton_ = nullptr;
    QPushButton *serviceUpdateButton_ = nullptr;
    QPushButton *serviceUninstallButton_ = nullptr;
    QPushButton *serviceRecoveryButton_ = nullptr;
    QCheckBox *serviceAutomaticCheck_ = nullptr;
    QCheckBox *betaUpdatesCheck_ = nullptr;
    QComboBox *autoConnectProfile_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *removeProfileButton_ = nullptr;
    QToolButton *profileGear_ = nullptr;
    int gearRow_ = -1;
    QTextEdit *log_ = nullptr;
    QSystemTrayIcon *tray_ = nullptr;
    QMenu *trayMenu_ = nullptr;
    QAction *trayStatusAction_ = nullptr;
    QAction *trayTrafficAction_ = nullptr;
    QString trayTraffic_ = tr("Traffic: —");
    QStackedWidget *pages_ = nullptr;
    QString state_ = "unknown";
    QString activeFileProtocol_;
    QString activeProfileId_;
    bool lastProxyMode_ = false;
    QString reconnectProfileAfterUpdate_;
    QString reconnectResultProfileId_;
    QString expectedCoreVersionAfterUpdate_;
    QString lastCoreError_;
    QString runningCoreVersion_;
    QString pendingStartPath_;
    QString instance_;
    QString sessionId_;
    QJsonObject capabilities_;
    bool autoConnectAttempted_ = false;
    qint64 previousUp_ = 0;
    qint64 previousDown_ = 0;
    QElapsedTimer statsClock_;
    quint64 eventCursor_ = 0;
    QElapsedTimer pollClock_;
    qint64 nextEvents_ = 0;
    qint64 nextStatus_ = 0;
    qint64 nextStats_ = 0;
    qint64 retryAt_ = 0;
    int retryCount_ = 0;
    bool helloReady_ = false;
    bool statusNeeded_ = true;
    bool sessionActive_ = false;
    bool nativeSession_ = false;
    bool cleanupBlocked_ = false;
    QString operation_;
    QString startReadyPath_;
    quint64 connectionGeneration_ = 0;
    QProcess *validationProcess_ = nullptr;
    QProcess *serviceStatusProcess_ = nullptr;
    bool recoveryCommand_ = false;
    bool recoveryVerified_ = false;
    QString recoveredSession_;
    QString recoveredInstance_;
    QString sessionMode_;
    QJsonObject runtimeIdentity_;
    QJsonObject validationIdentity_;
    QString validationExecutable_;
    bool stopRequested_ = false;
    bool connectionAttempted_ = false;
    bool quitRequested_ = false;
    bool startServiceAfterInstall_ = false;
    bool coreUpdateCheckStarted_ = false;
    bool coreUpdateCommand_ = false;
    bool coreUpdateAwaitingResult_ = false;
    quintptr serviceCommandProcess_ = 0;
    QProcess *linuxServiceProcess_ = nullptr;
    QString serviceCommandDescription_;
    DevelopmentCore *development_ = nullptr;
    QLabel *desktopVersionLabel_ = nullptr;
    QLabel *developmentStatus_ = nullptr;
    QLabel *developmentModified_ = nullptr;
    QComboBox *developmentBackend_ = nullptr;
    QLineEdit *developmentPath_ = nullptr;
    QPushButton *developmentApply_ = nullptr;
    int developmentClicks_ = 0;
    bool developerMode_ = false;
    bool developmentExit_ = false;
    bool restartRequested_ = false;
};
