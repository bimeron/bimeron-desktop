#pragma once

#include <QDialog>
#include <QJsonObject>

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
    explicit ProfileEditorDialog(const QString &text, const QJsonObject &capabilities, QWidget *parent = nullptr);
    QString configText() const;

private:
    struct Rule { QString target; QString action; bool enabled = true; bool domain = false; };
    void loadText(const QString &text);
    QString buildText() const;
    void addRule(const Rule &rule);
    void moveRule(int delta);
    void syncRaw();

    QTabWidget *tabs_ = nullptr;
    QLineEdit *name_ = nullptr;
    QComboBox *keySource_ = nullptr;
    QLineEdit *key_ = nullptr;
    QLineEdit *server_ = nullptr;
    QSpinBox *port_ = nullptr;
    QComboBox *method_ = nullptr;
    QLineEdit *password_ = nullptr;
    QLineEdit *username_ = nullptr;
    QLineEdit *prefix_ = nullptr;
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
    bool keyRevealed_ = false;
};
