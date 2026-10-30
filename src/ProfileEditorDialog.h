#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QMap>
#include <functional>

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
                                 std::function<bool(const QString &, QString *)> validateRaw = {});
    QString configText() const;

protected:
    void reject() override;

private:
    struct Rule { QString target; QString action; bool enabled = true; bool domain = false; };
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
    bool rawDirty_ = false;
    bool hasUnknownSettings_ = false;
    bool rawOnly_ = false;
    bool vlessMultiModeConfigured_ = false;
    bool keyRevealed_ = false;
    QString originalText_;
    QString loadedText_;
    QString visualBaseline_;
    std::function<void()> refreshControls_;
    std::function<bool(const QString &, QString *)> validateRaw_;
};
