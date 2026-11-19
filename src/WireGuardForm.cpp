#include "WireGuardForm.h"
#include "HiDpiComboBox.h"

#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
struct FieldText {
    const char *id;
    const char *label;
    const char *hint;
};

// Labels and hints are translated where the form is built.
const QList<FieldText> &mainTexts() {
    static const QList<FieldText> texts{
        {"Interface/PrivateKey", QT_TRANSLATE_NOOP("WireGuardForm", "Private key"), ""},
        {"Interface/Address", QT_TRANSLATE_NOOP("WireGuardForm", "Addresses"), QT_TRANSLATE_NOOP("WireGuardForm", "Tunnel addresses, for example 10.0.0.2/32, fd00::2/128")},
        {"Interface/DNS", QT_TRANSLATE_NOOP("WireGuardForm", "DNS"), QT_TRANSLATE_NOOP("WireGuardForm", "Used when the Network tab sets no DNS")},
        {"Interface/MTU", QT_TRANSLATE_NOOP("WireGuardForm", "Tunnel MTU"), QT_TRANSLATE_NOOP("WireGuardForm", "Empty uses 1420")},
        {"Peer/PublicKey", QT_TRANSLATE_NOOP("WireGuardForm", "Server public key"), ""},
        {"Peer/PresharedKey", QT_TRANSLATE_NOOP("WireGuardForm", "Preshared key"), QT_TRANSLATE_NOOP("WireGuardForm", "Optional")},
        {"Peer/Endpoint", QT_TRANSLATE_NOOP("WireGuardForm", "Endpoint"), QT_TRANSLATE_NOOP("WireGuardForm", "host:port")},
        {"Peer/AllowedIPs", QT_TRANSLATE_NOOP("WireGuardForm", "AllowedIPs"), QT_TRANSLATE_NOOP("WireGuardForm", "Networks the server accepts, for example 0.0.0.0/0, ::/0")},
        {"Peer/PersistentKeepalive", QT_TRANSLATE_NOOP("WireGuardForm", "Keepalive, seconds"), QT_TRANSLATE_NOOP("WireGuardForm", "Optional, for example 25")},
    };
    return texts;
}

struct AmneziaText {
    const char *version;
    const char *key;
    const char *hint;
};

const QList<AmneziaText> &amneziaTexts() {
    static const QList<AmneziaText> texts{
        {"1.0", "Jc", QT_TRANSLATE_NOOP("WireGuardForm", "Number of junk packets before the handshake")},
        {"1.0", "Jmin", QT_TRANSLATE_NOOP("WireGuardForm", "Minimum junk packet size")},
        {"1.0", "Jmax", QT_TRANSLATE_NOOP("WireGuardForm", "Maximum junk packet size")},
        {"1.0", "S1", QT_TRANSLATE_NOOP("WireGuardForm", "Handshake initiation padding")},
        {"1.0", "S2", QT_TRANSLATE_NOOP("WireGuardForm", "Handshake response padding")},
        {"1.0", "H1", QT_TRANSLATE_NOOP("WireGuardForm", "Initiation header: value or range lo-hi (ranges since 2.0)")},
        {"1.0", "H2", QT_TRANSLATE_NOOP("WireGuardForm", "Response header: value or range lo-hi (ranges since 2.0)")},
        {"1.0", "H3", QT_TRANSLATE_NOOP("WireGuardForm", "Cookie header: value or range lo-hi (ranges since 2.0)")},
        {"1.0", "H4", QT_TRANSLATE_NOOP("WireGuardForm", "Transport header: value or range lo-hi (ranges since 2.0)")},
        {"1.5", "I1", QT_TRANSLATE_NOOP("WireGuardForm", "Special junk packet 1")},
        {"1.5", "I2", QT_TRANSLATE_NOOP("WireGuardForm", "Special junk packet 2")},
        {"1.5", "I3", QT_TRANSLATE_NOOP("WireGuardForm", "Special junk packet 3")},
        {"1.5", "I4", QT_TRANSLATE_NOOP("WireGuardForm", "Special junk packet 4")},
        {"1.5", "I5", QT_TRANSLATE_NOOP("WireGuardForm", "Special junk packet 5")},
        {"2.0", "S3", QT_TRANSLATE_NOOP("WireGuardForm", "Cookie reply padding")},
        {"2.0", "S4", QT_TRANSLATE_NOOP("WireGuardForm", "Transport packet padding")},
        {"3.0", "HeaderProtectionKey", QT_TRANSLATE_NOOP("WireGuardForm", "Header protection key")},
        {"3.0", "ContentPaddingAddition", QT_TRANSLATE_NOOP("WireGuardForm", "Additional content padding")},
        {"3.0", "RekeyAfterTime", QT_TRANSLATE_NOOP("WireGuardForm", "Rekey after, seconds")},
        {"3.0", "RekeyTimeout", QT_TRANSLATE_NOOP("WireGuardForm", "Rekey timeout, seconds")},
        {"3.0", "RejectAfterTime", QT_TRANSLATE_NOOP("WireGuardForm", "Reject after, seconds")},
        {"3.0", "KeepaliveTimeout", QT_TRANSLATE_NOOP("WireGuardForm", "Keepalive timeout, seconds")},
        {"3.0", "MaxHandshakeAttempts", QT_TRANSLATE_NOOP("WireGuardForm", "Maximum handshake attempts")},
        {"3.1", "RandomTrailers", QT_TRANSLATE_NOOP("WireGuardForm", "Random packet trailers")},
        {"3.1", "DisableCookies", QT_TRANSLATE_NOOP("WireGuardForm", "Disable cookie replies")},
    };
    return texts;
}

QString tr(const char *text) { return QCoreApplication::translate("WireGuardForm", text); }
// Short words such as "On" have other translations elsewhere; the disambiguation
// selects the AmneziaWG option wording.
QString choiceText(const char *text) { return QCoreApplication::translate("WireGuardForm", text, "AmneziaWG option"); }

// Not set removes the key; On and Off use the amneziawg-tools spelling 1 and 0.
QComboBox *makeChoice(const QString &id, const QString &hint) {
    auto *choice = new HiDpiComboBox;
    choice->setObjectName("wireGuard" + QString(id).replace('/', '_'));
    choice->setToolTip(hint);
    choice->addItem(choiceText("Not set"), QString());
    choice->setItemData(0, choiceText("The key is not written; the default applies."), Qt::ToolTipRole);
    choice->addItem(choiceText("On"), QStringLiteral("1"));
    choice->setItemData(1, choiceText("Written as 1."), Qt::ToolTipRole);
    choice->addItem(choiceText("Off"), QStringLiteral("0"));
    choice->setItemData(2, choiceText("Written as 0."), Qt::ToolTipRole);
    return choice;
}

QLineEdit *makeEdit(const QString &id) {
    auto *edit = new QLineEdit;
    edit->setObjectName("wireGuard" + QString(id).replace('/', '_'));
    // The default 32767-character limit would truncate long lists on load.
    edit->setMaxLength(64 * 1024);
    if (WireGuardForm::secretField(id)) edit->setEchoMode(QLineEdit::Password);
    return edit;
}

// Secret fields are masked; Show reveals the value in this editor only.
QWidget *fieldRow(QLineEdit *edit) {
    if (edit->echoMode() != QLineEdit::Password) return edit;
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(edit);
    auto *show = new QPushButton(tr("Show"));
    show->setObjectName(edit->objectName() + "Reveal");
    show->setAutoDefault(false);
    layout->addWidget(show);
    QObject::connect(show, &QPushButton::clicked, edit, [edit, show] {
        const bool reveal = edit->echoMode() == QLineEdit::Password;
        edit->setEchoMode(reveal ? QLineEdit::Normal : QLineEdit::Password);
        show->setText(reveal ? tr("Hide") : tr("Show"));
    });
    return row;
}
} // namespace

const QStringList &WireGuardForm::mainFields() {
    static const QStringList fields = [] {
        QStringList result;
        for (const auto &text : mainTexts()) result << text.id;
        return result;
    }();
    return fields;
}

const QStringList &WireGuardForm::amneziaFields() {
    static const QStringList fields = [] {
        QStringList result;
        for (const auto &text : amneziaTexts()) result << QStringLiteral("Interface/") + text.key;
        return result;
    }();
    return fields;
}

bool WireGuardForm::choiceField(const QString &id) {
    return id == "Interface/RandomTrailers" || id == "Interface/DisableCookies";
}

QWidget *WireGuardForm::widget(const QString &id) const {
    if (QWidget *field = edits_.value(id)) return field;
    return choices_.value(id);
}

bool WireGuardForm::secretField(const QString &id) {
    return id == "Interface/PrivateKey" || id == "Peer/PresharedKey" || id == "Interface/HeaderProtectionKey";
}

WireGuardForm::WireGuardForm(QWidget *parent) : QWidget(parent) {
    setObjectName("wireGuardForm");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    // Problem labels are found through the QFormLayout of the field's parent widget.
    auto *mainFields = new QWidget;
    auto *main = new QFormLayout(mainFields);
    main->setContentsMargins(0, 0, 0, 0);
    for (const auto &text : mainTexts()) {
        auto *edit = makeEdit(text.id);
        if (*text.hint) edit->setPlaceholderText(tr(text.hint));
        edits_.insert(text.id, edit);
        main->addRow(tr(text.label), fieldRow(edit));
    }
    layout->addWidget(mainFields);

    amneziaToggle_ = new QToolButton;
    amneziaToggle_->setObjectName("amneziaToggle");
    amneziaToggle_->setCheckable(true);
    amneziaToggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    amneziaToggle_->setArrowType(Qt::RightArrow);
    amneziaToggle_->setAutoRaise(true);
    layout->addWidget(amneziaToggle_);
    amneziaBody_ = new QWidget;
    amneziaBody_->setObjectName("amneziaParameters");
    auto *amnezia = new QFormLayout(amneziaBody_);
    amnezia->setContentsMargins(16, 0, 0, 0);
    auto *intro = new QLabel(tr("Obfuscation parameters of AmneziaWG. Leave them empty for plain WireGuard; they must match the server."));
    intro->setObjectName("sectionHint");
    intro->setWordWrap(true);
    amnezia->addRow(intro);
    QString version;
    for (const auto &text : amneziaTexts()) {
        if (version != text.version) {
            version = text.version;
            auto *heading = new QLabel(tr("AmneziaWG %1").arg(version));
            heading->setObjectName("sectionTitle");
            amnezia->addRow(heading);
        }
        const QString id = QStringLiteral("Interface/") + text.key;
        if (choiceField(id)) {
            auto *choice = makeChoice(id, tr(text.hint));
            choices_.insert(id, choice);
            auto *label = new QLabel(text.key);
            label->setToolTip(tr(text.hint));
            label->setBuddy(choice);
            amnezia->addRow(label, choice);
            continue;
        }
        auto *edit = makeEdit(id);
        edit->setToolTip(tr(text.hint));
        edit->setPlaceholderText(tr(text.hint));
        edits_.insert(id, edit);
        auto *label = new QLabel(text.key);
        label->setToolTip(tr(text.hint));
        label->setBuddy(edit);
        amnezia->addRow(label, fieldRow(edit));
    }
    layout->addWidget(amneziaBody_);
    amneziaBody_->hide();
    connect(amneziaToggle_, &QToolButton::toggled, this, [this](bool expanded) { setAmneziaExpanded(expanded); });

    status_ = new QLabel;
    status_->setObjectName("wireGuardStatus");
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    status_->hide();
    layout->addWidget(status_);
    layout->addStretch();
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it)
        connect(it.value(), &QLineEdit::textChanged, this, [this, id = it.key()] {
            changed_.insert(id);
            refreshAmneziaSummary();
            emit edited();
        });
    for (auto it = choices_.cbegin(); it != choices_.cend(); ++it)
        connect(it.value(), &QComboBox::currentIndexChanged, this, [this, id = it.key()] {
            changed_.insert(id);
            refreshAmneziaSummary();
            emit edited();
        });
    refreshAmneziaSummary();
}

void WireGuardForm::setValues(const QJsonObject &fields) {
    changed_.clear();
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it) {
        const QSignalBlocker blocker(it.value());
        it.value()->setText(fields.value(it.key()).toString());
    }
    // Any spelling Core accepts maps to On or Off; a value Core rejects shows Not set
    // (its problem marks the field, and the raw tab keeps the original text).
    for (auto it = choices_.cbegin(); it != choices_.cend(); ++it) {
        const QString value = fields.value(it.key()).toString().trimmed().toLower();
        const QSignalBlocker blocker(it.value());
        it.value()->setCurrentIndex(QStringList{"true", "on", "1"}.contains(value) ? 1
            : QStringList{"false", "off", "0"}.contains(value) ? 2 : 0);
    }
    refreshAmneziaSummary();
}

void WireGuardForm::presetValue(const QString &id, const QString &value) {
    QLineEdit *field = edits_.value(id);
    if (!field) return;
    {
        const QSignalBlocker blocker(field);
        field->setText(value);
    }
    changed_.insert(id);
    refreshAmneziaSummary();
}

QMap<QString, QString> WireGuardForm::values() const {
    QMap<QString, QString> result;
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it) result.insert(it.key(), it.value()->text());
    for (auto it = choices_.cbegin(); it != choices_.cend(); ++it) result.insert(it.key(), it.value()->currentData().toString());
    return result;
}

void WireGuardForm::setStatus(const QString &text, bool problem) {
    status_->setText(text);
    status_->setVisible(!text.isEmpty());
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    status_->setStyleSheet(problem ? QStringLiteral("color: %1").arg(dark ? "#f3cf72" : "#b54708") : QString());
}

void WireGuardForm::setAmneziaExpanded(bool expanded) {
    const QSignalBlocker blocker(amneziaToggle_);
    amneziaToggle_->setChecked(expanded);
    amneziaToggle_->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    amneziaBody_->setVisible(expanded);
}

bool WireGuardForm::amneziaExpanded() const { return amneziaBody_->isVisibleTo(this); }

void WireGuardForm::refreshAmneziaSummary() {
    int set = 0;
    for (const QString &id : amneziaFields())
        set += choices_.contains(id) ? (choices_.value(id)->currentIndex() > 0 ? 1 : 0) : (edits_.value(id)->text().isEmpty() ? 0 : 1);
    amneziaToggle_->setText(set ? tr("AmneziaWG (%1 set)").arg(set) : tr("AmneziaWG"));
}
