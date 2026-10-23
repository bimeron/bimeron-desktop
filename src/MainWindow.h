#pragma once

#include "CoreClient.h"
#include "CoreUpdater.h"
#include "ProfileStore.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QMainWindow>

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

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void showWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *profilesPage();
    QWidget *settingsPage();
    QWidget *logPage();
    QWidget *aboutPage();
    void refreshProfiles();
    void addProfile();
    void importProfile();
    void removeProfile();
    void toggleConnection();
    void poll();
    void handleReply(const QString &command, const QJsonObject &response);
    void handleStatus(const QJsonObject &status);
    void updateProfileAppearance();
    void updateConnectionButton();
    void initTray();
    void rebuildTrayMenu();
    void ensureCoreService();
    void startProfile(int row);
    void requestShutdown();
    void saveDiagnostics();
    bool runServiceCommand(const QStringList &arguments, const QString &description);
    bool coreUpdatePrerequisite();
    void pollServiceCommand();
    void finishServiceCommand(int exitCode, const QString &details = {});
    void refreshServiceStatus();
    void refreshCoreAfterUpdate();
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
    QToolButton *profileGear_ = nullptr;
    int gearRow_ = -1;
    QTextEdit *log_ = nullptr;
    QSystemTrayIcon *tray_ = nullptr;
    QMenu *trayMenu_ = nullptr;
    QAction *trayStatusAction_ = nullptr;
    QAction *trayTrafficAction_ = nullptr;
    QString trayTraffic_ = tr("Traffic: —");
    QStackedWidget *pages_ = nullptr;
    QTimer *pollTimer_ = nullptr;
    QString state_ = "unknown";
    QString activeProfileId_;
    QString reconnectProfileAfterUpdate_;
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
    int pollCount_ = 0;
    bool stopRequested_ = false;
    bool connectionAttempted_ = false;
    bool quitRequested_ = false;
    bool serviceStartAttempted_ = false;
    bool startServiceAfterInstall_ = false;
    bool coreUpdateCheckStarted_ = false;
    bool coreUpdateCommand_ = false;
    quintptr serviceCommandProcess_ = 0;
    QProcess *linuxServiceProcess_ = nullptr;
    QString serviceCommandDescription_;
};
