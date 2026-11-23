#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QFutureWatcher>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QWidget>
#include <array>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QToolButton;

// Fields of one WireGuard/AmneziaWG document. Values are Core-decoded strings
// keyed by "Section/Key" (Core `bimeron wireguard` fields). Core validates the
// document; the Reserved inputs constrain individual bytes and retain partial
// input in the form. Absent keys stay empty instead of acquiring defaults.
class WireGuardForm final : public QWidget {
    Q_OBJECT
public:
    explicit WireGuardForm(QFormLayout *networkForm, const QString &coreExecutable, const QJsonArray &keyOperations, QWidget *parent = nullptr);
    void initializeNewKey();
    void invalidateKeyGeneration();
    bool keyGenerationPending() const;
    void showNetworkFields(bool visible, bool editable);

    // Main WireGuard keys and AmneziaWG keys, in display order.
    static const QStringList &mainFields();
    static const QStringList &amneziaFields();
    static const QString &reservedField();
    QString reservedEditError() const;
    static bool secretField(const QString &id);
    // AmneziaWG 3.1 booleans use a Not set / On / Off choice instead of free text.
    static bool choiceField(const QString &id);
    // The selector version ("1.0", "1.5", "2.0" or "3.1") that introduces a field.
    static QString fieldVersion(const QString &id);

    void setValues(const QJsonObject &fields);
    QMap<QString, QString> values() const;
    // Proposes a value for a new draft: it becomes an edit without counting as user input.
    void presetValue(const QString &id, const QString &value);
    // Fields changed since the last setValues(); only these become Core edits.
    QStringList changedFields() const { return changed_.values(); }
    QLineEdit *edit(const QString &id) const { return edits_.value(id); }
    // The control of a field: a line edit, or a choice for AmneziaWG booleans.
    QWidget *widget(const QString &id) const;
    void setStatus(const QString &text, bool problem);
    void setAmneziaExpanded(bool expanded);
    bool amneziaExpanded() const;

    // AmneziaWG version selector. It only controls which fields are shown; the
    // document changes only through field edits. Core's awg_version sets it on load.
    void setAmneziaVersion(const QString &coreVersion);
    QString amneziaVersion() const; // "" for plain WireGuard, else "1.0", "1.5", "2.0" or "3.1"

    // Core problems shown under their fields (the summary stays in the status line).
    void setFieldProblem(const QString &id, const QString &message);
    void clearFieldProblems();

signals:
    void edited();
    void versionChanged();
    void keyGenerated();

private:
    void refreshPublicKey();
    void generateKey(bool pair);
    QFutureWatcher<QJsonObject> *pairRequest_ = nullptr;
    QFutureWatcher<QJsonObject> *pskRequest_ = nullptr;
    QFutureWatcher<QJsonObject> *derivation_ = nullptr;
    quint64 keyRevision_ = 0;
    bool derivationPending_ = false;
    QString coreExecutable_;
    QLineEdit *publicKey_ = nullptr;
    bool generatePair_ = false;
    void refreshAmneziaSummary();
    void refreshVersionRows();
    void changeVersion(int index);
    bool fieldSet(const QString &id) const;
    void clearField(const QString &id);
    QMap<QString, QLineEdit *> edits_;
    std::array<QLineEdit *, 3> reservedBytes_{};
    QString loadedReserved_;
    QFormLayout *mainForm_ = nullptr;
    QMap<QString, QComboBox *> choices_;
    QMap<QString, QWidget *> rows_;          // Field containers, including the problem label.
    QMap<QString, QLabel *> problems_;
    QMap<QString, QLabel *> headings_;       // Version headings of the AmneziaWG section.
    QMap<QString, QString> loadedChoices_;   // Raw boolean values as loaded, for removal.
    QSet<QString> changed_;
    QFormLayout *networkForm_;
    QFormLayout *amneziaForm_ = nullptr;
    QComboBox *version_ = nullptr;
    int versionIndex_ = 0;
    QToolButton *amneziaToggle_ = nullptr;
    QWidget *amneziaBody_ = nullptr;
    QLabel *status_ = nullptr;
};
