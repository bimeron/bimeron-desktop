#pragma once

#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QToolButton;

// Fields of one WireGuard/AmneziaWG document. Values are Core-decoded strings
// keyed by "Section/Key" (Core `bimeron wireguard` fields); the form neither
// parses nor validates them. Every field is a plain text edit so an absent key
// stays absent instead of acquiring a displayed default.
class WireGuardForm final : public QWidget {
    Q_OBJECT
public:
    explicit WireGuardForm(QWidget *parent = nullptr);

    // Main WireGuard keys and AmneziaWG keys, in display order.
    static const QStringList &mainFields();
    static const QStringList &amneziaFields();
    static bool secretField(const QString &id);
    // AmneziaWG 3.1 booleans use a Not set / On / Off choice instead of free text.
    static bool choiceField(const QString &id);

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

signals:
    void edited();

private:
    void refreshAmneziaSummary();
    QMap<QString, QLineEdit *> edits_;
    QMap<QString, QComboBox *> choices_;
    QSet<QString> changed_;
    QToolButton *amneziaToggle_ = nullptr;
    QWidget *amneziaBody_ = nullptr;
    QLabel *status_ = nullptr;
};
