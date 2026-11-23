#include "WireGuardForm.h"
#include "WireGuardCore.h"
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QTimer>
#include "FormRowVisibility.h"
#include "HiDpiComboBox.h"

#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
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

// version is the selector entry that introduces the key; 3.0 keys appear with 3.1.
struct AmneziaText {
    const char *version;
    const char *key;
    const char *label;
    const char *hint;
};

const QList<AmneziaText> &amneziaTexts() {
    static const QList<AmneziaText> texts{
        {"1.0", "Jc", QT_TRANSLATE_NOOP("WireGuardForm", "Junk packet count (Jc)"), QT_TRANSLATE_NOOP("WireGuardForm", "How many junk packets are sent before the handshake.")},
        {"1.0", "Jmin", QT_TRANSLATE_NOOP("WireGuardForm", "Min junk packet size (Jmin)"), QT_TRANSLATE_NOOP("WireGuardForm", "Smallest junk packet size in bytes.")},
        {"1.0", "Jmax", QT_TRANSLATE_NOOP("WireGuardForm", "Max junk packet size (Jmax)"), QT_TRANSLATE_NOOP("WireGuardForm", "Largest junk packet size in bytes.")},
        {"1.0", "S1", QT_TRANSLATE_NOOP("WireGuardForm", "Init packet extra bytes (S1)"), QT_TRANSLATE_NOOP("WireGuardForm", "Random bytes added to the handshake initiation.")},
        {"1.0", "S2", QT_TRANSLATE_NOOP("WireGuardForm", "Response packet extra bytes (S2)"), QT_TRANSLATE_NOOP("WireGuardForm", "Random bytes added to the handshake response.")},
        {"1.0", "H1", QT_TRANSLATE_NOOP("WireGuardForm", "Init packet header (H1)"), QT_TRANSLATE_NOOP("WireGuardForm", "Message type of the handshake initiation: a value, or a range lo-hi since 2.0.")},
        {"1.0", "H2", QT_TRANSLATE_NOOP("WireGuardForm", "Response packet header (H2)"), QT_TRANSLATE_NOOP("WireGuardForm", "Message type of the handshake response: a value, or a range lo-hi since 2.0.")},
        {"1.0", "H3", QT_TRANSLATE_NOOP("WireGuardForm", "Cookie packet header (H3)"), QT_TRANSLATE_NOOP("WireGuardForm", "Message type of the cookie reply: a value, or a range lo-hi since 2.0.")},
        {"1.0", "H4", QT_TRANSLATE_NOOP("WireGuardForm", "Data packet header (H4)"), QT_TRANSLATE_NOOP("WireGuardForm", "Message type of data packets: a value, or a range lo-hi since 2.0.")},
        {"1.5", "I1", QT_TRANSLATE_NOOP("WireGuardForm", "Signature packet 1 (I1)"), QT_TRANSLATE_NOOP("WireGuardForm", "Packet sent before the handshake, described with tags.")},
        {"1.5", "I2", QT_TRANSLATE_NOOP("WireGuardForm", "Signature packet 2 (I2)"), QT_TRANSLATE_NOOP("WireGuardForm", "Packet sent before the handshake, described with tags.")},
        {"1.5", "I3", QT_TRANSLATE_NOOP("WireGuardForm", "Signature packet 3 (I3)"), QT_TRANSLATE_NOOP("WireGuardForm", "Packet sent before the handshake, described with tags.")},
        {"1.5", "I4", QT_TRANSLATE_NOOP("WireGuardForm", "Signature packet 4 (I4)"), QT_TRANSLATE_NOOP("WireGuardForm", "Packet sent before the handshake, described with tags.")},
        {"1.5", "I5", QT_TRANSLATE_NOOP("WireGuardForm", "Signature packet 5 (I5)"), QT_TRANSLATE_NOOP("WireGuardForm", "Packet sent before the handshake, described with tags.")},
        {"2.0", "S3", QT_TRANSLATE_NOOP("WireGuardForm", "Cookie packet extra bytes (S3)"), QT_TRANSLATE_NOOP("WireGuardForm", "Random bytes added to the cookie reply.")},
        {"2.0", "S4", QT_TRANSLATE_NOOP("WireGuardForm", "Data packet extra bytes (S4)"), QT_TRANSLATE_NOOP("WireGuardForm", "Random bytes added to each data packet.")},
        {"3.1", "HeaderProtectionKey", QT_TRANSLATE_NOOP("WireGuardForm", "Header protection key (HeaderProtectionKey)"), QT_TRANSLATE_NOOP("WireGuardForm", "Key that hides packet headers; it must match the server.")},
        {"3.1", "ContentPaddingAddition", QT_TRANSLATE_NOOP("WireGuardForm", "Extra content padding (ContentPaddingAddition)"), QT_TRANSLATE_NOOP("WireGuardForm", "Extra padding added to packet contents.")},
        {"3.1", "RekeyAfterTime", QT_TRANSLATE_NOOP("WireGuardForm", "Rekey after, s (RekeyAfterTime)"), QT_TRANSLATE_NOOP("WireGuardForm", "Seconds before the session keys are renewed.")},
        {"3.1", "RekeyTimeout", QT_TRANSLATE_NOOP("WireGuardForm", "Rekey timeout, s (RekeyTimeout)"), QT_TRANSLATE_NOOP("WireGuardForm", "Seconds to wait before repeating a handshake.")},
        {"3.1", "RejectAfterTime", QT_TRANSLATE_NOOP("WireGuardForm", "Reject after, s (RejectAfterTime)"), QT_TRANSLATE_NOOP("WireGuardForm", "Seconds after which old keys are rejected.")},
        {"3.1", "KeepaliveTimeout", QT_TRANSLATE_NOOP("WireGuardForm", "Keepalive timeout, s (KeepaliveTimeout)"), QT_TRANSLATE_NOOP("WireGuardForm", "Seconds without traffic before a keepalive is sent.")},
        {"3.1", "MaxHandshakeAttempts", QT_TRANSLATE_NOOP("WireGuardForm", "Max handshake attempts (MaxHandshakeAttempts)"), QT_TRANSLATE_NOOP("WireGuardForm", "How many handshakes are tried before giving up.")},
        {"3.1", "RandomTrailers", QT_TRANSLATE_NOOP("WireGuardForm", "Random packet trailers (RandomTrailers)"), QT_TRANSLATE_NOOP("WireGuardForm", "Adds random bytes to the end of packets.")},
        {"3.1", "DisableCookies", QT_TRANSLATE_NOOP("WireGuardForm", "Disable cookies (DisableCookies)"), QT_TRANSLATE_NOOP("WireGuardForm", "Turns off cookie replies under load.")},
    };
    return texts;
}

// Selector entries; index 0 is plain WireGuard.
const QStringList &versions() {
    static const QStringList list{QString(), "1.0", "1.5", "2.0", "3.1"};
    return list;
}

QString tr(const char *text) { return QCoreApplication::translate("WireGuardForm", text); }
// Short words such as "On" have other translations elsewhere; the disambiguation
// selects the AmneziaWG option wording.
QString choiceText(const char *text) { return QCoreApplication::translate("WireGuardForm", text, "AmneziaWG option"); }

QLineEdit *makeEdit(const QString &id) {
    auto *edit = new QLineEdit;
    edit->setObjectName("wireGuard" + QString(id).replace('/', '_'));
    // The default 32767-character limit would truncate long lists on load.
    edit->setMaxLength(64 * 1024);
    if (WireGuardForm::secretField(id)) edit->setEchoMode(QLineEdit::Password);
    return edit;
}

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

// Secret fields are masked; Show reveals the value in this editor only.
QWidget *secretRow(QLineEdit *edit) {
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

// A field and, under it, the Core problem of that field.
QWidget *problemRow(QWidget *field, QLabel **problem) {
    auto *row = new QWidget;
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(field);
    *problem = new QLabel;
    (*problem)->setObjectName(field->objectName().isEmpty() ? QStringLiteral("wireGuardFieldProblem") : field->objectName() + "Problem");
    (*problem)->setWordWrap(true);
    (*problem)->setTextFormat(Qt::PlainText);
    (*problem)->hide();
    layout->addWidget(*problem);
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

QString WireGuardForm::fieldVersion(const QString &id) {
    for (const auto &text : amneziaTexts())
        if (id == QStringLiteral("Interface/") + text.key) return text.version;
    return {};
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

WireGuardForm::WireGuardForm(QFormLayout *networkForm, const QString &coreExecutable, const QJsonArray &keyOperations, QWidget *parent)
    : QWidget(parent), coreExecutable_(coreExecutable), generatePair_(keyOperations.contains("generate_keypair")), networkForm_(networkForm) {
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
        QLabel *problem = nullptr;
        QWidget *row = problemRow(secretRow(edit), &problem);
        problem->setObjectName(edit->objectName() + "Problem");
        rows_.insert(text.id, row);
        problems_.insert(text.id, problem);
        const QString id = text.id;
        if (id == "Interface/MTU" || id == "Interface/DNS") {
            edit->setPlaceholderText(tr("Automatic"));
            networkForm_->addRow(id == "Interface/MTU" ? tr("MTU") : tr("DNS"), row);
        } else {
            main->addRow(tr(text.label), row);
        }
        if (id == "Interface/PrivateKey" && keyOperations.contains("public_key")) {
            auto *keyRow = new QWidget;
            auto *keyLayout = new QHBoxLayout(keyRow);
            keyLayout->setContentsMargins(0, 0, 0, 0);
            publicKey_ = new QLineEdit;
            publicKey_->setObjectName("wireGuardClientPublicKey");
            publicKey_->setReadOnly(true);
            keyLayout->addWidget(publicKey_);
            auto *copy = new QPushButton(tr("Copy"));
            copy->setAutoDefault(false);
            copy->setEnabled(false);
            keyLayout->addWidget(copy);
            connect(publicKey_, &QLineEdit::textChanged, copy, [copy](const QString &key) { copy->setEnabled(!key.isEmpty()); });
            connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(publicKey_->text()); });
            main->addRow(tr("Client public key"), keyRow);
            auto *timer = new QTimer(this);
            timer->setSingleShot(true);
            timer->setInterval(400);
            connect(timer, &QTimer::timeout, this, &WireGuardForm::refreshPublicKey);
            connect(edit, &QLineEdit::textChanged, this, [this, timer] {
                ++keyRevision_;
                publicKey_->clear();
                timer->start();
            });
        }
        if (id == "Peer/PresharedKey" && keyOperations.contains("generate_preshared_key")) {
            auto *generate = new QPushButton(tr("Generate preshared key"));
            generate->setAutoDefault(false);
            generate->setObjectName("wireGuardGeneratePsk");
            row->layout()->addWidget(generate);
            connect(generate, &QPushButton::clicked, this, [this] { generateKey(false); });
        }
    }
    layout->addWidget(mainFields);

    amneziaToggle_ = new QToolButton;
    amneziaToggle_->setObjectName("amneziaToggle");
    amneziaToggle_->setCheckable(true);
    amneziaToggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    amneziaToggle_->setFocusPolicy(Qt::StrongFocus);
    amneziaToggle_->setIconSize(QSize(16, 16));
    amneziaToggle_->setAutoRaise(true);
    layout->addWidget(amneziaToggle_, 0, Qt::AlignLeft);
    amneziaBody_ = new QWidget;
    amneziaBody_->setObjectName("amneziaParameters");
    amneziaForm_ = new QFormLayout(amneziaBody_);
    amneziaForm_->setContentsMargins(16, 0, 0, 0);
    auto *intro = new QLabel(tr("Obfuscation parameters of AmneziaWG. Choose the version the server uses; the parameters must match the server."));
    intro->setObjectName("sectionHint");
    intro->setWordWrap(true);
    amneziaForm_->addRow(intro);
    version_ = new HiDpiComboBox;
    version_->setObjectName("amneziaVersion");
    version_->addItem(tr("None (plain WireGuard)"), QString());
    for (int i = 1; i < versions().size(); ++i) version_->addItem(versions().at(i), versions().at(i));
    version_->setToolTip(tr("Shows the parameters of this version and earlier ones. Choosing a version alone changes nothing in the configuration."));
    amneziaForm_->addRow(tr("AmneziaWG version"), version_);
    QString version;
    for (const auto &text : amneziaTexts()) {
        if (version != text.version) {
            version = text.version;
            auto *heading = new QLabel(version == "3.1" ? tr("AmneziaWG 3.x") : tr("AmneziaWG %1").arg(version));
            heading->setObjectName("sectionTitle");
            headings_.insert(version, heading);
            amneziaForm_->addRow(heading);
        }
        const QString id = QStringLiteral("Interface/") + text.key;
        QWidget *field = nullptr;
        if (choiceField(id)) {
            auto *choice = makeChoice(id, tr(text.hint));
            choices_.insert(id, choice);
            field = choice;
        } else {
            auto *edit = makeEdit(id);
            edit->setToolTip(tr(text.hint));
            edits_.insert(id, edit);
            field = edit;
        }
        QLabel *problem = nullptr;
        QWidget *row = problemRow(choiceField(id) ? field : secretRow(static_cast<QLineEdit *>(field)), &problem);
        problem->setObjectName(field->objectName() + "Problem");
        rows_.insert(id, row);
        problems_.insert(id, problem);
        auto *label = new QLabel(tr(text.label));
        label->setToolTip(tr(text.hint));
        label->setBuddy(field);
        amneziaForm_->addRow(label, row);
    }
    layout->addWidget(amneziaBody_);
    setAmneziaExpanded(false);
    connect(amneziaToggle_, &QToolButton::toggled, this, [this](bool expanded) { setAmneziaExpanded(expanded); });
    connect(version_, &QComboBox::currentIndexChanged, this, [this](int index) { changeVersion(index); });

    status_ = new QLabel;
    status_->setObjectName("wireGuardStatus");
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    status_->hide();
    layout->addWidget(status_);
    layout->addStretch();
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it)
        connect(it.value(), &QLineEdit::textChanged, this, [this, id = it.key()] {
            if (id == "Interface/PrivateKey") pairRequest_ = nullptr;
            if (id == "Peer/PresharedKey") pskRequest_ = nullptr;
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
    refreshVersionRows();
    refreshAmneziaSummary();
}

void WireGuardForm::showNetworkFields(bool visible, bool editable) {
    for (const QString &id : {QStringLiteral("Interface/MTU"), QStringLiteral("Interface/DNS")}) {
        QWidget *row = rows_.value(id);
        setFormRowVisible(networkForm_, row, visible);
        row->setEnabled(editable);
    }
}

void WireGuardForm::initializeNewKey() {
    if (!generatePair_ || !edit("Interface/PrivateKey")->text().isEmpty()) return;
    generatePair_ = false;
    generateKey(true);
}

void WireGuardForm::invalidateKeyGeneration() {
    // An abandoned request may be retried on returning to a still-empty new draft.
    if (pairRequest_) generatePair_ = true;
    pairRequest_ = nullptr;
    pskRequest_ = nullptr;
}

bool WireGuardForm::keyGenerationPending() const {
    return pairRequest_ || pskRequest_;
}

void WireGuardForm::generateKey(bool pair) {
    auto &pending = pair ? pairRequest_ : pskRequest_;
    if (pending) return;
    auto *watcher = new QFutureWatcher<QJsonObject>(this);
    pending = watcher;
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, pair, watcher] {
        watcher->deleteLater();
        auto &current = pair ? pairRequest_ : pskRequest_;
        if (current != watcher) return;
        current = nullptr;
        const auto result = watcher->result();
        if (result.isEmpty()) {
            setStatus(pair ? tr("Core could not generate the client key pair.") : tr("Core could not generate the preshared key."), true);
            return;
        }
        if (pair) {
            presetValue("Interface/PrivateKey", result.value("private_key").toString());
            if (publicKey_) publicKey_->setText(result.value("public_key").toString());
        } else {
            edit("Peer/PresharedKey")->setText(result.value("preshared_key").toString());
        }
        emit keyGenerated();
    });
    const QString op = pair ? QStringLiteral("generate_keypair") : QStringLiteral("generate_preshared_key");
    watcher->setFuture(WireGuardCore::keyOperation(coreExecutable_, {{"version", 1}, {"op", op}}));
}

void WireGuardForm::refreshPublicKey() {
    if (!publicKey_) return;
    const auto revision = ++keyRevision_;
    publicKey_->clear();
    const QString key = edit("Interface/PrivateKey")->text();
    if (key.isEmpty()) {
        derivationPending_ = false;
        return;
    }
    if (derivation_) {
        derivationPending_ = true;
        return;
    }
    derivation_ = new QFutureWatcher<QJsonObject>(this);
    connect(derivation_, &QFutureWatcher<QJsonObject>::finished, this, [this, revision] {
        if (revision == keyRevision_) publicKey_->setText(derivation_->result().value("public_key").toString());
        derivation_->deleteLater();
        derivation_ = nullptr;
        if (derivationPending_) {
            derivationPending_ = false;
            refreshPublicKey();
        }
    });
    derivation_->setFuture(WireGuardCore::keyOperation(coreExecutable_, {{"version", 1}, {"op", "public_key"}, {"private_key", key}}));
}

void WireGuardForm::setValues(const QJsonObject &fields) {
    invalidateKeyGeneration();
    changed_.clear();
    loadedChoices_.clear();
    for (auto it = edits_.cbegin(); it != edits_.cend(); ++it) {
        const QSignalBlocker blocker(it.value());
        it.value()->setText(fields.value(it.key()).toString());
    }
    // Any spelling Core accepts maps to On or Off; a value Core rejects shows Not set
    // (its problem marks the field, and the raw tab keeps the original text).
    for (auto it = choices_.cbegin(); it != choices_.cend(); ++it) {
        const QString raw = fields.value(it.key()).toString();
        const QString value = raw.trimmed().toLower();
        loadedChoices_.insert(it.key(), raw);
        const QSignalBlocker blocker(it.value());
        it.value()->setCurrentIndex(QStringList{"true", "on", "1"}.contains(value) ? 1
            : QStringList{"false", "off", "0"}.contains(value) ? 2 : 0);
    }
    clearFieldProblems();
    refreshPublicKey();
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

void WireGuardForm::setFieldProblem(const QString &id, const QString &message) {
    QLabel *problem = problems_.value(id);
    if (!problem) return;
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    problem->setStyleSheet(QStringLiteral("color: %1").arg(dark ? "#ff9a92" : "#c62828"));
    problem->setText(message);
    problem->setVisible(!message.isEmpty());
}

void WireGuardForm::clearFieldProblems() {
    for (QLabel *problem : std::as_const(problems_)) {
        problem->clear();
        problem->hide();
    }
}

void WireGuardForm::setAmneziaExpanded(bool expanded) {
    const QSignalBlocker blocker(amneziaToggle_);
    amneziaToggle_->setChecked(expanded);
    const qreal scale = amneziaToggle_->devicePixelRatioF();
    QPixmap icon(qRound(16 * scale), qRound(16 * scale));
    icon.setDevicePixelRatio(scale);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(amneziaToggle_->palette().buttonText().color(), 1.6, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.translate(8, 8);
    if (expanded) painter.rotate(90);
    painter.drawPolyline(QPolygonF{{-2, -4}, {2, 0}, {-2, 4}});
    painter.end();
    amneziaToggle_->setIcon(QIcon(icon));
    amneziaBody_->setVisible(expanded);
}

bool WireGuardForm::amneziaExpanded() const { return amneziaBody_->isVisibleTo(this); }

void WireGuardForm::setAmneziaVersion(const QString &coreVersion) {
    // Core reports the minimum version the keys imply; 3.0 keys are shown with 3.1.
    int index = versions().indexOf(coreVersion == "3.0" ? QStringLiteral("3.1") : coreVersion);
    if (index < 0) index = coreVersion.isEmpty() ? 0 : int(versions().size()) - 1;
    {
        const QSignalBlocker blocker(version_);
        version_->setCurrentIndex(index);
    }
    versionIndex_ = index;
    refreshVersionRows();
    refreshAmneziaSummary();
    emit versionChanged();
}

QString WireGuardForm::amneziaVersion() const { return versions().value(versionIndex_); }

bool WireGuardForm::fieldSet(const QString &id) const {
    if (choices_.contains(id)) return choices_.value(id)->currentIndex() > 0 || (!changed_.contains(id) && !loadedChoices_.value(id).isEmpty());
    return !edits_.value(id)->text().isEmpty();
}

void WireGuardForm::clearField(const QString &id) {
    if (QComboBox *choice = choices_.value(id)) choice->setCurrentIndex(0);
    else edits_.value(id)->clear();
    // A rejected boolean already shows Not set; record it so its key is removed too.
    changed_.insert(id);
}

void WireGuardForm::changeVersion(int index) {
    if (index < versionIndex_) {
        QStringList filled;
        for (const QString &id : amneziaFields())
            if (versions().indexOf(fieldVersion(id)) > index && fieldSet(id)) filled << id.mid(id.indexOf('/') + 1);
        if (!filled.isEmpty()) {
            QMessageBox box(QMessageBox::Question, tr("AmneziaWG version"), index == 0
                ? tr("Remove all AmneziaWG parameters? The profile becomes plain WireGuard.")
                : tr("Remove the parameters that AmneziaWG %1 does not use: %2?").arg(versions().at(index), filled.join(", ")),
                QMessageBox::Yes | QMessageBox::No, this);
            box.setObjectName("amneziaVersionChange");
            box.setDefaultButton(QMessageBox::No);
            if (box.exec() != QMessageBox::Yes) {
                const QSignalBlocker blocker(version_);
                version_->setCurrentIndex(versionIndex_);
                return;
            }
            for (const QString &id : amneziaFields())
                if (versions().indexOf(fieldVersion(id)) > index && fieldSet(id)) clearField(id);
            emit edited();
        }
    }
    versionIndex_ = index;
    refreshVersionRows();
    refreshAmneziaSummary();
    emit versionChanged();
}

void WireGuardForm::refreshVersionRows() {
    for (auto it = rows_.cbegin(); it != rows_.cend(); ++it) {
        const QString version = fieldVersion(it.key());
        if (!version.isEmpty()) setFormRowVisible(amneziaForm_, it.value(), versions().indexOf(version) <= versionIndex_);
    }
    for (auto it = headings_.cbegin(); it != headings_.cend(); ++it)
        setFormRowVisible(amneziaForm_, it.value(), versions().indexOf(it.key()) <= versionIndex_);
}

void WireGuardForm::refreshAmneziaSummary() {
    int set = 0;
    for (const QString &id : amneziaFields()) set += fieldSet(id) ? 1 : 0;
    const QString version = amneziaVersion();
    amneziaToggle_->setText(version.isEmpty() ? tr("AmneziaWG: not used")
        : set ? tr("AmneziaWG %1 (%2 set)").arg(version).arg(set) : tr("AmneziaWG %1").arg(version));
}
