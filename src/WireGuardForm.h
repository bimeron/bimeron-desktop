#pragma once

#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QWidget>

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

    void setValues(const QJsonObject &fields);
    QMap<QString, QString> values() const;
    QLineEdit *edit(const QString &id) const { return edits_.value(id); }
    void setStatus(const QString &text, bool problem);
    void setAmneziaExpanded(bool expanded);
    bool amneziaExpanded() const;

signals:
    void edited();

private:
    void refreshAmneziaSummary();
    QMap<QString, QLineEdit *> edits_;
    QToolButton *amneziaToggle_ = nullptr;
    QWidget *amneziaBody_ = nullptr;
    QLabel *status_ = nullptr;
};
