#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <functional>

#include "CoreProblems.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
class QTabWidget;

class ProfileEditorDialog final : public QDialog {
    Q_OBJECT
public:
    explicit ProfileEditorDialog(const QString &text, const QJsonObject &capabilities, QWidget *parent = nullptr,
                                 std::function<bool(const QString &, CoreFailure *)> validateRaw = {},
                                 const QString &coreExecutable = {}, const QString &profilePath = {}, bool create = false);
    ~ProfileEditorDialog() override;
    QString configText() const;
    // Developer sessions may convert the access source between a link and
    // fields when Core reports access_conversion.
    void setDeveloperMode(bool enabled);

protected:
    void reject() override;

private:
    friend class MainWindow;
    friend struct ConnectionManagementTest;
    static QDialog *nativeConfirmation(QWidget *parent, const QString &profile,
                                       const QString &server, const QString &action, bool reviewable, const QJsonObject &plan = {});
    QWidget *nativeProfileActions_ = nullptr;
    QWidget *nativeAuthentication_ = nullptr;
    QComboBox *nativeAuthMethod_ = nullptr, *nativeAuthSource_ = nullptr;
    bool nativeAuthPreview_ = false;
    std::function<void()> refreshNativeAuthentication_;
    struct Rule { QString target; QString action; bool enabled = true; QString matcher = "CIDR"; int index = -1; bool editable = true; QString error; };
    Rule tableRule(int row) const;
    QJsonObject document(const QJsonArray &operations, QString *error) const;
    QString documentText() const;
    QString routingError() const;
    QString matcherError(const QString &matcher) const;
    void refreshRouting();
    void manageDatabases();
    void inspectDataset(const QString &kind);
    void selectGroups(QLineEdit *target, QComboBox *type);
    QString coreExecutable_, profilePath_;
    QJsonObject capabilities_;
    QJsonArray documentRules_;
    QJsonObject documentFields_;
    bool populatingRules_ = false;
    bool bridgeReady_ = false;
    int localRule_ = -1;
    QLabel *routingStatus_ = nullptr;
    QMap<QString, QString> datasetPaths_, datasetErrors_, datasetIdentities_, datasetKeys_;
    // Kinds whose shared database is missing or failed its check. Rules of such kinds
    // may be saved, but the profile cannot connect while they are enabled.
    QMap<QString, bool> datasetUnavailable_;
    bool datasetChanged_ = false;
    QMap<QString, QStringList> datasetGroups_;
    QMap<QString, int> datasetGeneration_;
    mutable QString bridgeError_;
    mutable QString cachedVisual_, cachedDocument_;
    void loadText(const QString &text);
    QString buildText() const;
    void addRule(const Rule &rule);
    void moveRule(int delta);
    void syncRaw();
    void assessRepresentation();
    bool applyRaw();
    void refreshParameters();
    QString parameterError() const;
    QStringList parameterFields() const;
    QMap<QString, QLineEdit *> parameters_;
    QComboBox *trojanNetwork_ = nullptr;
    QCheckBox *shadowTls_ = nullptr;
    QLineEdit *shadowName_ = nullptr;
    QLineEdit *shadowPassword_ = nullptr;
    QLabel *shadowGuidance_ = nullptr;
    QWidget *parameterPage_ = nullptr;

    bool native_ = false;
    QLineEdit *windowsProfile_ = nullptr;
    QLineEdit *nativeServerName_ = nullptr;
    QLabel *nativeMode_ = nullptr;
    QCheckBox *nativeKillSwitch_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QLineEdit *name_ = nullptr;
    QComboBox *keySource_ = nullptr;
    QLineEdit *key_ = nullptr;
    QLineEdit *server_ = nullptr;
    QSpinBox *port_ = nullptr;
    QComboBox *method_ = nullptr;
    QLineEdit *password_ = nullptr;
    QLineEdit *username_ = nullptr;
    QComboBox *ssPlugin_ = nullptr;
    QLineEdit *ssPluginOpts_ = nullptr;
    QLineEdit *sshIdentity_ = nullptr;
    QLineEdit *sshKnownHosts_ = nullptr;
    QLineEdit *prefix_ = nullptr;
    QLineEdit *vlessId_ = nullptr;
    QComboBox *vlessNetwork_ = nullptr;
    QComboBox *vlessSecurity_ = nullptr;
    QComboBox *vlessFlow_ = nullptr;
    QLineEdit *vlessServerName_ = nullptr;
    QComboBox *vlessFingerprint_ = nullptr;
    QLineEdit *vlessPublicKey_ = nullptr;
    QLineEdit *vlessShortId_ = nullptr;
    QLineEdit *vlessSpiderX_ = nullptr;
    QLineEdit *vlessAlpn_ = nullptr;
    QLineEdit *vlessPath_ = nullptr;
    QLineEdit *vlessHost_ = nullptr;
    QSpinBox *vlessEarlyData_ = nullptr;
    QLineEdit *vlessServiceName_ = nullptr;
    QLineEdit *vlessAuthority_ = nullptr;
    QCheckBox *vlessMultiMode_ = nullptr;
    QComboBox *vlessXhttpMode_ = nullptr;
    QComboBox *mode_ = nullptr;
    QLineEdit *listen_ = nullptr;
    QLineEdit *interfaceName_ = nullptr;
    QSpinBox *mtu_ = nullptr;
    QLineEdit *dns_ = nullptr;
    QComboBox *ipv6_ = nullptr;
    QCheckBox *killSwitch_ = nullptr;
    QCheckBox *ignoreLocalNetwork_ = nullptr;
    QComboBox *defaultRoute_ = nullptr;
    QTableWidget *rules_ = nullptr;
    QPlainTextEdit *raw_ = nullptr;
    bool creating_ = false;
    bool freshDraft_ = false;
    bool rawDirty_ = false;
    bool hasUnknownSettings_ = false;
    bool rawOnly_ = false;
    bool vlessMultiModeConfigured_ = false;
    bool keyRevealed_ = false;
    bool developerMode_ = false;
    QWidget *accessConversion_ = nullptr;
    void convertAccess(bool toFields);
    void ensureSourceItem(const QString &source);
    QString originalText_;
    QString loadedText_;
    QString visualBaseline_;
    std::function<void()> refreshControls_;
    std::function<bool(const QString &, CoreFailure *)> validateRaw_;
    // Problem presentation: maps Core locations to editor fields.
    QWidget *problemWidget(const CoreProblem &problem) const;
    QString problemLabel(QWidget *widget) const;
    void showProblems(const CoreFailure &failure);
    void focusProblem(const CoreProblem &problem);
    void markProblemField(QWidget *widget);
    void clearProblemMarks();
};
