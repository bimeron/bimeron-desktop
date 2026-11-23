#include "ProfileEditorDialog.h"
#include "ProfileTextEdit.h"
#include "RoutingCompatibility.h"
#include "HiDpiCheckBox.h"
#include "FormRowVisibility.h"
#include "HiDpiComboBox.h"
#include "ProfileStore.h"
#include "GeoDataSelection.h"
#include "GeoDataDialog.h"
#include "GeoIPLabels.h"
#include "GeoSitePicker.h"
#include "CoreDiagnostic.h"
#include "CoreProblems.h"
#include "DnsAddressEdit.h"
#include "WireGuardCore.h"
#include "WireGuardForm.h"
#include <QToolButton>
#include <QTextBlock>
#include <algorithm>
#include <QApplication>
#include <memory>
#include <tuple>
#include <vector>
#include <QJsonArray>

#include <QCheckBox>
#include <QAction>
#include <QStyle>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QDateTime>
#include <QJsonDocument>
#include <QProcess>
#include <QTimer>
#include <QDir>
#include <QListWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSet>
#include <QScrollArea>
#include <QScreen>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QSpinBox>
#include <QSyntaxHighlighter>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextCharFormat>
#include <QToolButton>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

class AllowedIpsSummary : public QWidget {
public:
    AllowedIpsSummary() {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        label_ = new QLabel;
        label_->setObjectName("allowedIpsList");
        label_->setTextFormat(Qt::PlainText);
        label_->setWordWrap(true);
        label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(label_, 1);
        button_ = new QPushButton(ProfileEditorDialog::tr("Show all"));
        layout->addWidget(button_);
        connect(button_, &QPushButton::clicked, this, [this] {
            QDialog dialog(this);
            dialog.setWindowTitle(QStringLiteral("AllowedIPs"));
            auto *layout = new QVBoxLayout(&dialog);
            auto *list = new QPlainTextEdit;
            list->setReadOnly(true);
            list->setLineWrapMode(QPlainTextEdit::NoWrap);
            list->setPlainText(networks_.join('\n'));
            layout->addWidget(list);
            auto *close = new QPushButton(ProfileEditorDialog::tr("Close"));
            layout->addWidget(close, 0, Qt::AlignRight);
            connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
            dialog.resize(QSize(520, 420).boundedTo(screen()->availableGeometry().size() * 0.8));
            dialog.exec();
            button_->setFocus(Qt::OtherFocusReason);
        });
    }

    void setNetworks(const QString &value) {
        networks_ = value.split(',', Qt::SkipEmptyParts);
        for (QString &network : networks_) network = network.trimmed();
        networks_.removeAll(QString());
        full_ = ProfileEditorDialog::tr("AllowedIPs: %1").arg(networks_.isEmpty()
            ? ProfileEditorDialog::tr("not set") : networks_.join(", "));
        refresh();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        refresh();
    }
    void changeEvent(QEvent *event) override {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::FontChange) refresh();
    }

private:
    void refresh() {
        const QFontMetrics metrics(label_->font());
        const auto bounds = metrics.boundingRect(QRect(0, 0, width(), 0), Qt::TextWordWrap, full_);
        const bool compact = bounds.height() > metrics.height() + metrics.lineSpacing() || bounds.width() > width();
        button_->setVisible(compact);
        label_->setText(compact ? ProfileEditorDialog::tr("AllowedIPs networks: %1").arg(networks_.size()) : full_);
    }

    QLabel *label_;
    QPushButton *button_;
    QStringList networks_;
    QString full_;
};

namespace {
// Profile contents travel only through stdin/stdout, never process arguments or diagnostics.
QJsonObject bridgeJson(const QString &executable, const QStringList &args, const QByteArray &input, QString *error) {
    QProcess process;
    process.start(executable, args);
    if (!process.waitForStarted(2000)) { *error = QObject::tr("Core document bridge is unavailable. Use Raw config."); return {}; }
    process.write(input);
    process.closeWriteChannel();
    QByteArray output;
    QElapsedTimer elapsed; elapsed.start();
    while (process.state() != QProcess::NotRunning && elapsed.elapsed() < 5000) {
        process.waitForReadyRead(50);
        output += process.readAllStandardOutput();
        process.readAllStandardError();
        if (output.size() > 32 * 1024 * 1024) break;
    }
    if (process.state() != QProcess::NotRunning) {
        process.kill(); process.waitForFinished(1000);
        *error = QObject::tr("Core document bridge timed out or exceeded its response limit. The draft has been retained."); return {};
    }
    output += process.readAllStandardOutput();
    QJsonParseError parse;
    const auto json = QJsonDocument::fromJson(output, &parse);
    const auto result = json.object();
    if (output.size() > 32 * 1024 * 1024 || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
        || parse.error != QJsonParseError::NoError || !json.isObject() || result.value("version").toInt() != 1) {
        *error = QObject::tr("Core document bridge returned an invalid response. The draft has been retained."); return {};
    }
    if (!result.value("error").toString().isEmpty()) *error = QObject::tr("Core refused this document edit. Use Raw config; the draft has been retained.");
    return result;
}

QString groupLabel(const QString &id, bool geoIP = false) {
    if (geoIP) return geoIPLabel(id, true);
    const QString name = geoSiteName(id);
    return name.isEmpty() ? id : name + " (" + id + ')';
}

void showGroupSummary(QLineEdit *target, const QString &ids) {
    QStringList labels;
    for (const auto &id : ids.split(',', Qt::SkipEmptyParts)) labels.append(groupLabel(id.trimmed(), target->property("geoip").toBool()));
    const QSignalBlocker blocker(target);
    target->setProperty("groupIds", ids);
    target->setText(labels.join(", "));
    target->setAccessibleDescription(labels.join(", "));
}

QString resourceIdentity(const QString &path) {
    const QFileInfo info(path);
    return info.canonicalFilePath() + '/' + QString::number(info.size()) + '/' + QString::number(info.lastModified().toMSecsSinceEpoch());
}

QMap<QString, QString> visualFields(const QString &text) {
    QMap<QString, QString> fields;
    QString section;
    for (const auto &source : text.split('\n')) {
        const QString line = source;
        if (line.startsWith('[') && line.endsWith(']')) section = line.mid(1, line.size() - 2);
        else if (section != "Rule" && !line.startsWith('#') && !line.startsWith(';') && line.contains('=')) {
            QString value = line.mid(line.indexOf('=') + 1);
            // buildText adds exactly one separator space; the rest is user data.
            if (value.startsWith(' ')) value.remove(0, 1);
            QString decoded;
            if (value.startsWith('"') && ProfileTextEdit::decode(value, &decoded)) value = decoded;
            fields.insert(section + "/" + line.left(line.indexOf('=')).trimmed(), value);
        }
    }
    return fields;
}

bool hasDohFields(const QString &text) {
    QString section;
    for (const auto &line : ProfileTextEdit::lines(text)) {
        const auto header = ProfileTextEdit::header(line);
        if (!header.isEmpty()) section = header;
        const auto content = ProfileTextEdit::clean(line);
        const int equal = content.indexOf('=');
        if (section != "Interface" || equal < 0) continue;
        const auto key = content.left(equal).trimmed();
        if (key == "DoH" || key == "DoHBootstrap") return true;
    }
    return false;
}

class ConfigHighlighter final : public QSyntaxHighlighter {
public:
    explicit ConfigHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}
protected:
    void highlightBlock(const QString &text) override {
        QTextCharFormat format;
        if (text.trimmed().startsWith('#') || text.trimmed().startsWith(';')) {
            format.setForeground(QColor("#8794a6"));
            setFormat(0, text.size(), format);
            return;
        }
        if (QRegularExpression(R"(^\s*\[[^]]+\]\s*$)").match(text).hasMatch()) {
            format.setForeground(QColor("#1677ff"));
            format.setFontWeight(QFont::Bold);
            setFormat(0, text.size(), format);
            return;
        }
        const auto match = QRegularExpression(R"(^\s*([^=]+?)\s*=)").match(text);
        if (match.hasMatch()) {
            format.setForeground(QColor("#7c4dba"));
            setFormat(match.capturedStart(1), match.capturedLength(1), format);
            format.setForeground(QColor("#167246"));
            setFormat(match.capturedEnd(0), text.size() - match.capturedEnd(0), format);
        }
    }
};

QString valueFor(const QString &text, const QString &section, const QString &key) {
    QString current;
    for (const QString &line : text.split('\n')) {
        const QString trimmed = QString(line).remove(QChar(0xfeff)).trimmed();
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) current = ProfileStore::sectionName(trimmed);
        else if (current == section) {
            const auto match = QRegularExpression(QString("^%1\\s*=\\s*(.*)$").arg(QRegularExpression::escape(key))).match(trimmed);
            if (match.hasMatch()) {
                QString value;
                return ProfileTextEdit::decode(match.captured(1), &value) ? value : match.captured(1).trimmed();
            }
        }
    }
    return {};
}

enum class ProtectionIntent { Disabled, Enabled, Unknown };

ProtectionIntent nativeProtectionIntent(const QString &text) {
    const QString value = valueFor(text, "IKEv2", "KillSwitch");
    if (value.isEmpty() || value == "false" || value == "\"false\"") return ProtectionIntent::Disabled;
    if (value == "true" || value == "\"true\"") return ProtectionIntent::Enabled;
    // Core decodes Go escapes. Leave other spellings to authoritative validation;
    // never treat an unrecognized candidate as safely disabled or imported consent.
    return ProtectionIntent::Unknown;
}

QString normalizedCidrs(const QString &value) {
    QStringList entries = value.split(',');
    for (QString &entry : entries) {
        const QString address = entry.trimmed();
        QHostAddress parsed;
        if (!address.contains('/') && parsed.setAddress(address))
            entry = address + (parsed.protocol() == QAbstractSocket::IPv4Protocol ? "/32" : "/128");
        else
            entry = address;
    }
    return entries.join(", ");
}

QString localNetworkCidrs() {
    return "10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 169.254.0.0/16, fc00::/7, fe80::/10";
}

struct SsParameters {
    QString server;
    QString method;
    QString password;
    QString prefix;
    int port = 0;
    bool valid = false;
};

QString decodedBase64(QString value) {
    value.replace('-', '+');
    value.replace('_', '/');
    while (value.size() % 4) value += '=';
    return QString::fromUtf8(QByteArray::fromBase64(value.toUtf8()));
}

SsParameters parseSsUrl(const QString &text) {
    SsParameters result;
    const QUrl source(text.trimmed());
    if (source.scheme().compare("ss", Qt::CaseInsensitive) != 0) return result;
    result.prefix = QUrlQuery(source).queryItemValue("prefix");

    QString body = text.trimmed().mid(text.indexOf("://") + 3);
    const qsizetype suffix = body.indexOf(QRegularExpression("[?#]"));
    if (suffix >= 0) body.truncate(suffix);
    if (!body.contains('@')) body = decodedBase64(body);
    const qsizetype separator = body.lastIndexOf('@');
    if (separator <= 0) return result;

    QString credentials = body.left(separator);
    const QString decoded = decodedBase64(credentials);
    if (decoded.contains(':')) credentials = decoded;
    credentials = QUrl::fromPercentEncoding(credentials.toUtf8());
    const qsizetype colon = credentials.indexOf(':');
    if (colon <= 0) return result;

    const QUrl endpoint("ss://" + body.mid(separator + 1));
    result.server = endpoint.host();
    result.port = endpoint.port();
    result.method = credentials.left(colon);
    result.password = credentials.mid(colon + 1);
    result.valid = !result.server.isEmpty() && result.port > 0
        && !result.method.isEmpty() && !result.password.isEmpty();
    return result;
}

QCheckBox *ruleEnabled(QTableWidget *table, int row) {
    return table->cellWidget(row, 0)->findChild<QCheckBox *>();
}

// Explicit sources that require Core's explicit_access support, keyed to the
// capability name each one needs.
const QMap<QString, QString> &coreExplicitSources() {
    static const QMap<QString, QString> sources{{"Explicithysteria2", "hysteria2"}, {"Explicitvmess", "vmess"},
        {"Explicittuic", "tuic"}, {"Explicitanytls", "anytls"}, {"ExplicitnaiveQUIC", "naive_quic"}};
    return sources;
}

// Link fields that the editor keeps verbatim and never decodes.
const QStringList &linkSources() {
    static const QStringList sources{"VMess", "Trojan", "TUIC", "AnyTLS", "HTTP", "Naive", "MASQUE"};
    return sources;
}

// Sources whose protocol options are edited on the Protocol tab.
bool parameterSource(const QString &source) {
    return coreExplicitSources().contains(source)
        || QStringList{"ExplicitTrojan", "Explicithttp", "Explicithttps", "Explicitnaive", "Explicitmasque"}.contains(source);
}

bool usesUsername(const QString &source) {
    return !QStringList{"ExplicitTrojan", "Explicithysteria2", "Explicitanytls", "Explicittuic", "Explicitvmess"}.contains(source);
}

// The [AccessKey] Type value written for an explicit source.
QString explicitType(const QString &source) {
    if (source == "Explicit") return "shadowsocks";
    if (source == "ExplicitnaiveQUIC") return "naive";
    return source.mid(8).toLower();
}

// VMess editor fields hold transport values without the per-transport option
// names that Core uses, so the same field serves every transport.
QString vmessPathField(const QString &network) {
    return network == "grpc" ? "GRPCServiceName" : network == "xhttp" ? "XHTTPPath" : "WSPath";
}

QString vmessHostField(const QString &network) {
    return network == "grpc" ? "GRPCAuthority" : network == "xhttp" ? "XHTTPHost" : "WSHost";
}
}

ProfileEditorDialog::ProfileEditorDialog(const QString &text, const QJsonObject &capabilities, QWidget *parent,
                                         std::function<bool(const QString &, CoreFailure *)> validateRaw,
                                         const QString &coreExecutable, const QString &profilePath, bool create,
                                         const std::optional<QString> &wireGuardText)
    : QDialog(parent), coreExecutable_(coreExecutable), profilePath_(profilePath), capabilities_(capabilities),
      originalText_(text), validateRaw_(validateRaw) {
    creating_ = freshDraft_ = create;
    wireGuard_ = !create && wireGuardText.has_value();
    // New WireGuard drafts need both the running service and the selected CLI bridge.
    QJsonArray keyOperations;
    const bool bridge = (wireGuard_ || (create && WireGuardCore::serviceSupported(capabilities)))
        && WireGuardCore::cliSupported(coreExecutable, nullptr, &keyOperations);
    wireGuardCreatable_ = create && bridge;
    setWindowTitle(create ? tr("Create profile") : tr("Profile settings"));
    resize(680, wireGuard_ ? qBound(460, screen()->availableGeometry().height() - 80, 720) : 520);
    setMinimumSize(620, 460);
    auto *layout = new QVBoxLayout(this);
    tabs_ = new QTabWidget;
    tabs_->tabBar()->setFocusPolicy(Qt::NoFocus);
    tabs_->tabBar()->setUsesScrollButtons(false);

    auto *general = new QWidget;
    auto *generalForm = new QFormLayout(general);
    name_ = new QLineEdit;
    name_->setObjectName("profileName");
    keySource_ = new HiDpiComboBox(this);
    keySource_->setObjectName("keySource");
    if (!creating_) {
    keySource_->addItem(tr("ss:// key"), "SS");
    keySource_->addItem(tr("Dynamic ssconf://"), "SSConf");
    keySource_->addItem(tr("Remote socks5://"), "SOCKS5");
    keySource_->addItem(tr("vless:// key"), "VLESS");
    keySource_->addItem(tr("Hysteria2 key"), "Hysteria2");
    keySource_->addItem(tr("SSH link"), "SSH");
    if (!valueFor(text, "AccessKey", "Key").isEmpty())
        keySource_->addItem(tr("Legacy key"), "Key");
    if (wireGuard_)
        keySource_->addItem(tr("WireGuard / AmneziaWG configuration"), "WireGuard");
    else if (!valueFor(text, "AccessKey", "File").isEmpty())
        keySource_->addItem(tr("Key file"), "File");
    }
    keySource_->addItem(tr("Explicit Shadowsocks settings"), "Explicit");
    keySource_->addItem(tr("Explicit SOCKS5 settings"), "ExplicitSOCKS5");
    keySource_->addItem(tr("Explicit VLESS settings"), "ExplicitVLESS");
    keySource_->addItem(tr("Explicit SSH settings"), "ExplicitSSH");
    keySource_->addItem(tr("Explicit Trojan settings"), "ExplicitTrojan");
    keySource_->addItem(tr("Explicit HTTP settings"), "Explicithttp");
    keySource_->addItem(tr("Explicit HTTPS settings"), "Explicithttps");
    keySource_->addItem(tr("Explicit Naive H2 settings"), "Explicitnaive");
    keySource_->addItem(tr("Explicit MASQUE settings"), "Explicitmasque");
    if (creating_) {
        const QStringList labels{"Shadowsocks", "SOCKS5", "VLESS", "SSH", "Trojan", "HTTP", "HTTPS", "Naive HTTPS", "MASQUE"};
        for (int i = 0; i < labels.size(); ++i) keySource_->setItemText(i, labels[i]);
    }
    // Existing profiles always map to their source; new ones only when the
    // running Core accepts the protocol as explicit settings.
    const QJsonArray explicitAccess = capabilities.value("explicit_access").toArray();
    bool explicitMissing = false;
    for (const auto &[data, name, label] : std::initializer_list<std::tuple<const char *, const char *, QString>>{
             {"Explicithysteria2", "Hysteria2", tr("Explicit Hysteria2 settings")},
             {"Explicitvmess", "VMess", tr("Explicit VMess settings")},
             {"Explicittuic", "TUIC", tr("Explicit TUIC settings")},
             {"Explicitanytls", "AnyTLS", tr("Explicit AnyTLS settings")},
             {"ExplicitnaiveQUIC", "Naive QUIC", tr("Explicit Naive QUIC settings")}}) {
        if (creating_ && !explicitAccess.contains(QJsonValue(coreExplicitSources().value(data)))) {
            explicitMissing = true;
            continue;
        }
        keySource_->addItem(creating_ ? QString(name) : label, QString(data));
    }
    if (wireGuardCreatable_) keySource_->addItem(QStringLiteral("WireGuard / AmneziaWG"), "WireGuard");
    key_ = new QLineEdit;
    auto *keyRow = new QWidget;
    auto *keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(key_);
    auto *browse = new QPushButton(tr("Browse"));
    auto *reveal = new QPushButton(tr("Show"));
    keyLayout->addWidget(reveal);
    keyLayout->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Key file"));
        if (!path.isEmpty()) { keySource_->setCurrentIndex(keySource_->findData("File")); key_->setText(path); }
    });
    server_ = new QLineEdit;
    server_->setObjectName("server");
    port_ = new QSpinBox;
    port_->setObjectName("port");
    port_->setRange(0, 65535);
    port_->setSpecialValueText(QStringLiteral("—"));
    method_ = new HiDpiComboBox;
    method_->setObjectName("method");
    method_->addItems({"chacha20-ietf-poly1305", "aes-128-gcm", "aes-192-gcm", "aes-256-gcm", "2022-blake3-aes-128-gcm", "2022-blake3-aes-256-gcm", "2022-blake3-chacha20-poly1305"});
    password_ = new QLineEdit;
    password_->setEchoMode(QLineEdit::Password);
    password_->setObjectName("password");
    username_ = new QLineEdit;
    username_->setObjectName("username");
    sshIdentity_ = new QLineEdit;
    sshIdentity_->setObjectName("sshIdentity");
    sshKnownHosts_ = new QLineEdit;
    auto *identityRow = new QWidget;
    auto *identityLayout = new QHBoxLayout(identityRow);
    identityLayout->setContentsMargins(0, 0, 0, 0);
    identityLayout->addWidget(sshIdentity_);
    auto *chooseIdentity = new QPushButton(tr("Browse"));
    identityLayout->addWidget(chooseIdentity);
    connect(chooseIdentity, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("SSH identity file"));
        if (!path.isEmpty()) sshIdentity_->setText(QFileInfo(path).absoluteFilePath());
    });
    auto *hostsRow = new QWidget;
    auto *hostsLayout = new QHBoxLayout(hostsRow);
    hostsLayout->setContentsMargins(0, 0, 0, 0);
    hostsLayout->addWidget(sshKnownHosts_);
    auto *chooseHosts = new QPushButton(tr("Browse"));
    hostsLayout->addWidget(chooseHosts);
    connect(chooseHosts, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("SSH known_hosts file"));
        if (!path.isEmpty()) sshKnownHosts_->setText(QFileInfo(path).absoluteFilePath());
    });

    username_->setEchoMode(QLineEdit::Password);
    ssPlugin_ = new HiDpiComboBox;
    ssPlugin_->setObjectName("ssPlugin");
    ssPlugin_->addItems({"", "v2ray-plugin", "obfs-local"});
    ssPluginOpts_ = new QLineEdit;
    ssPluginOpts_->setObjectName("ssPluginOpts");
    ssPluginOpts_->setEchoMode(QLineEdit::Password);
    ssPluginOpts_->setToolTip(tr("SIP003 options are preserved literally. tls=false enables WSS; obfs-local TLS is camouflage, not verified TLS. Plugins carry TCP only."));
    prefix_ = new QLineEdit;
    prefix_->setObjectName("prefix");
    mode_ = new HiDpiComboBox;
    if (creating_ || capabilities.value("vpn").toBool()) mode_->addItem(tr("System VPN"), "vpn");
    if (creating_ || capabilities.value("proxy").toBool()) mode_->addItem(tr("Local SOCKS5"), "proxy");
    listen_ = new QLineEdit;
    windowsProfile_ = new QLineEdit;
    windowsProfile_->setObjectName("windowsProfile");
    nativeServerName_ = new QLineEdit;
    nativeServerName_->setObjectName("nativeServerName");
    nativeKillSwitch_ = new HiDpiCheckBox(tr("Request native traffic and DNS protection"));
    nativeKillSwitch_->setObjectName("nativeKillSwitch");
    generalForm->addRow(tr("Windows VPN profile"), windowsProfile_);
    generalForm->addRow(tr("Server name"), nativeServerName_);
    generalForm->addRow(nativeKillSwitch_);
    nativeMode_ = new QLabel(tr("System VPN (vpn)"));
    generalForm->addRow(tr("Mode"), nativeMode_);
    auto *sourceNotice = new QLabel(tr("Changing the key source requires Raw config. Cleared optional fields are removed when saving."));
    sourceNotice->setWordWrap(true);
    sourceNotice->setObjectName("sourceEditRestriction");
    sourceNotice->setVisible(!creating_ && !coreExecutable_.isEmpty());
    generalForm->addRow(sourceNotice);
    generalForm->addRow(tr("Name"), name_);
    auto *auth = new QWidget;
    nativeAuthentication_ = auth;
    auth->setObjectName("nativeAuthentication");
    auto *authForm = new QFormLayout(auth);
    authForm->setContentsMargins(0, 0, 0, 0);
    auto *authTitle = new QLabel(tr("Authentication"));
    authTitle->setObjectName("sectionTitle");
    authForm->addRow(authTitle);
    nativeAuthMethod_ = new HiDpiComboBox;
    nativeAuthMethod_->setObjectName("nativeAuthMethod");
    nativeAuthMethod_->addItems({tr("Username and password"), tr("Client certificate"), tr("Smart card")});
    authForm->addRow(tr("Method"), nativeAuthMethod_);
    nativeAuthSource_ = new HiDpiComboBox;
    nativeAuthSource_->setObjectName("nativeAuthSource");
    nativeAuthSource_->addItems({tr("Use saved Windows credentials"), tr("Enter credentials")});
    authForm->addRow(tr("Credentials"), nativeAuthSource_);
    auto *authName = new QLineEdit;
    authName->setObjectName("nativeAuthUsername");
    auto *authPassword = new QLineEdit;
    authPassword->setObjectName("nativeAuthPassword");
    authPassword->setEchoMode(QLineEdit::Password);
    authForm->addRow(tr("Username"), authName);
    authForm->addRow(tr("Password"), authPassword);
    auto *ask = new HiDpiCheckBox(tr("Ask at connection"));
    ask->setChecked(true);
    auto *remember = new HiDpiCheckBox(tr("Remember in Windows"));
    authForm->addRow(ask);
    authForm->addRow(remember);
    auto *identity = new QLabel(tr("No identity selected"));
    authForm->addRow(identity);
    auto *choose = new QPushButton(tr("Choose"));
    authForm->addRow(choose);
    auto *notice = new QLabel;
    notice->setWordWrap(true);
    notice->setTextFormat(Qt::PlainText);
    authForm->addRow(notice);
    choose->setEnabled(false);
    ask->setObjectName("nativeAuthAsk");
    remember->setObjectName("nativeAuthRemember");
    connect(ask, &QCheckBox::toggled, this, [authPassword](bool checked) {
        authPassword->clear();
        authPassword->setEnabled(!checked);
    });
    generalForm->addRow(nativeAuthentication_);
    refreshNativeAuthentication_ = [this, authForm, authName, authPassword, ask, remember, identity, choose, notice] {
        const bool password = nativeAuthMethod_->currentIndex() == 0;
        const bool enter = password && nativeAuthSource_->currentIndex() == 1;
        nativeAuthPreview_ = !password || enter;
        authName->clear(); authPassword->clear();
        ask->setChecked(true); remember->setChecked(false);
        authPassword->setEnabled(false);
        setFormRowVisible(authForm, nativeAuthSource_, password);
        for (QWidget *widget : QList<QWidget *>{authName, authPassword, ask, remember}) setFormRowVisible(authForm, widget, enter);
        setFormRowVisible(authForm, identity, !password);
        setFormRowVisible(authForm, choose, !password);
        notice->setText(nativeAuthPreview_
            ? tr("UI draft only: credentials are not saved or sent.")
            : tr("Authentication setup is not available in this build. Saved Windows credentials have not been checked."));
        if (auto *box = findChild<QDialogButtonBox *>()) {
            box->button(QDialogButtonBox::Save)->setEnabled(!native_ || !nativeAuthPreview_);
        }
    };
    connect(nativeAuthMethod_, &QComboBox::currentIndexChanged, this, [this] { refreshNativeAuthentication_(); });
    connect(nativeAuthSource_, &QComboBox::currentIndexChanged, this, [this] { refreshNativeAuthentication_(); });
    connect(windowsProfile_, &QLineEdit::textChanged, this, [this] { nativeAuthMethod_->setCurrentIndex(0); nativeAuthSource_->setCurrentIndex(0); });
    connect(nativeServerName_, &QLineEdit::textChanged, this, [this] { nativeAuthMethod_->setCurrentIndex(0); nativeAuthSource_->setCurrentIndex(0); });
    nativeProfileActions_ = new QWidget;
    nativeProfileActions_->setObjectName("nativeProfileActions");
    auto *nativeActions = new QVBoxLayout(nativeProfileActions_);
    nativeActions->setContentsMargins(0, 0, 0, 0);

    auto *nativeNotice = new QLabel(tr("Windows profile operations are not enabled in this build."));
    nativeNotice->setWordWrap(true);
    nativeNotice->setTextFormat(Qt::PlainText);
    nativeActions->addWidget(nativeNotice);
    auto *nativeActionRow = new QHBoxLayout;
    for (const auto &text : {tr("Review changes"), tr("Remove Windows profile")}) {
        auto *button = new QPushButton(text);
        button->setEnabled(false);
        nativeActionRow->addWidget(button);
    }
    nativeActions->addLayout(nativeActionRow);
    int serverRow = 0;
    QFormLayout::ItemRole serverRole;
    generalForm->getWidgetPosition(nativeServerName_, &serverRow, &serverRole);
    generalForm->insertRow(serverRow + 1, nativeProfileActions_);
    generalForm->addRow(creating_ ? tr("Protocol") : tr("Key source"), keySource_);
    if (explicitMissing) {
        auto *explicitNotice = new QLabel(tr("Hysteria2, VMess, TUIC, AnyTLS and Naive QUIC are unavailable: the running Core does not accept them as separate settings. Start or update Core to create these profiles."));
        explicitNotice->setObjectName("explicitAccessUnavailable");
        explicitNotice->setWordWrap(true);
        generalForm->addRow(explicitNotice);
    }
    if (creating_ && !wireGuardCreatable_) {
        auto *wireGuardNotice = new QLabel(tr("WireGuard and AmneziaWG are unavailable: the running Core does not support them. Start or update Core to create these profiles."));
        wireGuardNotice->setObjectName("wireGuardUnavailable");
        wireGuardNotice->setWordWrap(true);
        generalForm->addRow(wireGuardNotice);
    }
    wgServiceNotice_ = new QLabel(tr("The running Core does not support WireGuard and AmneziaWG. The profile can be edited, but it connects only after Core is started or updated."));
    wgServiceNotice_->setObjectName("wireGuardServiceUnavailable");
    wgServiceNotice_->setWordWrap(true);
    generalForm->addRow(wgServiceNotice_);
    accessConversion_ = new QWidget;
    accessConversion_->setObjectName("accessConversion");
    auto *conversionLayout = new QHBoxLayout(accessConversion_);
    conversionLayout->setContentsMargins(0, 0, 0, 0);
    auto *toFields = new QPushButton(tr("Convert link to fields"));
    toFields->setObjectName("convertAccessToFields");
    auto *toLink = new QPushButton(tr("Convert fields to link"));
    toLink->setObjectName("convertAccessToLink");
    conversionLayout->addWidget(toFields);
    conversionLayout->addWidget(toLink);
    conversionLayout->addStretch();
    connect(toFields, &QPushButton::clicked, this, [this] { convertAccess(true); });
    connect(toLink, &QPushButton::clicked, this, [this] { convertAccess(false); });
    generalForm->addRow(accessConversion_);
    generalForm->addRow(tr("Key or path"), keyRow);
    generalForm->addRow(tr("Server"), server_);
    generalForm->addRow(tr("Port"), port_);
    generalForm->addRow(tr("Cipher"), method_);
    generalForm->addRow(tr("Username"), username_);
    generalForm->addRow(tr("SSH identity file"), identityRow);
    generalForm->addRow(tr("SSH known_hosts file"), hostsRow);
    generalForm->addRow(tr("Password"), password_);
    generalForm->addRow(tr("Prefix"), prefix_);
    generalForm->addRow(tr("Plugin"), ssPlugin_);
    generalForm->addRow(tr("Plugin options"), ssPluginOpts_);
    generalForm->addRow(tr("Mode"), mode_);
    generalForm->addRow(tr("SOCKS5 address"), listen_);
    auto *keyLabel = qobject_cast<QLabel *>(generalForm->labelForField(keyRow));
    if (routingHasSection(text, "IKEv2")) {
        auto *nativeGeneralScroll = new QScrollArea;
        nativeGeneralScroll->setWidgetResizable(true);
        nativeGeneralScroll->setFrameShape(QFrame::NoFrame);
        generalForm->setFormAlignment(Qt::AlignTop);
        nativeGeneralScroll->setWidget(general);
        general->setAutoFillBackground(false);
        nativeGeneralScroll->viewport()->setAutoFillBackground(false);
        nativeGeneralScroll->setAutoFillBackground(false);
        tabs_->addTab(nativeGeneralScroll, tr("General"));
    } else {
        tabs_->addTab(general, tr("General"));
    }

    auto *vlessPage = new QWidget;
    auto *vlessForm = new QFormLayout(vlessPage);
    auto *vlessEndpoint = new QLineEdit;
    vlessEndpoint->setObjectName("vlessEndpoint");
    auto *vlessEncryption = new QLineEdit;
    auto *vlessExtra = new QPlainTextEdit;
    vlessExtra->setObjectName("vlessExtra");
    vlessExtra->setFixedHeight(72);
    vlessId_ = new QLineEdit;
    vlessId_->setObjectName("vlessId");
    vlessId_->setEchoMode(QLineEdit::Password);
    vlessNetwork_ = new HiDpiComboBox;
    vlessNetwork_->setObjectName("vlessNetwork");
    for (const char *network : {"tcp", "ws", "grpc", "xhttp"}) vlessNetwork_->addItem(network);
    vlessSecurity_ = new HiDpiComboBox;
    vlessSecurity_->setObjectName("vlessSecurity");
    vlessSecurity_->addItems({"tls", "reality"});
    vlessFlow_ = new HiDpiComboBox;
    vlessFlow_->addItems({"", "xtls-rprx-vision", "xtls-rprx-vision-udp443"});
    vlessServerName_ = new QLineEdit;
    vlessFingerprint_ = new HiDpiComboBox;
    vlessFingerprint_->addItems({"", "chrome", "firefox", "safari", "ios", "android", "edge"});
    vlessPublicKey_ = new QLineEdit;
    vlessShortId_ = new QLineEdit;
    vlessPublicKey_->setEchoMode(QLineEdit::Password);
    vlessShortId_->setEchoMode(QLineEdit::Password);
    vlessSpiderX_ = new QLineEdit;
    vlessAlpn_ = new QLineEdit;
    vlessPath_ = new QLineEdit;
    vlessPath_->setObjectName("vlessPath");
    vlessHost_ = new QLineEdit;
    vlessEarlyData_ = new QSpinBox;
    vlessEarlyData_->setObjectName("vlessEarlyData");
    vlessEarlyData_->setRange(0, 8192);
    vlessServiceName_ = new QLineEdit;
    vlessServiceName_->setObjectName("vlessServiceName");
    vlessServiceName_->setPlaceholderText("proxy or /proxy/Custom");
    vlessAuthority_ = new QLineEdit;
    vlessMultiMode_ = new HiDpiCheckBox;
    vlessMultiMode_->setObjectName("vlessMultiMode");
    vlessXhttpMode_ = new HiDpiComboBox;
    vlessXhttpMode_->addItems({"packet-up", "stream-up", "auto", "stream-one"});
    vlessForm->addRow(tr("Server"), vlessEndpoint);
    vlessForm->addRow(tr("Encryption"), vlessEncryption);
    vlessForm->addRow(tr("User ID"), vlessId_);
    vlessForm->addRow(tr("Transport"), vlessNetwork_);
    vlessForm->addRow(tr("Security"), vlessSecurity_);
    vlessForm->addRow(tr("Flow"), vlessFlow_);
    vlessForm->addRow(tr("Server name (SNI)"), vlessServerName_);
    vlessForm->addRow(tr("Fingerprint"), vlessFingerprint_);
    vlessForm->addRow(tr("REALITY public key"), vlessPublicKey_);
    vlessForm->addRow(tr("REALITY short ID"), vlessShortId_);
    vlessForm->addRow(tr("REALITY SpiderX"), vlessSpiderX_);
    vlessForm->addRow(tr("ALPN"), vlessAlpn_);
    vlessForm->addRow(tr("Path"), vlessPath_);
    vlessForm->addRow(tr("Host"), vlessHost_);
    vlessForm->addRow(tr("WebSocket early data"), vlessEarlyData_);
    vlessForm->addRow(tr("gRPC service name"), vlessServiceName_);
    vlessForm->addRow(tr("gRPC authority"), vlessAuthority_);
    vlessForm->addRow(tr("gRPC TunMulti"), vlessMultiMode_);
    vlessForm->addRow(tr("XHTTP mode"), vlessXhttpMode_);
    vlessForm->addRow(tr("XHTTP extra"), vlessExtra);
    // Hidden tab pages still contribute to QStackedWidget's minimum size.
    // Bound the long VLESS form with a scroll viewport, as for other parameters.
    auto *vlessScroll = new QScrollArea;
    vlessScroll->setObjectName("vlessScroll");
    vlessScroll->setWidgetResizable(true);
    vlessScroll->setFrameShape(QFrame::NoFrame);
    vlessScroll->setWidget(vlessPage);
    vlessPage->setAutoFillBackground(false);
    vlessScroll->viewport()->setAutoFillBackground(false);
    tabs_->addTab(vlessScroll, tr("VLESS"));

    auto *network = new QWidget;
    auto *networkForm = new QFormLayout(network);
    interfaceName_ = new QLineEdit;
    interfaceName_->setObjectName("interfaceName");
    mtu_ = new QSpinBox;
    mtu_->setRange(1280, 1500);
    dns_ = new DnsAddressEdit(true);
    dns_->setObjectName("dns");
    // An empty field omits DNS so Core applies its own default, shown here.
    const QString defaultDns = ProfileStore::defaultDns(capabilities);
    dns_->setPlaceholderText(defaultDns);
    dns_->setToolTip(tr("Leave empty to use the Core default: %1").arg(defaultDns));
    configureDoh_ = new QPushButton;
    configureDoh_->setObjectName("configureDoh");
    connect(configureDoh_, &QPushButton::clicked, this, &ProfileEditorDialog::editDoh);
    refreshDohSummary();
    ipv6_ = new HiDpiComboBox;
    ipv6_->addItem(tr("Enabled"), "enable");
    ipv6_->addItem(tr("Disabled"), "disable");
    killSwitch_ = new HiDpiCheckBox(tr("Block traffic outside the VPN"));
    const bool killSwitchSupported = capabilities.value("kill_switch").toBool();
    killSwitch_->setEnabled(killSwitchSupported);
    if (!killSwitchSupported) killSwitch_->setToolTip(tr("Not supported by the current core build"));
    networkForm->addRow(tr("Interface name"), interfaceName_);
    networkForm->addRow(tr("MTU"), mtu_);
    networkForm->addRow(tr("DNS (up to four IPv4 addresses)"), dns_);
    networkForm->addRow(tr("Captured VPN DNS"), configureDoh_);
    networkForm->addRow(tr("IPv6"), ipv6_);
    networkForm->addRow(tr("Kill switch"), killSwitch_);
    tabs_->addTab(network, tr("Network"));

    auto *routing = new QWidget;
    auto *routingLayout = new QVBoxLayout(routing);
    auto *defaultRow = new QHBoxLayout;
    defaultRoute_ = new HiDpiComboBox;
    defaultRoute_->addItem(tr("Via VPN"), "proxy");
    defaultRoute_->addItem(tr("Direct"), "direct");
    defaultRoute_->addItem(tr("Block"), "block");
    ignoreLocalNetwork_ = new HiDpiCheckBox(tr("Bypass the VPN for local networks"));
    ignoreLocalNetwork_->setToolTip(tr("Route private and link-local addresses outside the VPN"));
    ignoreLocalNetwork_->setEnabled(capabilities.value("ip_split").toBool());
    defaultRow->addWidget(new QLabel(tr("Default action")));
    defaultRow->addWidget(defaultRoute_);
    defaultRow->addSpacing(12);
    defaultRow->addWidget(ignoreLocalNetwork_);
    defaultRow->addStretch();
    allowedIpsGroup_ = new QWidget;
    allowedIpsGroup_->setObjectName("allowedIpsGroup");
    auto *allowedLayout = new QVBoxLayout(allowedIpsGroup_);
    allowedLayout->setContentsMargins(0, 0, 0, 6);
    auto *allowedRow = new QHBoxLayout;
    allowedRow->addWidget(new QLabel(tr("AllowedIPs priority")));
    allowedIpsMode_ = new HiDpiComboBox;
    allowedIpsMode_->setObjectName("allowedIpsMode");
    for (const auto &[label, value, hint] : std::initializer_list<std::tuple<QString, const char *, QString>>{
             {tr("Direct outside AllowedIPs"), "direct", tr("Default. Bimeron rules decide; tunnel traffic outside AllowedIPs goes directly.")},
             {tr("Block outside AllowedIPs"), "block", tr("Bimeron rules decide; tunnel traffic outside AllowedIPs is blocked.")},
             {tr("Ignore AllowedIPs"), "ignore", tr("Bimeron rules decide; all tunnel traffic goes into the tunnel.")},
             {tr("AllowedIPs only"), "only", tr("As in plain WireGuard: AllowedIPs go into the tunnel, everything else directly. Bimeron rules are inactive.")}}) {
        allowedIpsMode_->addItem(label, QString(value));
        allowedIpsMode_->setItemData(allowedIpsMode_->count() - 1, hint, Qt::ToolTipRole);
    }
    allowedRow->addWidget(allowedIpsMode_);
    allowedRow->addStretch();
    allowedLayout->addLayout(allowedRow);
    for (QLabel **label : {&allowedIpsHint_, &allowedIpsNote_}) {
        *label = new QLabel;
        (*label)->setWordWrap(true);
        (*label)->setTextFormat(Qt::PlainText);
        allowedLayout->addWidget(*label);
    }
    allowedIpsHint_->setObjectName("allowedIpsHint");
    allowedIpsList_ = new AllowedIpsSummary;
    allowedLayout->insertWidget(2, allowedIpsList_);
    allowedIpsNote_->setObjectName("allowedIpsNote");
    allowedIpsGroup_->hide();
    routingLayout->addWidget(allowedIpsGroup_);
    connect(allowedIpsMode_, &QComboBox::currentIndexChanged, this, [this] { refreshRouting(); refreshWireGuard(); });
    auto *rulesArea = new QWidget;
    rulesArea->setObjectName("routingRules");
    auto *rulesLayout = new QVBoxLayout(rulesArea);
    rulesLayout->setContentsMargins(0, 0, 0, 0);
    ruleControls_ = {rulesArea};
    rulesLayout->addLayout(defaultRow);
    rules_ = new QTableWidget(0, 4);
    // A flexible table must not turn its preferred viewport height into the
    // height-for-width minimum of every page in QTabWidget.
    rules_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    rules_->setMinimumHeight(100);
    rules_->setProperty("ipSplit", capabilities.value("ip_split").toBool());
    rules_->setProperty("domainSplit", capabilities.value("domain_split").toBool());
    rules_->setHorizontalHeaderLabels({tr("On"), tr("Type"), tr("Destination"), tr("Action")});
    rules_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    rules_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    rules_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    rules_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    rules_->horizontalHeader()->resizeSection(0, 48);
    rules_->horizontalHeader()->resizeSection(1, 120);
    rules_->horizontalHeader()->resizeSection(3, 136);
    rules_->verticalHeader()->hide();
    rules_->verticalHeader()->setDefaultSectionSize(30);
    rules_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rulesLayout->addWidget(rules_, 1);
    auto *ruleButtons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add"));
    add->setEnabled(capabilities.value("ip_split").toBool() || capabilities.value("domain_split").toBool());
    auto *remove = new QPushButton(tr("Remove"));
    auto *up = new QPushButton(tr("Move up"));
    auto *down = new QPushButton(tr("Move down"));
    const auto updateRuleActions = [this, remove, up, down] {
        const int row = rules_->currentRow();
        const bool selected = row >= 0 && rules_->selectionModel()->hasSelection();
        remove->setEnabled(selected);
        up->setEnabled(selected && row > 0);
        down->setEnabled(selected && row + 1 < rules_->rowCount());
    };
    connect(rules_, &QTableWidget::itemSelectionChanged, this, updateRuleActions);
    connect(rules_->model(), &QAbstractItemModel::rowsInserted, this, updateRuleActions);
    connect(rules_->model(), &QAbstractItemModel::rowsRemoved, this, updateRuleActions);
    updateRuleActions();
    connect(add, &QPushButton::clicked, this, [this] {
        addRule(Rule{});
        const int row = rules_->rowCount() - 1;
        rules_->setCurrentCell(row, 1, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        rules_->scrollTo(rules_->model()->index(row, 1));
        rules_->cellWidget(row, 1)->setFocus(Qt::OtherFocusReason);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = rules_->currentRow();
        if (row < 0) return;
        if (!tableRule(row).target.trimmed().isEmpty()
            && QMessageBox::question(this, tr("Remove rule"),
                tr("This rule has a destination. Remove it?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        rules_->removeRow(row);
        refreshRouting();
    });
    connect(up, &QPushButton::clicked, this, [this, updateRuleActions] { moveRule(-1); updateRuleActions(); });
    connect(down, &QPushButton::clicked, this, [this, updateRuleActions] { moveRule(1); updateRuleActions(); });
    for (auto *button : {add, remove, up, down}) ruleButtons->addWidget(button);
    ruleButtons->addStretch();
    rulesLayout->addLayout(ruleButtons);
    routingLayout->addWidget(rulesArea, 1);
    auto *firstMatch = new QLabel(tr("Rules run top to bottom; the first match wins. New local-network bypass rules are inserted first; imported rules keep their positions. Edited IP literals use /32 or /128."));
    firstMatch->setWordWrap(true);
    auto *guidance = new QPushButton(tr("How rules are applied"));
    guidance->setCheckable(true);
    firstMatch->hide();
    connect(guidance, &QPushButton::toggled, firstMatch, &QWidget::setVisible);
    routingLayout->addWidget(guidance, 0, Qt::AlignLeft);
    routingLayout->addWidget(firstMatch);
    auto *limitations = new QPushButton(tr("Routing limitations"));
    connect(limitations, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Routing limitations"),
            tr("GeoSite VPN routing uses DNS metadata (TTL, shared IPs and caches); DoH/DoT and bare IPs may not match. SOCKS GeoIP uses known IPs only, without local DNS lookup. IPv6 disable remains dominant."));
    });

    routingStatus_ = new QLabel;
    routingStatus_->setObjectName("routingStatus");
    routingStatus_->setWordWrap(true);
    routingStatus_->setTextFormat(Qt::PlainText);
    // Capability diagnostics can be long on older cores. Keep them scrollable
    // instead of letting a hidden tab increase the entire dialog's minimum height.
    auto *routingStatusScroll = new QScrollArea;
    routingStatusScroll->setObjectName("routingStatusScroll");
    routingStatusScroll->setWidgetResizable(true);
    routingStatusScroll->setFrameShape(QFrame::NoFrame);
    routingStatusScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    routingStatusScroll->setMinimumHeight(0);
    routingStatusScroll->setMaximumHeight(64);
    routingStatusScroll->setWidget(routingStatus_);
    routingStatus_->setAutoFillBackground(false);
    routingStatusScroll->viewport()->setAutoFillBackground(false);
    routingLayout->addWidget(routingStatusScroll);
    auto *manage = new QPushButton(tr("Manage databases"));
    manage->setObjectName("manageGeodata");
    manage->setObjectName("manageDatabases");
    connect(manage, &QPushButton::clicked, this, &ProfileEditorDialog::manageDatabases);
    auto *secondary = new QHBoxLayout;
    secondary->addWidget(limitations);
    secondary->addWidget(manage);
    routingLayout->addLayout(secondary);
    add->setShortcut(QKeySequence("Alt+Insert"));
    remove->setShortcut(QKeySequence("Alt+Delete"));
    up->setShortcut(QKeySequence("Alt+Up"));
    down->setShortcut(QKeySequence("Alt+Down"));
    tabs_->addTab(routing, tr("Routing"));

    auto *rawPage = new QWidget;
    auto *rawLayout = new QVBoxLayout(rawPage);
    raw_ = new QPlainTextEdit;
    raw_->setObjectName("rawConfig");
    raw_->setLineWrapMode(QPlainTextEdit::NoWrap);
    raw_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    new ConfigHighlighter(raw_->document());
    auto *rawNotice = new QLabel(tr("Raw config contains credentials."));
    rawNotice->setWordWrap(true);
    rawLayout->addWidget(rawNotice);
    rawLayout->addWidget(raw_);
    tabs_->addTab(rawPage, tr("Raw config"));
    for (int index = 0; index < tabs_->count(); ++index) {
        auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(index + 1)), this);
        connect(shortcut, &QShortcut::activated, this, [this, index] { tabs_->setCurrentIndex(index); });
    }
    // Qt creates clickable tab-scroll buttons even when all tabs fit. They also
    // render incorrectly with our flat tab style, so disable them explicitly.
    for (auto *button : tabs_->tabBar()->findChildren<QToolButton *>()) {
        button->setEnabled(false);
        button->setAttribute(Qt::WA_TransparentForMouseEvents);
        button->hide();
    }
    layout->addWidget(tabs_);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Save)->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (native_ && nativeAuthPreview_) return;
        if (rawOnly_ && !isWireGuard() && tabs_->currentIndex() != tabs_->count() - 1) {
            tabs_->setCurrentIndex(tabs_->count() - 1);
            return;
        }
        if (tabs_->currentIndex() != tabs_->count() - 1
            && keySource_->currentData().toString() == "Explicit"
            && method_->currentText().startsWith("2022-") && !prefix_->text().isEmpty()) {
            QMessageBox::warning(this, tr("Error"), tr("SS2022 does not support Prefix. Clear Prefix before saving."));
            return;
        }
        clearProblemMarks();
        const QString draft = configText();
        const bool checkParameters = !native_ && !rawOnly_ && !rawDirty_;
        CoreFailure failure;
        failure.text = (rawDirty_ || rawOnly_ || !bridgeReady_) ? QString() : bridgeError_;
        if (failure.text.isEmpty() && checkParameters && !isWireGuard()
            && (freshDraft_ || dns_->text() != visualFields(visualBaseline_).value("Interface/DNS")))
            failure.text = static_cast<DnsAddressEdit *>(dns_)->error(dns_->text());
        if (failure.text.isEmpty()) failure.text = routingError();
        // Core problems name the exact field, so they take precedence over the
        // editor's own combination check, which only reports a generic message.
        if (!native_ && failure.text.isEmpty() && !validateDraft(draft, &failure) && failure.isEmpty())
            failure.text = tr("Core validation failed. The draft has been retained.");
        if (failure.isEmpty() && checkParameters && !parameterError().isEmpty()) failure.text = parameterError();
        if (!failure.isEmpty()) {
            showProblems(failure);
            return;
        }
        if (configText() != originalText_
            && nativeProtectionIntent(configText()) != ProtectionIntent::Disabled
            && nativeProtectionIntent(originalText_) != ProtectionIntent::Enabled) {
            if (QMessageBox::question(this, tr("Native protection"),
                tr("Initial connection bootstrap is unprotected. Native protection blocks ordinary LAN access. A crash or failed cleanup can retain WFP and DNS protection and leave this computer offline until explicit administrative recovery. Enable it?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &ProfileEditorDialog::reject);
    layout->addWidget(buttons);
    parameterPage_ = new QWidget;
    auto *parameterLayout = new QVBoxLayout(parameterPage_);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setAutoFillBackground(false);
    auto *fields = new QWidget;
    auto *form = new QFormLayout(fields);
    trojanNetwork_ = new HiDpiComboBox;
    trojanNetwork_->setObjectName("trojanNetwork");
    trojanNetwork_->addItems({"", "tcp", "ws", "grpc"});
    form->addRow(tr("Trojan transport"), trojanNetwork_);
    const QList<QPair<QString, QString>> labels{
        {"ServerName", tr("Server name (SNI)")}, {"Security", tr("Security")},
        {"ALPN", tr("ALPN")}, {"WSPath", tr("Path")}, {"WSHost", tr("Host")},
        {"GRPCServiceName", tr("gRPC service name")}, {"GRPCAuthority", tr("gRPC authority")},
        {"GRPCMultiMode", tr("gRPC TunMulti")}, {"CAFile", tr("CA file")},
        {"MASQUEPath", tr("MASQUE UDP path")}, {"ID", tr("User ID (UUID)")},
        {"Fingerprint", tr("Fingerprint")}, {"VMessSecurity", tr("VMess encryption")},
        {"VMessNetwork", tr("VMess transport")}, {"VMessTLS", tr("VMess TLS")},
        {"VMessPath", tr("Transport path / service name")}, {"VMessHost", tr("Transport host / authority")},
        {"VMessMode", tr("Transport mode")}, {"VMessExtra", tr("XHTTP extra (JSON)")},
        {"PublicKey", tr("Reality public key")}, {"ShortID", tr("Reality short ID")}, {"SpiderX", tr("Reality spider path")}};
    for (const auto &entry : labels) {
        auto *edit = new QLineEdit;
        edit->setObjectName("parameter" + entry.first);
        parameters_.insert(entry.first, edit);
        form->addRow(entry.second, edit);
    }
    parameters_["VMessSecurity"]->setPlaceholderText("auto, aes-128-gcm, chacha20-poly1305");
    parameters_["VMessNetwork"]->setPlaceholderText("tcp, ws, httpupgrade, grpc, xhttp");
    parameters_["VMessTLS"]->setPlaceholderText("tls, reality");
    parameters_["VMessMode"]->setPlaceholderText("auto, packet-up, stream-up, stream-one, multi");
    parameters_["Security"]->setValidator(new QRegularExpressionValidator(QRegularExpression("tls"), this));
    parameters_["GRPCMultiMode"]->setValidator(new QRegularExpressionValidator(QRegularExpression("true|false"), this));
    parameters_["Security"]->setPlaceholderText(tr("Empty uses TLS"));
    parameters_["GRPCMultiMode"]->setPlaceholderText(tr("Empty, true or false"));
    auto *browseCa = new QPushButton(tr("Choose CA file"));
    browseCa->setObjectName("browseParameterCa");
    form->addRow(browseCa);
    connect(browseCa, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("CA file"));
        if (!path.isEmpty()) parameters_["CAFile"]->setText(QFileInfo(path).absoluteFilePath());
    });
    shadowTls_ = new HiDpiCheckBox(tr("Use ShadowTLS"));
    shadowTls_->setObjectName("shadowTls");
    shadowName_ = new QLineEdit;
    shadowName_->setObjectName("shadowName");
    shadowPassword_ = new QLineEdit;
    shadowPassword_->setObjectName("shadowPassword");
    shadowPassword_->setEchoMode(QLineEdit::Password);

    shadowGuidance_ = new QLabel;
    shadowGuidance_->setObjectName("shadowGuidance");
    shadowGuidance_->setWordWrap(true);
    // Keep wrapping text outside the form: Qt 6.4's height-for-width handling
    // can corrupt its row cache when form rows are hidden.
    auto *parameterContent = new QWidget;
    auto *parameterContentLayout = new QVBoxLayout(parameterContent);
    parameterContentLayout->setContentsMargins(0, 0, 0, 0);
    parameterContentLayout->addWidget(fields);
    parameterContent->setAutoFillBackground(false);
    auto *heading = new QHBoxLayout;
    auto *title = new QLabel(tr("ShadowTLS v3"));
    title->setObjectName("sectionTitle");
    const QString linkColor = palette().color(QPalette::Text).lightness() > 128 ? "#77aef4" : "#1267d6";
    auto *about = new QLabel("<a href=\"about\" style=\"color:" + linkColor + "\">" + tr("About ShadowTLS") + "</a>");
    about->setObjectName("shadowAbout");
    about->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    heading->addWidget(title);
    heading->addStretch();
    heading->addWidget(about);
    parameterContentLayout->addLayout(heading);
    auto *subtitle = new QLabel(tr("Additional camouflage for the proxy connection."));
    subtitle->setObjectName("sectionHint");
    subtitle->setWordWrap(true);
    parameterContentLayout->addWidget(subtitle);
    parameterContentLayout->addSpacing(12);
    parameterContentLayout->addWidget(shadowTls_);
    auto *wrapperForm = new QGridLayout;
    wrapperForm->setContentsMargins(0, 0, 0, 0);
    wrapperForm->setHorizontalSpacing(12);
    wrapperForm->setVerticalSpacing(6);
    wrapperForm->setColumnStretch(1, 1);
    auto *shadowNameLabel = new QLabel(tr("Handshake server name"));
    shadowNameLabel->setBuddy(shadowName_);
    wrapperForm->addWidget(shadowNameLabel, 0, 0);
    wrapperForm->addWidget(shadowName_, 0, 1);
    auto *nameHint = new QLabel(tr("The external TLS server name configured for ShadowTLS."));
    nameHint->setObjectName("sectionHint");
    nameHint->setWordWrap(true);
    wrapperForm->addWidget(nameHint, 1, 1);
    wrapperForm->setRowMinimumHeight(2, 10);
    auto *shadowPasswordLabel = new QLabel(tr("ShadowTLS password"));
    shadowPasswordLabel->setBuddy(shadowPassword_);
    wrapperForm->addWidget(shadowPasswordLabel, 3, 0);
    wrapperForm->addWidget(shadowPassword_, 3, 1);
    auto *passwordHint = new QLabel(tr("A separate password, independent of the inner proxy."));
    passwordHint->setObjectName("sectionHint");
    passwordHint->setWordWrap(true);
    wrapperForm->addWidget(passwordHint, 4, 1);
    parameterContentLayout->addLayout(wrapperForm);
    parameterContentLayout->addSpacing(12);
    QPalette hintPalette = shadowGuidance_->palette();
    hintPalette.setColor(QPalette::WindowText, palette().color(QPalette::PlaceholderText));
    shadowGuidance_->setPalette(hintPalette);
    parameterContentLayout->addWidget(shadowGuidance_);
    connect(about, &QLabel::linkActivated, this, [this] {
        QMessageBox::information(this, tr("About ShadowTLS"), tr("ShadowTLS v3 disguises an encrypted TCP connection. It does not replace inner encryption or server verification. The handshake name and password are independent of the inner proxy settings. Use the public ShadowTLS listener as the profile server and port. Native UDP is not carried; supported inner protocols retain UDP-over-stream."));
    });
    parameterContentLayout->addStretch();
    scroll->setWidget(parameterContent);
    parameterContent->setAutoFillBackground(false);
    scroll->viewport()->setAutoFillBackground(false);
    parameterLayout->addWidget(scroll);
    tabs_->insertTab(tabs_->count() - 1, parameterPage_, tr("Protocol"));
    wgForm_ = new WireGuardForm(networkForm, coreExecutable, keyOperations);
    auto *wgScroll = new QScrollArea;
    wgScroll->setObjectName("wireGuardScroll");
    wgScroll->setWidgetResizable(true);
    wgScroll->setFrameShape(QFrame::NoFrame);
    wgScroll->setWidget(wgForm_);
    wgForm_->setAutoFillBackground(false);
    wgScroll->viewport()->setAutoFillBackground(false);
    wgPage_ = wgScroll;
    tabs_->insertTab(tabs_->count() - 1, wgPage_, QStringLiteral("WireGuard"));
    wgRawPage_ = new QWidget;
    auto *wgRawLayout = new QVBoxLayout(wgRawPage_);
    auto *wgRawNotice = new QLabel(tr("The WG/AWG config contains the private key. Copy it from here to use the profile in another WireGuard or AmneziaWG client."));
    wgRawNotice->setWordWrap(true);
    wgRawLayout->addWidget(wgRawNotice);
    wgRaw_ = new QPlainTextEdit;
    wgRaw_->setObjectName("wireGuardRawConfig");
    wgRaw_->setLineWrapMode(QPlainTextEdit::NoWrap);
    wgRaw_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    new ConfigHighlighter(wgRaw_->document());
    wgRawLayout->addWidget(wgRaw_);
    tabs_->insertTab(tabs_->count() - 1, wgRawPage_, tr("WG/AWG config"));
    wgCheck_ = new QTimer(this);
    wgCheck_->setSingleShot(true);
    wgCheck_->setInterval(600);
    connect(wgCheck_, &QTimer::timeout, this, [this] { checkWireGuard(); });
    connect(wgForm_, &WireGuardForm::edited, this, [this] { wgEdited_ = true; wgCheck_->start(); });
    connect(wgForm_, &WireGuardForm::versionChanged, this, [this] { refreshWireGuard(); });
    connect(wgForm_, &WireGuardForm::keyGenerated, this, [this] {
        if (!wgRawDirty_ && tabs_->currentWidget() == wgRawPage_) syncWireGuardRaw();
        wgCheck_->start();
    });
    connect(wgRaw_, &QPlainTextEdit::textChanged, this, [this] {
        wgForm_->invalidateKeyGeneration();
        wgRawDirty_ = wgEdited_ = true;
        if (wgPage_) tabs_->setTabEnabled(tabs_->indexOf(wgPage_), true);
    });
    connect(trojanNetwork_, &QComboBox::currentTextChanged, this, [this] { refreshParameters(); });
    connect(shadowTls_, &QCheckBox::toggled, this, [this] { refreshParameters(); });
    connect(keySource_, &QComboBox::currentIndexChanged, this, [this, form, browseCa] {
        refreshParameters();
        setFormRowVisible(form, browseCa, keySource_->currentData() == "Explicitmasque");
    });
    setFormRowVisible(form, browseCa, valueFor(text, "AccessKey", "Type") == "masque");
    connect(GeoDataSelection::instance(), &GeoDataSelection::catalogResult, this,
        [this](const QString &key, const QStringList &groups, const QString &error, GeoDataSelection::CatalogFailure failure) {
            for (const QString &kind : {QString("geoip"), QString("geosite")}) if (datasetKeys_.value(kind) == key) {
                // Lead with the classified meaning; the raw diagnostic follows for support.
                datasetGroups_[kind] = groups;
                datasetUnavailable_[kind] = failure != GeoDataSelection::CatalogFailure::None;
                datasetErrors_[kind] = failure == GeoDataSelection::CatalogFailure::None ? error
                    : GeoDataSelection::failureExplanation(failure) + '\n' + error;
                refreshRouting();
            }
        });
    connect(GeoDataSelection::instance(), &GeoDataSelection::catalogInvalidated, this, [this](const QString &key) {
        for (const QString &kind : {QString("geoip"), QString("geosite")}) if (datasetKeys_.value(kind) == key) {
            datasetChanged_ = true; inspectDataset(kind);
        }
    });
    loadText(text);
    if (wireGuard_) {
        wgOriginal_ = *wireGuardText;
        const auto reply = WireGuardCore::inspect(coreExecutable_, wgOriginal_);
        loadWireGuard(wgOriginal_, reply.answered() ? reply.object : QJsonObject());
    } else if (creating_) {
        loadWireGuard({}, {});
        // A new draft offers the usual full-tunnel peer networks.
        wgForm_->presetValue("Peer/AllowedIPs", "0.0.0.0/0, ::/0");
    }
    if (freshDraft_) {
        keySource_->setCurrentIndex(keySource_->findData("Explicit"));
        method_->setCurrentText("aes-128-gcm");
        mode_->setCurrentIndex(mode_->findData("vpn"));
        interfaceName_->setText("Bimeron");
        mtu_->setValue(1420);
        ipv6_->setCurrentIndex(ipv6_->findData("enable"));
        listen_->setText("127.0.0.1:1080");
        defaultRoute_->setCurrentIndex(defaultRoute_->findData("proxy"));
    }
    connect(GeoDataSelection::instance(), &GeoDataSelection::changed, this, [this] {
        const auto shared = GeoDataSelection::instance()->paths();
        for (const QString &kind : {QString("geoip"), QString("geosite")}) {
            if (datasetPaths_.value(kind) == shared.value(kind)) continue;
            datasetPaths_[kind] = shared.value(kind); datasetChanged_ = true; inspectDataset(kind);
        }
        cachedVisual_.clear(); refreshRouting();
    });
    const auto applySs = [this](const QString &value) {
        const SsParameters parsed = parseSsUrl(value);
        server_->setText(parsed.valid ? parsed.server : QString());
        port_->setValue(parsed.valid ? parsed.port : 0);
        password_->setText(parsed.valid ? parsed.password : QString());
        prefix_->setText(parsed.valid ? parsed.prefix : QString());
        if (parsed.valid) {
            int index = method_->findText(parsed.method);
            if (index < 0) { method_->addItem(parsed.method); index = method_->count() - 1; }
            method_->setCurrentIndex(index);
        }
        return parsed.valid;
    };
    const auto updateVlessControls = [this, vlessForm, vlessEndpoint, vlessEncryption, vlessExtra] {
        const bool explicitVless = keySource_->currentData().toString() == "ExplicitVLESS";
        const QString source = keySource_->currentData().toString();
        const QUrl url(key_->text().trimmed());
        tabs_->setTabVisible(1, explicitVless || source == "VLESS"
            || (source == "Key" && url.scheme() == "vless"));
        const bool uriVless = (source == "VLESS" || source == "Key") && url.scheme() == "vless"
            && ProfileStore::connectionInfo("[AccessKey]\nKey = " + key_->text()).connectable;
        if (!explicitVless && !uriVless) return;
        setFormRowVisible(vlessForm, vlessEndpoint, uriVless);
        setFormRowVisible(vlessForm, vlessEncryption, uriVless);
        if (uriVless) {
            const QUrlQuery query(url);
            const auto field = [&query](const QString &name) { return query.queryItemValue(name, QUrl::FullyDecoded); };
            const auto choice = [](QComboBox *combo, const QString &value) {
                int index = combo->findText(value);
                if (index < 0 && !value.isEmpty()) { combo->addItem(value); index = combo->count() - 1; }
                combo->setCurrentIndex(value.isEmpty() ? -1 : index);
            };
            vlessId_->setText(url.userName());
            vlessEndpoint->setText(url.host() + ":" + QString::number(url.port()));
            vlessEncryption->setText(field("encryption"));
            server_->setText(url.host());
            port_->setValue(url.port());
            choice(vlessNetwork_, field("type").isEmpty() ? "tcp" : field("type"));
            choice(vlessSecurity_, field("security"));
            choice(vlessFlow_, field("flow"));
            vlessServerName_->setText(field("sni"));
            choice(vlessFingerprint_, field("fp"));
            vlessPublicKey_->setText(field("pbk"));
            vlessShortId_->setText(field("sid"));
            vlessSpiderX_->setText(field("spx"));
            vlessAlpn_->setText(field("alpn"));
            QString path = field("path");
            QString earlyData = field("ed");
            if (vlessNetwork_->currentText() == "ws" && earlyData.isEmpty() && path.contains("?")) {
                QUrl pathUrl(path);
                QUrlQuery pathQuery(pathUrl);
                if (pathQuery.hasQueryItem("ed")) {
                    earlyData = pathQuery.queryItemValue("ed");
                    pathQuery.removeAllQueryItems("ed");
                    pathUrl.setQuery(pathQuery);
                    path = pathUrl.toString(QUrl::FullyDecoded);
                }
            }
            vlessPath_->setText(path);
            vlessHost_->setText(field("host"));
            vlessEarlyData_->setValue(earlyData.toInt());
            vlessServiceName_->setText(field("serviceName"));
            vlessAuthority_->setText(field("authority"));
            vlessMultiMode_->setChecked(field("mode") == "multi");
            choice(vlessXhttpMode_, field("mode"));
            vlessExtra->setPlainText(field("extra").isEmpty() ? QString() : tr("[hidden]"));
        }
        tabs_->widget(1)->setEnabled(explicitVless);
        const QString network = vlessNetwork_->currentText();
        if (auto *model = qobject_cast<QStandardItemModel *>(vlessSecurity_->model()))
            model->item(vlessSecurity_->findText("reality"))->setEnabled(network != "ws");
        const bool reality = vlessSecurity_->currentText() == "reality";
        if (network == "xhttp") {
            const QString current = vlessXhttpMode_->currentText();
            const QSignalBlocker blocker(vlessXhttpMode_);
            vlessXhttpMode_->clear();
            vlessXhttpMode_->addItems(reality ? QStringList{"stream-one", "stream-up", "auto"}
                                               : QStringList{"packet-up", "stream-up", "auto"});
            int index = vlessXhttpMode_->findText(current);
            if (index < 0 && !current.isEmpty()) {
                vlessXhttpMode_->addItem(current);
                index = vlessXhttpMode_->count() - 1;
                qobject_cast<QStandardItemModel *>(vlessXhttpMode_->model())->item(index)->setEnabled(false);
            }
            vlessXhttpMode_->setCurrentIndex(index);
        }
        setFormRowVisible(vlessForm, vlessFlow_, (network == "tcp" && reality) || !vlessFlow_->currentText().isEmpty());
        setFormRowVisible(vlessForm, vlessPublicKey_, reality || !vlessPublicKey_->text().isEmpty());
        setFormRowVisible(vlessForm, vlessShortId_, reality || !vlessShortId_->text().isEmpty());
        setFormRowVisible(vlessForm, vlessSpiderX_, reality || !vlessSpiderX_->text().isEmpty());
        setFormRowVisible(vlessForm, vlessAlpn_, !(network == "tcp" && reality));
        setFormRowVisible(vlessForm, vlessPath_, network == "ws" || network == "xhttp");
        setFormRowVisible(vlessForm, vlessHost_, network == "ws" || network == "xhttp");
        setFormRowVisible(vlessForm, vlessEarlyData_, network == "ws" || vlessEarlyData_->value() != 0);
        setFormRowVisible(vlessForm, vlessServiceName_, network == "grpc" || !vlessServiceName_->text().isEmpty());
        setFormRowVisible(vlessForm, vlessAuthority_, network == "grpc" || !vlessAuthority_->text().isEmpty());
        setFormRowVisible(vlessForm, vlessMultiMode_, network == "grpc" || vlessMultiModeConfigured_ || vlessMultiMode_->isChecked());
        setFormRowVisible(vlessForm, vlessXhttpMode_, network == "xhttp");
        setFormRowVisible(vlessForm, vlessExtra, uriVless && network == "xhttp" && !vlessExtra->toPlainText().isEmpty());
    };
    connect(vlessNetwork_, &QComboBox::currentIndexChanged, this, updateVlessControls);
    connect(vlessSecurity_, &QComboBox::currentIndexChanged, this, updateVlessControls);
    const auto updateKeyControls = [this, generalForm, keyRow, keyLabel, browse, reveal, identityRow, hostsRow, applySs, updateVlessControls] {
        const QString source = keySource_->currentData().toString();
        const bool ss = source == "SS";
        const bool ssconf = source == "SSConf";
        const bool socks5 = source == "SOCKS5";
        const bool file = source == "File";
        const bool explicitKey = source == "Explicit";
        const bool explicitSocks5 = source == "ExplicitSOCKS5";
        const bool explicitVless = source == "ExplicitVLESS";
        const bool explicitSsh = source == "ExplicitSSH";
        const bool ssh = source == "SSH" || explicitSsh || ((source == "Key") && QUrl(key_->text()).scheme() == "ssh");
        const bool added = parameterSource(source);
        const bool showParameters = ss || explicitKey || explicitSocks5 || explicitVless || explicitSsh || added;
        mode_->setToolTip(ssh ? tr("SSH VPN carries TCP and captured DNS only; other proxied UDP is dropped.") : QString());

        keyLabel->setText(file ? tr("File path") : tr("URL"));
        key_->setEnabled(true);
        const bool wireGuard = source == "WireGuard";
        setFormRowVisible(generalForm, keyRow, !explicitKey && !explicitSocks5 && !explicitVless && !explicitSsh && !added && !wireGuard);
        key_->setVisible(file || keyRevealed_);
        browse->setVisible(file);
        reveal->setVisible(ss || ssconf || socks5 || source == "VLESS" || source == "Hysteria2" || source == "SSH" || source == "Key" || linkSources().contains(source));
        reveal->setText(keyRevealed_ ? tr("Hide") : tr("Show"));

        setFormRowVisible(generalForm, server_, showParameters);
        setFormRowVisible(generalForm, port_, showParameters);
        setFormRowVisible(generalForm, method_, ss || explicitKey);
        setFormRowVisible(generalForm, username_, explicitSocks5 || explicitSsh || (added && usesUsername(source)));
        setFormRowVisible(generalForm, identityRow, ssh);
        setFormRowVisible(generalForm, hostsRow, ssh);
        setFormRowVisible(generalForm, password_, showParameters && !explicitVless && !explicitSsh && source != "Explicitvmess");
        setFormRowVisible(generalForm, prefix_, ss || explicitKey);
        setFormRowVisible(generalForm, ssPlugin_, explicitKey);
        setFormRowVisible(generalForm, ssPluginOpts_, explicitKey);
        for (auto *widget : {static_cast<QWidget *>(server_), static_cast<QWidget *>(port_), static_cast<QWidget *>(method_), static_cast<QWidget *>(username_), static_cast<QWidget *>(password_), static_cast<QWidget *>(prefix_)})
            widget->setEnabled(explicitKey || explicitSocks5 || explicitVless || explicitSsh || added);
        if (ss) applySs(key_->text());
        if (explicitKey && port_->value() == 0) port_->setValue(8388);
        if (explicitSocks5 && port_->value() == 0) port_->setValue(1080);
        if (explicitVless && port_->value() == 0) port_->setValue(443);
        if (explicitSsh && port_->value() == 0) port_->setValue(22);
        if (added && port_->value() == 0) port_->setValue(443);
        // Conversion edits the draft of a stored profile, never a new one.
        setFormRowVisible(generalForm, accessConversion_, developerMode_ && !creating_ && !native_
            && !coreExecutable_.isEmpty() && capabilities_.value("access_conversion").toBool());
        const bool linkSource = QStringList{"SS", "SOCKS5", "VLESS", "Hysteria2", "SSH", "Key"}.contains(source) || linkSources().contains(source);
        accessConversion_->findChild<QPushButton *>("convertAccessToFields")->setEnabled(linkSource && !key_->text().trimmed().startsWith("ssconf://"));
        accessConversion_->findChild<QPushButton *>("convertAccessToLink")->setEnabled(source.startsWith("Explicit"));
        // Defaults belong to new drafts; a loaded profile keeps omitted options omitted.
        if (source == "Explicitvmess" && freshDraft_) {
            if (parameters_["VMessTLS"]->text().isEmpty()) parameters_["VMessTLS"]->setText("tls");
            if (parameters_["VMessNetwork"]->text().isEmpty()) parameters_["VMessNetwork"]->setText("tcp");
            if (parameters_["VMessSecurity"]->text().isEmpty()) parameters_["VMessSecurity"]->setText("auto");
        }
        updateVlessControls();
        refreshParameters();
        refreshWireGuard();
    };
    connect(keySource_, &QComboBox::currentIndexChanged, this, [this, updateKeyControls] {
        if (creating_ && isWireGuard()) wgForm_->initializeNewKey();
        else if (!isWireGuard()) wgForm_->invalidateKeyGeneration();
        keyRevealed_ = false;
        updateKeyControls();
    });
    const auto updateSourceLock = [this] {
        const QString source = keySource_->currentData().toString();
        const QString accessKey = source.startsWith("Explicit")
            ? "Type = " + explicitType(source)
            : source + " = " + key_->text();
        keySource_->setEnabled(freshDraft_ || (!wireGuard_ && !bridgeReady_ && !ProfileStore::connectionInfo("[AccessKey]\n" + accessKey).connectable));
    };
    connect(keySource_, &QComboBox::currentIndexChanged, this, updateSourceLock);
    connect(key_, &QLineEdit::textChanged, this, [this, applySs](const QString &value) {
        if (keySource_->currentData().toString() == "SS") applySs(value);
    });
    connect(key_, &QLineEdit::textChanged, this, updateVlessControls);
    connect(key_, &QLineEdit::textChanged, this, updateKeyControls);
    connect(key_, &QLineEdit::textChanged, this, updateSourceLock);
    connect(reveal, &QPushButton::clicked, this, [this, updateKeyControls] {
        keyRevealed_ = !keyRevealed_;
        updateKeyControls();
    });
    refreshControls_ = [updateKeyControls, updateSourceLock] { updateKeyControls(); updateSourceLock(); };
    refreshControls_();
    const auto updateModeControls = [this, killSwitchSupported](bool interactive) {
        bool vpnMode = mode_->currentData().toString() == "vpn";
        if (interactive && !vpnMode && (ipv6_->currentData().toString() == "disable" || killSwitch_->isChecked())) {
            QStringList changes;
            if (ipv6_->currentData().toString() == "disable") changes << tr("enable IPv6");
            if (killSwitch_->isChecked()) changes << tr("disable the kill switch");
            const auto answer = QMessageBox::question(this, tr("Change mode"),
                tr("Local SOCKS5 requires you to %1. Apply the changes?").arg(changes.join(tr(" and "))));
            if (answer == QMessageBox::Yes) {
                ipv6_->setCurrentIndex(ipv6_->findData("enable"));
                killSwitch_->setChecked(false);
            } else {
                const QSignalBlocker blocker(mode_);
                mode_->setCurrentIndex(mode_->findData("vpn"));
                vpnMode = true;
            }
        }
        listen_->setEnabled(!vpnMode);
        ipv6_->setEnabled(vpnMode);
        killSwitch_->setEnabled(vpnMode && killSwitchSupported);
    };
    connect(mode_, &QComboBox::currentIndexChanged, this, [updateModeControls] { updateModeControls(true); });
    refreshControls_ = [updateKeyControls, updateSourceLock, updateModeControls] {
        updateKeyControls();
        updateSourceLock();
        updateModeControls(false);
    };
    updateModeControls(false);
    connect(raw_, &QPlainTextEdit::textChanged, this, [this] {
        rawDirty_ = true;
        // A corrected Raw-only document can request visual editing by switching tabs.
        for (int i = 0; i < tabs_->count() - 1; ++i) tabs_->setTabEnabled(i, true);
    });
    connect(method_, &QComboBox::currentTextChanged, this, [this] { refreshParameters(); });
    connect(ssPlugin_, &QComboBox::currentTextChanged, this, [this] { refreshParameters(); });
    connect(vlessAlpn_, &QLineEdit::textChanged, this, [this] { refreshParameters(); });
    assessRepresentation();
    connect(mode_, &QComboBox::currentIndexChanged, this, [this] { refreshRouting(); });
    connect(killSwitch_, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && bridgeReady_ && mode_->currentData() == "vpn") {
            QList<int> conflicts;
            for (int row = 0; row < rules_->rowCount(); ++row)
                if (tableRule(row).enabled && tableRule(row).matcher != "CIDR") conflicts.append(row);
            if (!conflicts.isEmpty()) {
                QMessageBox prompt(QMessageBox::Warning, tr("Routing conflict"),
                    tr("The VPN kill switch permits CIDR rules only. Disable the conflicting rules or cancel this protection change."), QMessageBox::NoButton, this);
                auto *disable = prompt.addButton(tr("Disable conflicting rules"), QMessageBox::AcceptRole);
                prompt.addButton(QMessageBox::Cancel);
                prompt.exec();
                if (prompt.clickedButton() == disable) {
                    for (int row : conflicts) ruleEnabled(rules_, row)->setChecked(false);
                } else { const QSignalBlocker blocker(killSwitch_); killSwitch_->setChecked(false); }
            }
        }
        refreshRouting();
    });
    connect(ignoreLocalNetwork_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!bridgeReady_) return;
        if (localRule_ >= 0) {
            for (int i = 0; i < rules_->rowCount(); ++i)
                if (tableRule(i).index == localRule_) ruleEnabled(rules_, i)->setChecked(checked);
        } else if (localRule_ == -2) {
            for (int i = 0; i < rules_->rowCount(); ++i)
                if (tableRule(i).index == -2) ruleEnabled(rules_, i)->setChecked(checked);
        } else if (checked) {
            addRule(Rule{localNetworkCidrs(), "direct", true, "CIDR", -2});
            int row = rules_->rowCount() - 1;
            while (row > 0) { rules_->selectRow(row--); moveRule(-1); }
            localRule_ = -2;
        }
        refreshRouting();
    });
    refreshRouting();
    syncRaw();
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        const int rawIndex = tabs_->count() - 1;
        const int wgRawIndex = tabs_->indexOf(wgRawPage_);
        const int previous = previousTab_;
        previousTab_ = index;
        if (previous == wgRawIndex && index != wgRawIndex && wgRawDirty_ && !applyWireGuardRaw()) {
            const QSignalBlocker blocker(tabs_);
            tabs_->setCurrentIndex(wgRawIndex);
            previousTab_ = wgRawIndex;
            return;
        }
        if (index == rawIndex) {
            if (!rawDirty_) syncRaw();
        } else if (rawDirty_) {
            if (!applyRaw()) {
                const QSignalBlocker blocker(tabs_);
                tabs_->setCurrentIndex(rawIndex);
                previousTab_ = rawIndex;
                return;
            }
        }
        if (index == wgRawIndex && !wgRawDirty_) {
            this->wireGuardText(); // The constructor parameter shadows the member.
            if (!wgBridgeError_.isEmpty()) {
                // The raw tab would show the original text and silently drop the
                // pending field edits once edited; keep the field draft instead.
                const QSignalBlocker blocker(tabs_);
                tabs_->setCurrentIndex(tabs_->indexOf(wgPage_));
                previousTab_ = tabs_->indexOf(wgPage_);
                QMessageBox box(QMessageBox::Information, tr("WG/AWG config"),
                    tr("Correct the WireGuard fields first: Core could not apply them, so the WG/AWG config cannot show them yet."), QMessageBox::Ok, this);
                box.setObjectName("wireGuardFieldsPending");
                box.exec();
                return;
            }
            syncWireGuardRaw();
        }
    });
    refreshWireGuard();
    checkWireGuard();
}

ProfileEditorDialog::~ProfileEditorDialog() {
    // Stop callbacks before QWidget members and their children begin destruction.
    const auto processes = findChildren<QProcess *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *watcher : findChildren<QFileSystemWatcher *>()) watcher->disconnect();
    for (auto *process : processes) {
        process->disconnect();
        for (auto *timer : process->findChildren<QTimer *>()) timer->stop();
        if (process->state() != QProcess::NotRunning) process->kill();
    }
    QElapsedTimer elapsed; elapsed.start();
    for (auto *process : processes)
        if (process->state() != QProcess::NotRunning) process->waitForFinished(qMax(1, 1500 - int(elapsed.elapsed())));
}

QStringList ProfileEditorDialog::parameterFields() const {
    const QString source = keySource_->currentData().toString();
    if (source == "Explicithysteria2") return {"ServerName", "ALPN", "Fingerprint"};
    if (source == "Explicitanytls" || source == "ExplicitnaiveQUIC" || source == "Explicitnaive") return {"ServerName"};
    if (source == "Explicittuic") return {"ServerName", "ALPN", "ID"};
    if (source == "Explicitvmess") return {"ServerName", "ALPN", "ID", "Fingerprint", "VMessSecurity", "VMessNetwork", "VMessTLS", "VMessPath", "VMessHost", "VMessMode", "VMessExtra", "PublicKey", "ShortID", "SpiderX"};
    if (source == "Explicitmasque") return {"ServerName", "CAFile", "MASQUEPath"};
    if (source != "ExplicitTrojan") return {};
    QStringList fields{"ServerName", "Security", "ALPN"};
    const QString network = trojanNetwork_->currentText();
    if (network == "ws") fields << "WSPath" << "WSHost";
    if (network == "grpc") fields << "GRPCServiceName" << "GRPCAuthority" << "GRPCMultiMode";
    return fields;
}

void ProfileEditorDialog::refreshParameters() {
    if (!parameterPage_) return;
    auto *form = qobject_cast<QFormLayout *>(parameters_.first()->parentWidget()->layout());
    setFormRowVisible(form, findChild<QPushButton *>("browseParameterCa"), keySource_->currentData() == "Explicitmasque");
    const auto fields = parameterFields();
    for (auto it = parameters_.cbegin(); it != parameters_.cend(); ++it)
        setFormRowVisible(form, it.value(), fields.contains(it.key()) || (!freshDraft_ && !it.value()->text().isEmpty()));
    setFormRowVisible(form, trojanNetwork_, keySource_->currentData() == "ExplicitTrojan");
    shadowName_->setEnabled(shadowTls_->isChecked());
    shadowPassword_->setEnabled(shadowTls_->isChecked());
    const QString source = keySource_->currentData().toString();
    const bool wrapper = source == "Explicit" || source == "ExplicitVLESS" || source == "ExplicitSSH"
        || source == "ExplicitTrojan" || source == "Explicitnaive";
    const bool compatible = wrapper && !(source == "Explicit" && (method_->currentText() == "aes-192-gcm"
        || !ssPlugin_->currentText().isEmpty()))
        && !(source == "ExplicitVLESS" && vlessAlpn_->text().split(',').contains("h3"));
    shadowTls_->setEnabled(compatible || shadowTls_->isChecked());
    QString guidance;
    if (!compatible) {
        if (source == "Explicit" && !ssPlugin_->currentText().isEmpty())
            guidance = tr("Disable the Shadowsocks plugin to use ShadowTLS.");
        else if (source == "Explicit")
            guidance = tr("AES-192-GCM cannot be used with ShadowTLS. Choose a supported cipher.");
        else if (source == "ExplicitVLESS")
            guidance = tr("HTTP/3 cannot be used with ShadowTLS. Choose a supported TCP transport.");
        else
            guidance = tr("This profile source has no compatible visual ShadowTLS settings. Use protected Raw configuration for supported sources.");
    } else {
        guidance = tr("Set the ShadowTLS server address and port on the General tab.");
    }
    if (shadowTls_->isChecked() && compatible) {
        guidance += "\n\n" + ((source == "ExplicitVLESS" || source == "ExplicitTrojan")
            ? tr("ShadowTLS carries no native UDP. Existing UDP-over-stream remains available.")
            : tr("In VPN mode, captured DNS uses TCP and other proxy-routed UDP is dropped. Explicit direct routes are unchanged."));
    }
    shadowGuidance_->setText(guidance);
    const bool added = parameterSource(source);
    tabs_->setTabVisible(tabs_->indexOf(parameterPage_), added || wrapper || shadowTls_->isChecked());
}

QString ProfileEditorDialog::parameterError() const {
    const QString source = keySource_->currentData().toString();
    const bool added = parameterSource(source);
    const auto invalid = [this] { return tr("Unsupported parameter combination. Correct the settings or use protected Raw config; no values were discarded."); };
    if (added) {
        if (server_->text().isEmpty() || port_->value() < 1) return invalid();
        const auto allowed = parameterFields();
        for (auto it = parameters_.cbegin(); it != parameters_.cend(); ++it) {
            if ((!freshDraft_ && !allowed.contains(it.key()) && !it.value()->text().isEmpty())
                || it.value()->text().contains(QRegularExpression("[\\r\\n]"))) return invalid();
        }
        if (source == "ExplicitTrojan") {
            if (password_->text().isEmpty() || trojanNetwork_->currentIndex() < 0) return invalid();
            const QString security = parameters_["Security"]->text();
            if (!security.isEmpty() && security != "tls") return invalid();
            const QString alpn = parameters_["ALPN"]->text(), network = trojanNetwork_->currentText();
            if (!alpn.isEmpty() && ((network == "ws" && alpn != "http/1.1") || (network == "grpc" && alpn != "h2"))) return invalid();
            if (network == "grpc" && parameters_["GRPCServiceName"]->text().isEmpty()) return invalid();
            const QString multi = parameters_["GRPCMultiMode"]->text();
            if (!multi.isEmpty() && multi != "true" && multi != "false") return invalid();
        } else if (coreExplicitSources().contains(source)) {
            // Core validates protocol-specific fields and credentials.
        } else if (source == "Explicitnaive") {
            if (username_->text().isEmpty() || password_->text().isEmpty()) return invalid();
        } else if (username_->text().isEmpty() != password_->text().isEmpty()) return invalid();
    }
    if (source == "ExplicitVLESS") {
        const QString network = vlessNetwork_->currentText(), security = vlessSecurity_->currentText(), alpn = vlessAlpn_->text();
        if ((security != "reality" && (!vlessPublicKey_->text().isEmpty() || !vlessShortId_->text().isEmpty() || !vlessSpiderX_->text().isEmpty()))
            || (network != "grpc" && (!vlessServiceName_->text().isEmpty() || !vlessAuthority_->text().isEmpty() || vlessMultiModeConfigured_ || vlessMultiMode_->isChecked()))
            || (network != "ws" && vlessEarlyData_->value() != 0)
            || (network != "ws" && network != "xhttp" && (!vlessPath_->text().isEmpty() || !vlessHost_->text().isEmpty()))
            || (network == "xhttp" && !(security == "reality" ? QStringList{"stream-one", "stream-up", "auto"} : QStringList{"packet-up", "stream-up", "auto"}).contains(vlessXhttpMode_->currentText()))
            || (network == "ws" && security != "tls")
            || (!vlessFlow_->currentText().isEmpty() && !(network == "tcp" && security == "reality"))
            || (!alpn.isEmpty() && ((network == "ws" && alpn != "http/1.1") || (network == "grpc" && alpn != "h2")))
            || (network == "xhttp" && !alpn.isEmpty() && !(security == "reality" ? alpn == "h2" : QStringList{"h2", "h2,http/1.1", "h3"}.contains(alpn)))
            || (network == "xhttp" && alpn == "h3" && (security != "tls" || !vlessFingerprint_->currentText().isEmpty()))) return invalid();
    }
    if (shadowTls_->isChecked()) {
        if (shadowName_->text().isEmpty() || shadowPassword_->text().isEmpty()) return invalid();
        if (!(source == "Explicit" || source == "ExplicitVLESS" || source == "ExplicitSSH" || source == "ExplicitTrojan" || source == "Explicitnaive")) return invalid();
        if (source == "Explicit" && (method_->currentText() == "aes-192-gcm" || !ssPlugin_->currentText().isEmpty())) return invalid();
        if (source == "ExplicitVLESS" && vlessAlpn_->text().split(',').contains("h3")) return invalid();
    }
    return {};
}

void ProfileEditorDialog::loadText(const QString &text) {
    // Loading must not invoke user-edit handlers or mutate another control.
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (auto *widget : findChildren<QWidget *>()) {
        if (qobject_cast<QLineEdit *>(widget) || qobject_cast<QComboBox *>(widget)
            || qobject_cast<QSpinBox *>(widget) || qobject_cast<QCheckBox *>(widget)
            || widget == raw_)
            blockers.push_back(std::make_unique<QSignalBlocker>(widget));
    }
    loadedText_ = text;
    clearProblemMarks();
    native_ = false;
    for (const auto &line : text.split('\n'))
        native_ = native_ || ProfileStore::sectionName(line) == "IKEv2";
    windowsProfile_->setText(valueFor(text, "IKEv2", "WindowsProfile"));
    nativeServerName_->setText(valueFor(text, "IKEv2", "ServerName"));
    nativeKillSwitch_->setChecked(nativeProtectionIntent(text) != ProtectionIntent::Disabled);
    populatingRules_ = true;
    QString inspectionError;
    QJsonObject inspection;
    if (!native_ && !coreExecutable_.isEmpty()) inspection = document({}, &inspectionError);
    documentFields_ = inspection.value("fields").toObject();
    const auto fieldValue = [this, &text](const QString &section, const QString &key) {
        const QString id = section + "/" + key;
        return documentFields_.contains(id) ? documentFields_.value(id).toString() : valueFor(text, section, key);
    };
    rules_->setRowCount(0);
    ignoreLocalNetwork_->setChecked(false);
    hasUnknownSettings_ = false;
    rawOnly_ = !inspection.value("fields_error").toString().isEmpty();
    for (auto it = documentFields_.constBegin(); it != documentFields_.constEnd(); ++it)
        if (it.value().toString().contains('\n') || it.value().toString().contains('\r')) rawOnly_ = true;
    name_->setText(fieldValue("Profile", "Name"));
    QString source = "SS", value = fieldValue("AccessKey", "SS");
    if (value.isEmpty()) { source = "SSConf"; value = fieldValue("AccessKey", "SSConf"); }
    if (value.isEmpty()) { source = "SOCKS5"; value = fieldValue("AccessKey", "SOCKS5"); }
    if (value.isEmpty()) { source = "VLESS"; value = fieldValue("AccessKey", "VLESS"); }
    if (value.isEmpty()) { source = "Hysteria2"; value = fieldValue("AccessKey", "Hysteria2"); }
    if (value.isEmpty()) { source = "SSH"; value = fieldValue("AccessKey", "SSH"); }
    for (const QString &field : linkSources())
        if (value.isEmpty()) { source = field; value = fieldValue("AccessKey", field); }
    if (value.isEmpty()) { source = "Key"; value = fieldValue("AccessKey", "Key"); }
    if (value.isEmpty()) { source = "File"; value = fieldValue("AccessKey", "File"); }
    if (source == "File" && (wireGuard_ || (creating_ && keySource_->currentData().toString() == "WireGuard"))) source = "WireGuard";
    // A conversion can introduce a link source this dialog did not start with.
    if (!value.isEmpty()) ensureSourceItem(source);
    if (value.isEmpty() && !fieldValue("AccessKey", "Server").isEmpty()) {
        const QString type = fieldValue("AccessKey", "Type");
        source = type == "socks5" ? "ExplicitSOCKS5" : type == "vless" ? "ExplicitVLESS" : type == "ssh" ? "ExplicitSSH"
            : type == "trojan" ? "ExplicitTrojan"
            : type == "naive" && fieldValue("AccessKey", "Network") == "quic" ? "ExplicitnaiveQUIC"
            : QStringList{"http", "https", "naive", "masque", "hysteria2", "tuic", "anytls", "vmess"}.contains(type) ? "Explicit" + type
            : (type == "shadowsocks" || type == "ss") ? "Explicit" : QString();
    }
    keySource_->setCurrentIndex(keySource_->findData(source)); key_->setText(value);
    // An access source without a visual equivalent stays Raw-only; native
    // profiles have no [AccessKey] section. A link is shown, never decoded,
    // only when its scheme belongs to the field that holds it.
    if (!native_ && keySource_->currentIndex() < 0) rawOnly_ = true;
    if (linkSources().contains(source) && ProfileStore::uriSourceField(value) != source) rawOnly_ = true;
    server_->setText(fieldValue("AccessKey", "Server"));
    port_->setValue(fieldValue("AccessKey", "Port").toInt());
    const int methodIndex = method_->findText(fieldValue("AccessKey", "Method"));
    method_->setCurrentIndex(methodIndex);
    if (source == "Explicit" && methodIndex < 0) rawOnly_ = true;
    const bool vmess = source == "Explicitvmess";
    for (auto it = parameters_.begin(); it != parameters_.end(); ++it)
        it.value()->setText(parameterSource(source) && (!vmess || parameterFields().contains(it.key())) ? fieldValue("AccessKey", it.key()) : QString());
    if (vmess) {
        // Each VMess editor field accepts one Core option; several mean Raw-only.
        const auto single = [this, &fieldValue](const QString &target, const QStringList &options) {
            QStringList values;
            for (const QString &option : options) {
                QString value = fieldValue("AccessKey", option);
                if (option == "GRPCMultiMode" && (value == "true" || value == "false")) value = value == "true" ? "multi" : "gun";
                if (!value.isEmpty()) values << value;
            }
            if (values.size() > 1) rawOnly_ = true;
            parameters_[target]->setText(values.value(0));
        };
        single("VMessSecurity", {"Encryption"});
        single("VMessNetwork", {"Network"});
        single("VMessTLS", {"Security"});
        single("VMessPath", {"WSPath", "GRPCServiceName", "XHTTPPath"});
        single("VMessHost", {"WSHost", "GRPCAuthority", "XHTTPHost"});
        single("VMessMode", {"XHTTPMode", "GRPCMultiMode"});
        single("VMessExtra", {"XHTTPExtra"});
    }
    trojanNetwork_->setCurrentIndex(trojanNetwork_->findText(fieldValue("AccessKey", "Network")));
    const auto shadow = ProfileTextEdit::shadow(text);
    rawOnly_ = rawOnly_ || !shadow.safe;
    shadowTls_->setChecked(shadow.enabled);
    shadowName_->setText(shadow.enabled ? fieldValue("ShadowTLS", "ServerName") : shadow.name);
    shadowPassword_->setText(shadow.enabled ? fieldValue("ShadowTLS", "Password") : shadow.password);
    password_->setText(fieldValue("AccessKey", "Password"));
    username_->setText(fieldValue("AccessKey", "Username"));
    sshIdentity_->setText(fieldValue("AccessKey", "IdentityFile"));
    sshKnownHosts_->setText(fieldValue("AccessKey", "KnownHostsFile"));
    prefix_->setText(fieldValue("AccessKey", "Prefix"));
    ssPlugin_->setCurrentIndex(ssPlugin_->findText(fieldValue("AccessKey", "Plugin")));
    ssPluginOpts_->setText(fieldValue("AccessKey", "PluginOpts"));
    if (source == "Explicit" && (ssPlugin_->currentIndex() < 0
        || (method_->currentText().startsWith("2022-") && !prefix_->text().isEmpty()))) rawOnly_ = true;
    vlessId_->setText(fieldValue("AccessKey", "ID"));
    const auto setChoice = [this, &fieldValue, &source](QComboBox *combo, const QString &field) {
        const QString value = fieldValue("AccessKey", field);
        if (value.isEmpty()) return;
        const int index = combo->findText(value);
        if (index < 0) {
            // These controls serialize VLESS only; other protocols keep their own fields.
            if (source == "ExplicitVLESS") hasUnknownSettings_ = rawOnly_ = true;
            return;
        }
        combo->setCurrentIndex(index);
    };
    setChoice(vlessNetwork_, "Network");
    setChoice(vlessSecurity_, "Security");
    setChoice(vlessFlow_, "Flow");
    setChoice(vlessFingerprint_, "Fingerprint");
    setChoice(vlessXhttpMode_, "XHTTPMode");
    vlessServerName_->setText(fieldValue("AccessKey", "ServerName"));
    vlessPublicKey_->setText(fieldValue("AccessKey", "PublicKey"));
    vlessShortId_->setText(fieldValue("AccessKey", "ShortID"));
    vlessSpiderX_->setText(fieldValue("AccessKey", "SpiderX"));
    vlessAlpn_->setText(fieldValue("AccessKey", "ALPN"));
    const QString network = vlessNetwork_->currentText();
    vlessPath_->setText(fieldValue("AccessKey", network == "xhttp" ? "XHTTPPath" : "WSPath"));
    vlessHost_->setText(fieldValue("AccessKey", network == "xhttp" ? "XHTTPHost" : "WSHost"));
    vlessEarlyData_->setValue(fieldValue("AccessKey", "WSEarlyData").toInt());
    vlessServiceName_->setText(fieldValue("AccessKey", "GRPCServiceName"));
    vlessAuthority_->setText(fieldValue("AccessKey", "GRPCAuthority"));
    const QString multiMode = fieldValue("AccessKey", "GRPCMultiMode");
    vlessMultiModeConfigured_ = !multiMode.isEmpty();
    vlessMultiMode_->setChecked(multiMode == "true");
    if (source == "ExplicitVLESS" && fieldValue("AccessKey", "Encryption") != "none")
        rawOnly_ = true;
    const QString configuredMode = fieldValue("Interface", "Mode");
    const int modeIndex = mode_->findData(configuredMode);
    if (!native_ && !configuredMode.isEmpty() && modeIndex < 0) hasUnknownSettings_ = true;
    mode_->setCurrentIndex(qMax(0, modeIndex));
    listen_->setText(fieldValue("Interface", "Listen").isEmpty() ? "127.0.0.1:1080" : fieldValue("Interface", "Listen"));
    interfaceName_->setText(fieldValue("Interface", "Name").isEmpty() ? "Bimeron" : fieldValue("Interface", "Name"));
    const int mtu = fieldValue("Interface", "MTU").toInt();
    mtu_->setValue(mtu ? mtu : 1420);
    dns_->setText(fieldValue("Interface", "DNS"));
    dohEndpoints_ = documentFields_.value("Interface.DoH").toString(fieldValue("Interface", "DoH"));
    dohBootstrap_ = documentFields_.value("Interface.DoHBootstrap").toString(fieldValue("Interface", "DoHBootstrap"));
    refreshDohSummary();
    if (hasDohFields(text) && !capabilities_.value("doh").toBool())
        rawOnly_ = true;
    const QString allowedMode = documentFields_.contains("Routing/AllowedIPsMode")
        ? documentFields_.value("Routing/AllowedIPsMode").toString()
        : ProfileStore::decodeOptionValue(routingSectionValue(text, "Routing", "AllowedIPsMode"));
    const int allowedIndex = allowedIpsMode_->findData(allowedMode.isEmpty() ? QStringLiteral("direct") : allowedMode);
    if (allowedIndex < 0) hasUnknownSettings_ = rawOnly_ = true;
    allowedIpsMode_->setCurrentIndex(qMax(0, allowedIndex));
    ipv6_->setCurrentIndex(qMax(0, ipv6_->findData(fieldValue("Interface", "IPv6"))));
    killSwitch_->setChecked(fieldValue("Interface", "KillSwitch").compare("true", Qt::CaseInsensitive) == 0);
    defaultRoute_->setCurrentIndex(qMax(0, defaultRoute_->findData(fieldValue("Routing", "Default"))));
    QString section; Rule rule;
    const auto finishRule = [this](const Rule &finished) {
        if (finished.target.isEmpty()) return;
        if (finished.enabled && finished.matcher == "CIDR" && finished.action == "direct"
            && normalizedCidrs(finished.target) == localNetworkCidrs()) {
            ignoreLocalNetwork_->setChecked(true);
        } else {
            addRule(finished);
        }
    };
    for (const QString &line : text.split('\n')) {
        QString trimmed = line.trimmed();
        const bool commented = trimmed.startsWith('#');
        if (commented) trimmed = trimmed.mid(1).trimmed();
        if (ProfileStore::sectionName(trimmed) == "Rule") { finishRule(rule); rule = {}; rule.enabled = !commented; section = "Rule"; }
        else if (trimmed.startsWith('[')) section.clear();
        else if (section == "Rule" && trimmed.startsWith("CIDR")) { rule.target = trimmed.mid(trimmed.indexOf('=') + 1).trimmed(); rule.matcher = "CIDR"; }
        else if (section == "Rule" && trimmed.startsWith("Domain")) { rule.target = trimmed.mid(trimmed.indexOf('=') + 1).trimmed(); rule.matcher = "Domain"; }
        else if (section == "Rule" && trimmed.startsWith("Action")) rule.action = trimmed.mid(trimmed.indexOf('=') + 1).trimmed();
    }
    finishRule(rule);
    const QHash<QString, QSet<QString>> packetKnown{
        {"Profile", {"Version", "Name"}},
        {"AccessKey", {"SS", "SSConf", "SOCKS5", "VLESS", "Hysteria2", "SSH", "Key", "File", "Type", "Server", "Port", "Method", "Username", "Password", "Prefix", "Plugin", "PluginOpts", "IdentityFile", "KnownHostsFile", "CAFile", "MASQUEPath", "ID", "Encryption", "Flow", "Network", "Security", "ServerName", "PublicKey", "ShortID", "Fingerprint", "SpiderX", "ALPN", "WSPath", "WSHost", "WSEarlyData", "GRPCServiceName", "GRPCAuthority", "GRPCMultiMode", "XHTTPPath", "XHTTPHost", "XHTTPMode", "XHTTPExtra", "VMess", "Trojan", "TUIC", "AnyTLS", "HTTP", "Naive", "MASQUE"}},
        {"ShadowTLS", {"ServerName", "Password"}},
        {"Interface", {"Mode", "Name", "MTU", "DNS", "DoH", "DoHBootstrap", "IPv6", "KillSwitch", "Listen"}},
        {"Routing", {"Default", "AllowedIPsMode"}}, {"Rule", {"CIDR", "Domain", "Action"}}};
    const QHash<QString, QSet<QString>> known = native_
        ? QHash<QString, QSet<QString>>{{"Profile", {"Version", "Name"}},
            {"IKEv2", {"WindowsProfile", "ServerName", "KillSwitch"}}, {"Interface", {"Mode"}}}
        : packetKnown;
    QString currentSection;
    for (const QString &sourceLine : text.split('\n')) {
        const QString line = QString(sourceLine).remove(QChar(0xfeff)).trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = ProfileStore::sectionName(line);
            if (!known.contains(currentSection)) hasUnknownSettings_ = true;
        } else if (line.contains('=')) {
            const QString key = line.left(line.indexOf('=')).trimmed();
            // The VLESS form has no XHTTP extra editor; VMess keeps it in its own field.
            if (!known.value(currentSection).contains(key) || (source == "ExplicitVLESS" && key == "XHTTPExtra")) {
                hasUnknownSettings_ = true;
                if (source == "ExplicitVLESS" && currentSection == "AccessKey") rawOnly_ = true;
            }
        }
    }
    bridgeReady_ = false;
    cachedVisual_.clear(); cachedDocument_.clear(); bridgeError_.clear();
    localRule_ = -1;
    if (!native_ && !coreExecutable_.isEmpty()) {
        const auto result = inspection;
        if (inspectionError.isEmpty()) {
            bridgeReady_ = true;
            documentRules_ = result.value("rules").toArray();
            rules_->setRowCount(0);
            int localCount = 0;
            for (const auto &value : documentRules_) {
                const auto row = value.toObject();
                Rule rule{row.value("value").toString(), row.value("action").toString(), row.value("enabled").toBool(),
                          row.value("matcher").toString(), row.value("index").toInt(), row.value("editable").toBool(), row.value("error").toString()};
                addRule(rule);
                if (rule.matcher == "CIDR" && rule.action == "direct" && normalizedCidrs(rule.target) == localNetworkCidrs()) {
                    ++localCount;
                    if (rule.index == 0 && rule.editable) localRule_ = rule.index;
                }
            }
            if (localCount != 1) localRule_ = -1;
            ignoreLocalNetwork_->setChecked(localRule_ == 0 && !documentRules_.isEmpty() && documentRules_.first().toObject().value("enabled").toBool());
        } else { bridgeError_ = inspectionError; }
    }
    for (const QString &kind : {QString("geoip"), QString("geosite")}) {
        const QString path = GeoDataSelection::instance()->paths().value(kind);
        if (datasetPaths_.value(kind) != path || !datasetGeneration_.contains(kind)) {
            datasetPaths_[kind] = path;
            inspectDataset(kind);
        }
    }
    raw_->setPlainText(text);
    rawDirty_ = false;
    populatingRules_ = false;
}

void ProfileEditorDialog::addRule(const Rule &rule) {
    if (coreExecutable_.isEmpty() && ((rule.matcher == "Domain" && !rules_->property("domainSplit").toBool())
        || (rule.matcher == "CIDR" && !rules_->property("ipSplit").toBool()))) hasUnknownSettings_ = true;
    const int row = rules_->rowCount(); rules_->insertRow(row);
    auto *enabledCell = new QWidget;
    enabledCell->setProperty("documentIndex", rule.index);
    enabledCell->setProperty("editable", rule.editable);
    enabledCell->setProperty("error", rule.error);
    auto *enabledLayout = new QHBoxLayout(enabledCell);
    enabledLayout->setContentsMargins(0, 0, 0, 0);
    enabledLayout->setAlignment(Qt::AlignCenter);
    auto *enabled = new HiDpiCheckBox;
    enabled->setAccessibleName(tr("Rule enabled"));
    enabled->setChecked(rule.enabled);
    enabled->setEnabled(rule.editable || rule.enabled);
    enabledLayout->addWidget(enabled);
    rules_->setCellWidget(row, 0, enabledCell);
    auto *type = new QComboBox;
    type->setObjectName("tableEditor");
    type->setAccessibleName(tr("Rule type"));
    type->addItem(tr("IP / CIDR"), "CIDR");
    type->addItem(tr("Domain"), "Domain");
    type->addItem(tr("Countries (GeoIP)"), "GeoIP");
    type->addItem(tr("Sites (GeoSite)"), "GeoSite");
    if (type->findData(rule.matcher) < 0) type->addItem(rule.matcher, rule.matcher);
    type->setCurrentIndex(type->findData(rule.matcher));
    type->setEnabled(rule.editable);
    rules_->setCellWidget(row, 1, type);
    rules_->horizontalHeader()->resizeSection(1, qMax(rules_->columnWidth(1), type->sizeHint().width()));
    auto *target = new QLineEdit(rule.target);
    target->setObjectName("tableEditor");
    target->setAccessibleName(tr("Destination"));
    target->setToolTip(rule.error);
    target->setProperty("groupIds", rule.target);
    rules_->setCellWidget(row, 2, target);
    auto *action = new QComboBox;
    action->setObjectName("tableEditor");
    action->setAccessibleName(tr("Action"));
    action->addItem(tr("Via VPN"), "proxy"); action->addItem(tr("Direct"), "direct"); action->addItem(tr("Block"), "block");
    action->setCurrentIndex(qMax(0, action->findData(rule.action)));
    action->setEnabled(rule.editable);
    rules_->setCellWidget(row, 3, action);
    QPixmap arrowIcon(16, 16);
    arrowIcon.fill(Qt::transparent);
    { QPainter painter(&arrowIcon); painter.drawPixmap(3, 6, QPixmap(":/icons/chevron-down.xpm")); }
    auto *choose = new QAction(QIcon(arrowIcon), tr("Choose groups"), target);
    target->addAction(choose, QLineEdit::TrailingPosition);
    const auto updateTarget = [this, type, target, choose, rule] {
        const QString matcher = type->currentData().toString();
        const bool geo = matcher == "GeoIP" || matcher == "GeoSite";
        target->setReadOnly(geo || !rule.editable);
        target->setPlaceholderText(geo ? tr("Choose groups…") : matcher == "Domain"
            ? tr("Domain: example.com") : tr("IP or CIDR: 1.1.1.1, 10.0.0.0/8"));
        const bool wasGeo = target->property("geodata").toBool();
        target->setProperty("geoip", matcher == "GeoIP");
        if (geo) showGroupSummary(target, wasGeo ? target->property("groupIds").toString() : target->text());
        else if (wasGeo) { const QSignalBlocker blocker(target); target->setText(target->property("groupIds").toString()); }
        target->setProperty("geodata", geo);
        choose->setVisible(geo && rule.editable);
        refreshRouting();
    };
    connect(choose, &QAction::triggered, this, [this, target, type] { selectGroups(target, type); });
    auto *open = new QShortcut(QKeySequence("Alt+Down"), target);
    open->setContext(Qt::WidgetShortcut);
    connect(open, &QShortcut::activated, this, [this, target, type] {
        if (target->property("geodata").toBool()) selectGroups(target, type);
    });
    type->setProperty("previousMatcher", rule.matcher);
    connect(type, &QComboBox::currentIndexChanged, this, [this, type, updateTarget] {
        if (!coreExecutable_.isEmpty() && !matcherError(type->currentData().toString()).isEmpty()) {
            const QSignalBlocker blocker(type);
            type->setCurrentIndex(type->findData(type->property("previousMatcher")));
        } else type->setProperty("previousMatcher", type->currentData());
        updateTarget();
    });
    connect(enabled, &QCheckBox::toggled, this, [this, enabled, type](bool checked) {
        if (checked && !coreExecutable_.isEmpty() && !matcherError(type->currentData().toString()).isEmpty()) {
            const QSignalBlocker blocker(enabled); enabled->setChecked(false);
        }
        refreshRouting();
    });
    connect(target, &QLineEdit::textChanged, this, [this] { refreshRouting(); });
    connect(action, &QComboBox::currentIndexChanged, this, [this] { refreshRouting(); });
    updateTarget();
}

ProfileEditorDialog::Rule ProfileEditorDialog::tableRule(int row) const {
    const auto *cell = rules_->cellWidget(row, 0);
    const auto *target = static_cast<QLineEdit *>(rules_->cellWidget(row, 2));
    return {target->property("geodata").toBool() ? target->property("groupIds").toString() : target->text(),
        static_cast<QComboBox *>(rules_->cellWidget(row, 3))->currentData().toString(), ruleEnabled(rules_, row)->isChecked(),
        static_cast<QComboBox *>(rules_->cellWidget(row, 1))->currentData().toString(),
        cell->property("documentIndex").toInt(), cell->property("editable").toBool(), cell->property("error").toString()};
}

void ProfileEditorDialog::moveRule(int delta) {
    const int row = rules_->currentRow(), target = row + delta;
    if (row < 0 || target < 0 || target >= rules_->rowCount()) return;
    QList<Rule> snapshot;
    for (int i = 0; i < rules_->rowCount(); ++i) snapshot.append(tableRule(i));
    snapshot.swapItemsAt(row, target);
    populatingRules_ = true;
    rules_->setRowCount(0);
    for (const auto &rule : snapshot) addRule(rule);
    populatingRules_ = false;
    rules_->selectRow(target);
    refreshRouting();
}

QString ProfileEditorDialog::buildText() const {
    if (native_) {
        QString text = "[Profile]\nVersion = 1\nName = " + name_->text()
            + "\n\n[IKEv2]\nWindowsProfile = " + windowsProfile_->text()
            + "\nServerName = " + nativeServerName_->text();
        if (nativeKillSwitch_->isChecked() || !valueFor(loadedText_, "IKEv2", "KillSwitch").isEmpty())
            text += "\nKillSwitch = " + QString(nativeKillSwitch_->isChecked() ? "true" : "false");
        return text + "\n\n[Interface]\nMode = vpn\n";
    }
    QString accessKey;
    const QString selected = keySource_->currentData().toString();
    if (parameterSource(selected)) {
        accessKey = "Type = " + explicitType(selected) + "\nServer = " + server_->text() + "\nPort = " + QString::number(port_->value());
        if (usesUsername(selected) && !username_->text().isEmpty()) accessKey += "\nUsername = " + username_->text();
        if (selected != "Explicitvmess" && !password_->text().isEmpty()) accessKey += "\nPassword = " + password_->text();
        if (selected == "ExplicitTrojan" && !trojanNetwork_->currentText().isEmpty()) accessKey += "\nNetwork = " + trojanNetwork_->currentText();
        if (selected == "ExplicitnaiveQUIC") accessKey += "\nNetwork = quic";
        // VMess editor fields map to the Core options of the selected transport.
        const QString network = parameters_.value("VMessNetwork")->text();
        const QString mode = parameters_.value("VMessMode")->text();
        const QMap<QString, QString> vmessFields{{"VMessSecurity", "Encryption"}, {"VMessNetwork", "Network"}, {"VMessTLS", "Security"},
            {"VMessPath", vmessPathField(network)}, {"VMessHost", vmessHostField(network)},
            {"VMessMode", network == "grpc" ? "GRPCMultiMode" : "XHTTPMode"}, {"VMessExtra", "XHTTPExtra"}};
        // Serialize inactive configured values too: the validation guard blocks
        // incompatible edits without silently deleting the user's draft.
        for (auto it = parameters_.cbegin(); it != parameters_.cend(); ++it) {
            if ((freshDraft_ && !parameterFields().contains(it.key())) || it.value()->text().isEmpty()) continue;
            QString key = it.key(), value = it.value()->text();
            if (selected == "Explicitvmess" && vmessFields.contains(key)) {
                key = vmessFields.value(key);
                if (key == "GRPCMultiMode" && (mode == "multi" || mode == "gun")) value = mode == "multi" ? "true" : "false";
            }
            accessKey += "\n" + key + " = " + value;
        }
    }
    else if (keySource_->currentData().toString() == "ExplicitSSH")
        accessKey = "Type = ssh\nServer = " + server_->text() + "\nPort = " + QString::number(port_->value()) + "\nUsername = " + username_->text();
    else if (keySource_->currentData().toString() == "ExplicitSOCKS5")
        accessKey = "Type = socks5\nServer = " + server_->text()
            + "\nPort = " + QString::number(port_->value())
            + (username_->text().isEmpty() && password_->text().isEmpty() ? QString()
                : "\nUsername = " + username_->text() + "\nPassword = " + password_->text());
    else if (keySource_->currentData().toString() == "Explicit")
        accessKey = "Type = shadowsocks\nServer = " + server_->text()
            + "\nPort = " + QString::number(port_->value())
            + "\nMethod = " + method_->currentText()
            + "\nPassword = " + password_->text()
            + (prefix_->text().isEmpty() ? QString() : "\nPrefix = " + prefix_->text())
            + (ssPlugin_->currentText().isEmpty() ? QString() : "\nPlugin = " + ssPlugin_->currentText())
            + (ssPluginOpts_->text().isEmpty() ? QString() : "\nPluginOpts = " + ssPluginOpts_->text());
    else if (keySource_->currentData().toString() == "ExplicitVLESS") {
        const QString network = vlessNetwork_->currentText();
        accessKey = "Type = vless\nServer = " + server_->text()
            + "\nPort = " + QString::number(port_->value())
            + "\nID = " + vlessId_->text()
            + "\nEncryption = none\nNetwork = " + network
            + "\nSecurity = " + vlessSecurity_->currentText();
        const auto append = [&accessKey](const QString &field, const QString &value) {
            if (!value.isEmpty()) accessKey += "\n" + field + " = " + value;
        };
        append("Flow", vlessFlow_->currentText());
        append("ServerName", vlessServerName_->text());
        append("Fingerprint", vlessFingerprint_->currentText());
        append("PublicKey", vlessPublicKey_->text());
        append("ShortID", vlessShortId_->text());
        append("SpiderX", vlessSpiderX_->text());
        append("ALPN", vlessAlpn_->text());
        if (network == "ws" || network == "xhttp") {
            append(network == "ws" ? "WSPath" : "XHTTPPath", vlessPath_->text());
            append(network == "ws" ? "WSHost" : "XHTTPHost", vlessHost_->text());
        }
        if (vlessEarlyData_->value()) append("WSEarlyData", QString::number(vlessEarlyData_->value()));
        {
            append("GRPCServiceName", vlessServiceName_->text());
            append("GRPCAuthority", vlessAuthority_->text());
            if (vlessMultiMode_->isChecked() || vlessMultiModeConfigured_)
                append("GRPCMultiMode", vlessMultiMode_->isChecked() ? "true" : "false");
        }
        if (network == "xhttp") append("XHTTPMode", vlessXhttpMode_->currentText());
    }
    else if (selected == "WireGuard")
        // A new pair has no key file yet; storing it replaces this placeholder name.
        accessKey = "File = " + (creating_ || key_->text().isEmpty() ? QStringLiteral("wireguard.conf") : key_->text());
    else
        accessKey = keySource_->currentData().toString() + " = " + key_->text();
    const QString source = keySource_->currentData().toString();
    if (source == "SSH" || source == "ExplicitSSH" || (source == "Key" && QUrl(key_->text()).scheme() == "ssh")) {
        if (!sshIdentity_->text().isEmpty()) accessKey += "\nIdentityFile = " + sshIdentity_->text();
        if (!sshKnownHosts_->text().isEmpty()) accessKey += "\nKnownHostsFile = " + sshKnownHosts_->text();
    }
    accessKey += "\n\n" + ProfileTextEdit::shadowBlock(shadowTls_->isChecked(), shadowName_->text(), shadowPassword_->text(), "\n");
    QString text = "[Profile]\nVersion = 1\nName = " + name_->text()
        + "\n\n[AccessKey]\n" + accessKey
        + "\n\n[Interface]\nMode = " + mode_->currentData().toString()
        + "\nName = " + interfaceName_->text()
        + "\nMTU = " + (isWireGuard() ? valueFor(loadedText_, "Interface", "MTU") : QString::number(mtu_->value()))
        + "\nDNS = " + (isWireGuard() ? valueFor(loadedText_, "Interface", "DNS") : dns_->text())
        + (dohEndpoints_.isEmpty() ? QString() : "\nDoH = " + dohEndpoints_)
        + (dohBootstrap_.isEmpty() ? QString() : "\nDoHBootstrap = " + dohBootstrap_)
        + "\nIPv6 = " + ipv6_->currentData().toString()
        + "\nKillSwitch = " + QString(killSwitch_->isChecked() ? "true" : "false")
        + "\nListen = " + listen_->text()
        + "\n\n[Routing]\nDefault = " + defaultRoute_->currentData().toString() + "\n";
    const QString allowedMode = allowedIpsMode_->currentData().toString();
    if (isWireGuard() && (allowedMode != "direct" || !valueFor(loadedText_, "Routing", "AllowedIPsMode").isEmpty()))
        text += "AllowedIPsMode = " + allowedMode + "\n";
    if (!bridgeReady_ && ignoreLocalNetwork_->isChecked())
        text += QString("\n[Rule]\nCIDR = %1\nAction = direct\n").arg(localNetworkCidrs());
    for (int row = 0; row < rules_->rowCount(); ++row) {
        const QString prefix = ruleEnabled(rules_, row)->isChecked() ? QString() : "# ";
        const QString type = static_cast<QComboBox *>(rules_->cellWidget(row, 1))->currentData().toString();
        const QString target = tableRule(row).target;
        text += QString("\n%1[Rule]\n%1%2 = %3\n%1Action = %4\n")
            .arg(prefix, type, type == "CIDR" ? normalizedCidrs(target) : target.trimmed(), static_cast<QComboBox *>(rules_->cellWidget(row, 3))->currentData().toString());
    }
    QString canonical, section;
    for (const QString &line : ProfileTextEdit::lines(text)) {
        const QString h = ProfileTextEdit::header(line);
        if (!h.isEmpty()) section = h;
        const int eq = line.indexOf('=');
        if (eq > 0 && section != "ShadowTLS") {
            QString key = line.left(eq).trimmed();
            QString value = line.mid(eq + 1);
            if (value.startsWith(' ')) value.remove(0, 1);
            if (value.endsWith('\n')) value.chop(1);
            if (!value.isEmpty()) canonical += key + " = " + ProfileTextEdit::encode(value) + "\n";
        } else canonical += line;
    }
    return canonical;
}

void ProfileEditorDialog::assessRepresentation() {
    if (freshDraft_) {
        // A new draft is never native; hide the IKEv2 rows the form starts with.
        auto *general = qobject_cast<QFormLayout *>(name_->parentWidget()->layout());
        for (QWidget *widget : QList<QWidget *>{windowsProfile_, nativeServerName_, nativeKillSwitch_,
                                                nativeMode_, nativeProfileActions_, nativeAuthentication_})
            setFormRowVisible(general, widget, false);
        rawOnly_ = false;
        bridgeReady_ = false;
        visualBaseline_ = buildText();
        return;
    }
    const QSignalBlocker tabBlocker(tabs_);
    auto *generalPage = name_->parentWidget();
    auto *scroll = qobject_cast<QScrollArea *>(tabs_->widget(0));
    const int currentTab = tabs_->currentIndex();
    if (native_ && !scroll) {
        tabs_->removeTab(0);
        scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(generalPage);
        generalPage->setAutoFillBackground(false);
        scroll->viewport()->setAutoFillBackground(false);
        scroll->setAutoFillBackground(false);
        tabs_->insertTab(0, scroll, tr("General"));
    } else if (!native_ && scroll) {
        scroll->takeWidget();
        tabs_->removeTab(0);
        tabs_->insertTab(0, generalPage, tr("General"));
        delete scroll;
    }
    tabs_->setCurrentIndex(currentTab);
    auto *general = qobject_cast<QFormLayout *>(name_->parentWidget()->layout());
    for (int row = 0; row < general->rowCount(); ++row) setFormRowVisible(general, row, true);
    for (int i = 1; i < tabs_->count() - 1; ++i) tabs_->setTabVisible(i, true);
    if (refreshControls_) refreshControls_();
    setFormRowVisible(general, windowsProfile_, native_);
    setFormRowVisible(general, nativeServerName_, native_);
    setFormRowVisible(general, nativeKillSwitch_, native_);
    setFormRowVisible(general, nativeMode_, native_);
    setFormRowVisible(general, nativeProfileActions_, native_);
    setFormRowVisible(general, nativeAuthentication_, native_);
    nativeAuthMethod_->setCurrentIndex(0);
    nativeAuthSource_->setCurrentIndex(0);
    refreshNativeAuthentication_();
    if (native_) {
        for (int row = 0; row < general->rowCount(); ++row) {
            auto *item = general->itemAt(row, QFormLayout::FieldRole);
            auto *widget = item ? item->widget() : nullptr;
            setFormRowVisible(general, row, widget == name_ || widget == windowsProfile_
                || widget == nativeServerName_ || widget == nativeKillSwitch_ || widget == nativeMode_
                || widget == nativeProfileActions_ || widget == nativeAuthentication_);
        }
    }
    for (int i = 1; i < tabs_->count() - 1; ++i)
        if (native_) tabs_->setTabVisible(i, false);
    visualBaseline_ = buildText();
    if (bridgeReady_) {
        // Older adapters expose only decoded rules. Keep quoted section values
        // Raw-only rather than displaying Go string syntax as credential data.
        if (documentFields_.isEmpty()) {
            QString section;
            for (QString line : loadedText_.split('\n')) {
                line = line.trimmed();
                if (line.startsWith(QChar::ByteOrderMark)) line.remove(0, 1);
                if (line.startsWith('[') && line.endsWith(']')) section = line.mid(1, line.size() - 2);
                else if (section != "Rule" && !line.startsWith('#') && !line.startsWith(';') && line.contains('='))
                    if (line.mid(line.indexOf('=') + 1).trimmed().startsWith('"')) rawOnly_ = true;
            }
        }
        rawOnly_ = rawOnly_ || !bridgeReady_ || !parameterError().isEmpty();
        const int rawIndex = tabs_->count() - 1;
        for (int i = 0; i < rawIndex; ++i) tabs_->setTabEnabled(i, !rawOnly_);
        if (rawOnly_) tabs_->setCurrentIndex(rawIndex);
        refreshRouting();
        refreshWireGuard();
        return;
    }
    // This is a conservative serializer guard, not a second Core validator.
    // Compare configured tokens with the actual serializer, including ordered rules.
    // Native IKEv2 editing predates the decoded-value contract and has no
    // quote-preserving serializer, so any quoted native value stays Raw-only.
    const auto tokens = [native = native_](const QString &text, bool *valid) {
        QMap<QString, QString> result;
        QSet<QString> sections;
        QString section;
        int rule = 0;
        for (QString line : text.split('\n')) {
            line = line.trimmed();
            if (line.startsWith(QChar(0xfeff))) line.remove(0, 1);
            if (line.isEmpty() || line.startsWith(';')) continue;
            if (line.startsWith('#')) {
                line = line.mid(1).trimmed();
                if (ProfileStore::sectionName(line) != "Rule" && !section.startsWith("DisabledRule")) continue;
                if (ProfileStore::sectionName(line) == "Rule") {
                    section = "DisabledRule" + QString::number(++rule);
                    result.insert(section + "/", {});
                    continue;
                }
            }
            if (line.startsWith('[') && line.endsWith(']')) {
                section = ProfileStore::sectionName(line);
                if (section == "Rule") section += QString::number(++rule);
                else if (sections.contains(section)) *valid = false;
                sections.insert(section);
                result.insert(section + "/", {});
                continue;
            }
            const auto equal = line.indexOf('=');
            if (section.isEmpty() || equal < 1) { *valid = false; continue; }
            const QString key = section + "/" + line.left(equal).trimmed();
            QString value;
            if (!ProfileTextEdit::decode(line.mid(equal + 1), &value)) *valid = false;
            if (native && line.mid(equal + 1).trimmed().startsWith('"')) *valid = false;
            if (result.contains(key)) *valid = false;
            result.insert(key, value);
        }
        return result;
    };
    bool valid = true;
    const QString source = keySource_->currentData().toString();
    if (source == "Key" && !QStringList{"ss", "ssconf", "socks5", "vless", "hysteria2", "hy2", "ssh"}
            .contains(QUrl(key_->text()).scheme())) valid = false;
    if ((source == "SSH" || (source == "Key" && QUrl(key_->text()).scheme() == "ssh"))
        && !ProfileStore::connectionInfo(loadedText_).connectable) valid = false;
    const auto original = tokens(loadedText_, &valid);
    const auto generated = tokens(visualBaseline_, &valid);
    for (auto it = original.begin(); it != original.end(); ++it) {
        if (it.value().isEmpty() && QStringList{"Profile/Name", "AccessKey/Prefix"}.contains(it.key()) && !generated.contains(it.key())) continue;
        if (!generated.contains(it.key()) || generated.value(it.key()) != it.value()) valid = false;
    }
    // Omitted settings may only acquire the documented Core default, never the
    // first available GUI/capability choice. Unknown defaults require Raw. An
    // empty DNS field stays omitted, so DNS needs no entry here.
    const QMap<QString, QString> defaults{
        {"Profile/Version", "1"}, {"Profile/Name", ""}, {"AccessKey/Prefix", ""},
        {"Interface/Mode", "vpn"}, {"Interface/Name", "Bimeron"}, {"Interface/MTU", "1420"},
        {"Interface/IPv6", "enable"},
        {"Interface/KillSwitch", "false"}, {"Interface/Listen", "127.0.0.1:1080"},
        {"Routing/Default", "proxy"}
    };
    for (auto it = generated.begin(); it != generated.end(); ++it) {
        if (original.contains(it.key())) continue;
        if (QStringList{"Profile/", "AccessKey/", "Interface/", "Routing/"}.contains(it.key())) continue;
        if (!defaults.contains(it.key()) || defaults.value(it.key()) != it.value()) valid = false;
    }
    rawOnly_ = rawOnly_ || hasUnknownSettings_ || !valid || (!native_ && !parameterError().isEmpty());
    // Missing native values are editable drafts; selected-Core validation still
    // guards persistence. Raw-only is reserved for unrepresentable documents.
    const int rawIndex = tabs_->count() - 1;
    for (int i = 0; i < rawIndex; ++i) tabs_->setTabEnabled(i, !rawOnly_);
    tabs_->widget(1)->setEnabled(!rawOnly_ && keySource_->currentData().toString() == "ExplicitVLESS");
    if (rawOnly_) {
        const QSignalBlocker blocker(tabs_);
        tabs_->setCurrentIndex(rawIndex);
    }
    refreshWireGuard();
}

bool ProfileEditorDialog::applyRaw() {
    const QString text = configText();
    CoreFailure failure;
    bool valid = (validateRaw_ || validatePair_);
    if (valid && isWireGuard()) {
        // The Bimeron draft is checked against the last WG/AWG text Core accepted
        // while the field draft is refused, and problems that belong to the WG/AWG
        // document are corrected on its own tabs; neither blocks leaving this tab.
        wireGuardText();
        // A new draft has no accepted WG/AWG text yet; Save validates the pair later.
        valid = wgBridgeError_.isEmpty() ? validateDraft(text, &failure)
            : wgLoaded_.isEmpty() || validateDraft(text, &failure, &wgLoaded_);
        if (!valid && !failure.problems.isEmpty()) {
            bool wireGuardOnly = true;
            for (const auto &problem : std::as_const(failure.problems)) wireGuardOnly = wireGuardOnly && problem.document == "wireguard";
            if (wireGuardOnly) { valid = true; failure.clear(); }
        }
    } else if (valid) {
        valid = validateDraft(text, &failure);
    }
    if (!valid) {
        if (failure.isEmpty()) failure.text = tr("The selected Core CLI must validate this configuration before visual editing.");
        showProblems(failure);
        return false;
    }
    freshDraft_ = false;
    loadText(text);
    assessRepresentation();
    if (rawOnly_) {
        QMessageBox::information(this, tr("Advanced settings"),
            tr("This configuration cannot be represented without loss. Continue editing Raw config."));
        return false;
    }
    return true;
}

void ProfileEditorDialog::syncRaw() {
    const QString text = configText();
    if (bridgeReady_ && !bridgeError_.isEmpty() && !rawOnly_) {
        routingStatus_->setText(bridgeError_);
        findChild<QScrollArea *>("routingStatusScroll")->show();
        return;
    }
    const QSignalBlocker blocker(raw_);
    raw_->setPlainText(text);
}

QString ProfileEditorDialog::configText() const {
    const QString draft = draftConfigText();
    return isWireGuard() ? ProfileTextEdit::withoutWireGuardNetworkOverrides(
        draft, hasDohFields(draft) || hasDohFields(originalText_)) : draft;
}

QString ProfileEditorDialog::draftConfigText() const {
    if (rawDirty_) {
        // QPlainTextEdit normalizes line endings; untouched Raw retains the source.
        QString normalized = loadedText_;
        normalized.replace("\r\n", "\n");
        normalized.replace('\r', '\n');
        if (raw_->toPlainText() == normalized) return loadedText_;
        return raw_->toPlainText();
    }
    if (freshDraft_) return buildText();
    if (rawOnly_) return loadedText_;
    if (bridgeReady_) return documentText();
    if (buildText() == visualBaseline_) return loadedText_;
    const QString visual = buildText();
    const auto ruleText = [](const QString &source) {
        QString result; bool inRule = false;
        for (const QString &line : ProfileTextEdit::lines(source)) {
            const QString header = ProfileTextEdit::header(line);
            if (!header.isEmpty()) inRule = header == "Rule";
            if (inRule) result += line;
        }
        return result;
    };
    if (ruleText(visual) != ruleText(visualBaseline_)) {
        const QString wrapper = ProfileTextEdit::replaceShadow(loadedText_, shadowTls_->isChecked(), shadowName_->text(), shadowPassword_->text());
        return ProfileTextEdit::copyShadowBlock(visual, wrapper);
    }
    QString edited = loadedText_;
    const auto before = visualFields(visualBaseline_), after = visualFields(visual);
    for (auto it = before.cbegin(); it != before.cend(); ++it) {
        if (!it.key().startsWith("ShadowTLS/") && !after.contains(it.key())) {
            const int slash = it.key().indexOf('/');
            edited = ProfileTextEdit::field(edited, it.key().left(slash), it.key().mid(slash + 1), {});
        }
    }
    for (auto it = after.cbegin(); it != after.cend(); ++it) {
        if (it.key().startsWith("ShadowTLS/") || (before.contains(it.key()) && before.value(it.key()) == it.value())) continue;
        const int slash = it.key().indexOf('/');
        edited = ProfileTextEdit::field(edited, it.key().left(slash), it.key().mid(slash + 1), it.value());
    }
    const auto shadow = ProfileTextEdit::shadow(loadedText_);
    if (shadow.enabled != shadowTls_->isChecked() || shadow.name != shadowName_->text() || shadow.password != shadowPassword_->text())
        edited = ProfileTextEdit::replaceShadow(edited, shadowTls_->isChecked(), shadowName_->text(), shadowPassword_->text());
    return edited;
}

void ProfileEditorDialog::reject() {
    bool reordered = false;
    int previousIndex = -1;
    for (int row = 0; row < rules_->rowCount(); ++row) {
        const int index = tableRule(row).index;
        if (index >= 0 && index < previousIndex) reordered = true;
        if (index >= 0) previousIndex = index;
    }
    const bool wireGuardChanged = isWireGuard() && (creating_ ? wgEdited_
        : (wireGuardText() != wgOriginal_ || !wgBridgeError_.isEmpty()));
    if (reordered || wireGuardChanged || buildText() != visualBaseline_ || (freshDraft_ ? rawDirty_ : draftConfigText() != originalText_)) {
        QMessageBox prompt(QMessageBox::Question, tr("Discard changes"), tr("Discard unsaved changes?"), QMessageBox::NoButton, this);
        auto *discard = prompt.addButton(tr("Discard changes"), QMessageBox::DestructiveRole);
        prompt.addButton(tr("Continue editing"), QMessageBox::RejectRole);
        prompt.exec();
        if (prompt.clickedButton() != discard) return;
    }
    QDialog::reject();
}


void ProfileEditorDialog::setDeveloperMode(bool enabled) {
    developerMode_ = enabled;
    if (refreshControls_) refreshControls_();
}

void ProfileEditorDialog::ensureSourceItem(const QString &source) {
    if (creating_ || keySource_->findData(source) >= 0) return;
    if (linkSources().contains(source)) keySource_->addItem(tr("%1 link").arg(source), source);
    else if (source == "Key") keySource_->addItem(tr("Legacy key"), source);
    else if (source == "File") keySource_->addItem(tr("Key file"), source);
}

// Core rewrites only the [AccessKey] source of the current draft and verifies
// that the connection is unchanged. Nothing is stored until the user saves.
void ProfileEditorDialog::convertAccess(bool toFields) {
    const QString draft = configText();
    QString error;
    QJsonObject result;
    if (draft.toUtf8().size() > 65536) {
        error = tr("Profile exceeds the Core document limit. Use Raw config.");
    } else {
        const QJsonObject request{{"version", 1}, {"profile", draft},
            {"operations", QJsonArray{QJsonObject{{"op", toFields ? "access_to_fields" : "access_to_uri"}}}}};
        result = bridgeJson(coreExecutable_, {"document"}, QJsonDocument(request).toJson(QJsonDocument::Compact), &error);
        if (error.isEmpty() && !result.value("profile").isString())
            error = tr("Core document bridge returned an invalid response. The draft has been retained.");
    }
    if (!error.isEmpty()) {
        QStringList lines;
        const auto problems = parseCoreProblems(result.value("problems"));
        const CoreErrorCatalog catalog = CoreErrorCatalog::forExecutable(coreExecutable_);
        for (const auto &problem : problems) lines << describeCoreProblem(problem, catalog, problemLabel(problemWidget(problem)));
        QMessageBox box(QMessageBox::Warning, tr("Conversion failed"),
            lines.isEmpty() ? error : tr("The draft has been retained.") + "\n\n" + lines.join("\n"), QMessageBox::Ok, this);
        box.setObjectName("accessConversionFailed");
        box.setTextFormat(Qt::PlainText);
        box.exec();
        return;
    }
    loadText(result.value("profile").toString());
    assessRepresentation();
    syncRaw();
}

QJsonObject ProfileEditorDialog::document(const QJsonArray &operations, QString *error) const {
    const QJsonObject request{{"version", 1}, {"profile", loadedText_}, {"operations", operations}};
    if (loadedText_.toUtf8().size() > 65536) {
        *error = tr("Profile exceeds the Core document limit. Use Raw config."); return {};
    }
    const auto result = bridgeJson(coreExecutable_, {"document"}, QJsonDocument(request).toJson(QJsonDocument::Compact), error);
    if (error->isEmpty() && (!result.value("profile").isString() || !result.value("rules").isArray()))
        *error = tr("Core document bridge returned an invalid response. The draft has been retained.");
    return result;
}

QString ProfileEditorDialog::documentText() const {
    const QString visual = buildText();
    QString cacheKey = visual;
    for (auto it = datasetPaths_.cbegin(); it != datasetPaths_.cend(); ++it) cacheKey += '\n' + it.key() + '=' + it.value();
    // The same text can contain reordered identical rules with different attached comments.
    for (int i = 0; i < rules_->rowCount(); ++i) cacheKey += '\n' + QString::number(tableRule(i).index);
    bridgeError_.clear();
    if (cacheKey == cachedVisual_) return cachedDocument_;
    QJsonArray operations;
    QSet<int> changedRules;
    const auto before = visualFields(visualBaseline_), after = visualFields(visual);
    QMap<QString, QString> changes;
    for (auto it = before.cbegin(); it != before.cend(); ++it)
        if (!after.contains(it.key()) && !it.key().startsWith("ShadowTLS/")) changes.insert(it.key(), {});
    for (auto it = after.cbegin(); it != after.cend(); ++it) {
        if (it.key().startsWith("ShadowTLS/")) continue;
        if (!before.contains(it.key()) || before.value(it.key()) != it.value()) changes.insert(it.key(), it.value());
    }
    const bool endpointsChanged = changes.remove("Interface/DoH") > 0;
    const bool bootstrapChanged = changes.remove("Interface/DoHBootstrap") > 0;
    if (endpointsChanged || bootstrapChanged) {
        QJsonObject operation{{"op", "set_doh"}};
        if (!dohEndpoints_.isEmpty()) {
            operation.insert("value", dohEndpoints_);
            operation.insert("bootstrap", dohBootstrap_);
        }
        operations.append(operation);
    }
    for (auto it = datasetPaths_.cbegin(); it != datasetPaths_.cend(); ++it) {
        bool used = false;
        for (int row = 0; row < rules_->rowCount(); ++row) {
            const auto rule = tableRule(row);
            used |= rule.enabled && rule.matcher == (it.key() == "geoip" ? "GeoIP" : "GeoSite");
        }
        if (!used || it.value().isEmpty()) continue;
        const QString key = it.key() == "geoip" ? "GeoIPFile" : "GeoSiteFile";
        if (it.value() != (documentFields_.contains("GeoData/" + key) ? documentFields_.value("GeoData/" + key).toString() : valueFor(loadedText_, "GeoData", key)))
            changes.insert("GeoData/" + key, it.value());
    }
    QList<int> order;
    QSet<int> retained;
    for (int i = 0; i < documentRules_.size(); ++i) order.append(i);
    for (int i = 0; i < rules_->rowCount(); ++i) if (tableRule(i).index >= 0) retained.insert(tableRule(i).index);
    for (int i = order.size() - 1; i >= 0; --i) {
        if (!retained.contains(order[i])) {
            operations.append(QJsonObject{{"op", "delete"}, {"index", i}});
            order.removeAt(i);
        }
    }
    for (int row = 0; row < rules_->rowCount(); ++row) {
        const Rule rule = tableRule(row);
        const QString target = rule.matcher == "CIDR" ? normalizedCidrs(rule.target) : rule.target.trimmed();
        if (rule.index < 0) {
            changedRules.insert(row);
            operations.append(QJsonObject{{"op", "add"}, {"index", row}, {"matcher", rule.matcher},
                              {"value", target}, {"action", rule.action}, {"enabled", rule.enabled}});
            order.insert(row, -1);
            continue;
        }
        const int from = order.indexOf(rule.index);
        if (from != row) {
            operations.append(QJsonObject{{"op", "move"}, {"index", from}, {"to", row}});
            order.move(from, row);
        }
        const auto old = documentRules_.at(rule.index).toObject();
        // Do not normalize an untouched imported value.
        if (rule.matcher != old.value("matcher").toString() || rule.target != old.value("value").toString()
            || rule.action != old.value("action").toString())
            changedRules.insert(row);
        if (changedRules.contains(row))
            operations.append(QJsonObject{{"op", "set_rule"}, {"index", row}, {"matcher", rule.matcher}, {"value", target}, {"action", rule.action}});
        if (rule.enabled != old.value("enabled").toBool())
            operations.append(QJsonObject{{"op", "toggle"}, {"index", row}, {"enabled", rule.enabled}});
    }
    QString edited = loadedText_;
    if (!operations.isEmpty()) {
        const auto result = document(operations, &bridgeError_);
        if (!bridgeError_.isEmpty()) return loadedText_;
        edited = result.value("profile").toString();
        int headers = 0;
        for (const QString &line : ProfileTextEdit::lines(edited))
            if (ProfileTextEdit::header(line) == "Rule") ++headers;
        if (!changedRules.isEmpty() && headers != result.value("rules").toArray().size()) {
            bridgeError_ = tr("Unsafe rule fields require Raw config.");
            return loadedText_;
        }
    }
    edited = ProfileTextEdit::normalizeRules(edited, changedRules);
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        const int slash = it.key().indexOf('/');
        edited = ProfileTextEdit::field(edited, it.key().left(slash), it.key().mid(slash + 1), it.value());
    }
    const auto initialShadow = ProfileTextEdit::shadow(loadedText_);
    if (initialShadow.enabled != shadowTls_->isChecked() || initialShadow.name != shadowName_->text()
        || initialShadow.password != shadowPassword_->text())
        edited = ProfileTextEdit::replaceShadow(edited, shadowTls_->isChecked(), shadowName_->text(), shadowPassword_->text());
    cachedVisual_ = cacheKey;
    cachedDocument_ = edited;
    return cachedDocument_;
}

void ProfileEditorDialog::refreshDohSummary() {
    configureDoh_->setText(dohEndpoints_.isEmpty()
        ? tr("Ordinary DNS · Configure…")
        : tr("DoH (%1 resolvers) · Configure…").arg(dohEndpoints_.count(',') + 1));
    configureDoh_->setEnabled(capabilities_.value("doh").toBool());
    configureDoh_->setToolTip(configureDoh_->isEnabled() ? QString()
        : tr("The selected Core does not support DNS-over-HTTPS."));
}

void ProfileEditorDialog::editDoh() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Captured VPN DNS"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *mode = new QComboBox;
    mode->addItems({tr("Ordinary DNS"), tr("DNS over HTTPS")});
    mode->setObjectName("dohMode");
    mode->setCurrentIndex(!dohEndpoints_.isEmpty() || !dohBootstrap_.isEmpty());
    layout->addWidget(mode);
    auto *body = new QScrollArea;
    body->setWidgetResizable(true);
    auto *content = new QWidget;
    auto *fields = new QVBoxLayout(content);
    body->setWidget(content);
    layout->addWidget(body);
    auto *resolvers = new QListWidget;
    resolvers->setObjectName("dohResolvers");
    resolvers->setMaximumHeight(4 * (resolvers->fontMetrics().height() + 6) + 2 * resolvers->frameWidth());
    fields->addWidget(resolvers);
    auto *actionBar = new QWidget;
    auto *actions = new QHBoxLayout(actionBar);
    actions->setContentsMargins(0, 0, 0, 0);
    auto *add = new QPushButton(tr("Add"));
    add->setObjectName("addDohResolver");
    auto *remove = new QPushButton(tr("Remove"));
    auto *up = new QPushButton(tr("Move up"));
    up->setObjectName("moveDohResolverUp");
    auto *down = new QPushButton(tr("Move down"));
    for (auto *button : {add, remove, up, down}) actions->addWidget(button);
    auto *form = new QFormLayout;
    auto *preset = new QComboBox;
    preset->setObjectName("dohPreset");
    preset->addItems({tr("Custom"), QStringLiteral("Cloudflare"), QStringLiteral("Google")});
    auto *url = new QLineEdit;
    url->setObjectName("dohUrl");
    url->setMaxLength(64 * 1024 + 1);
    auto *ips = new DnsAddressEdit(false);
    ips->setObjectName("dohBootstrapIps");
    form->addRow(tr("Preset"), preset);
    form->addRow(tr("HTTPS URL"), url);
    form->addRow(tr("Bootstrap IPs"), ips);
    fields->addLayout(form);
    auto *problem = new QLabel;
    problem->setWordWrap(true);
    layout->addWidget(problem);
    layout->addWidget(actionBar);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    auto endpoints = dohEndpoints_.isEmpty() ? QStringList{} : dohEndpoints_.split(',');
    auto groups = dohBootstrap_.isEmpty() ? QStringList{} : dohBootstrap_.split(',');
    for (auto &endpoint : endpoints) endpoint = endpoint.trimmed();
    for (auto &group : groups) {
        auto tokens = group.split('|');
        for (auto &token : tokens) token = token.trimmed();
        group = tokens.join('|');
    }
    for (int i = 0; i < endpoints.size(); ++i) {
        auto *item = new QListWidgetItem(endpoints.at(i).trimmed(), resolvers);
        auto addresses = groups.value(i).split('|');
        for (auto &address : addresses) address = address.trimmed();
        item->setData(Qt::UserRole, addresses.join(", "));
    }
    const auto refresh = [&] {
        const auto *item = resolvers->currentItem();
        const int row = resolvers->currentRow();
        const QSignalBlocker blockUrl(url), blockIps(ips), blockPreset(preset);
        url->setText(item ? item->text() : QString());
        ips->setText(item ? item->data(Qt::UserRole).toString() : QString());
        preset->setCurrentIndex(0);
        for (QWidget *widget : QList<QWidget *>{url, ips, preset, remove}) widget->setEnabled(item != nullptr);
        add->setEnabled(resolvers->count() < 4);
        up->setEnabled(row > 0);
        down->setEnabled(row >= 0 && row + 1 < resolvers->count());
    };
    connect(resolvers, &QListWidget::currentRowChanged, &dialog, refresh);
    connect(url, &QLineEdit::textChanged, &dialog, [=](const QString &text) {
        if (auto *item = resolvers->currentItem()) item->setText(text);
    });
    connect(ips, &QLineEdit::textChanged, &dialog, [=](const QString &text) {
        if (auto *item = resolvers->currentItem()) item->setData(Qt::UserRole, text);
    });
    connect(preset, &QComboBox::activated, &dialog, [=](int index) {
        if (index == 1) {
            url->setText("https://cloudflare-dns.com/dns-query");
            ips->setText("1.1.1.1, 1.0.0.1");
        } else if (index == 2) {
            url->setText("https://dns.google/dns-query");
            ips->setText("8.8.8.8, 8.8.4.4");
        }
    });
    connect(add, &QPushButton::clicked, &dialog, [&] {
        resolvers->setCurrentItem(new QListWidgetItem(resolvers));
        refresh();
        url->setFocus();
    });
    connect(remove, &QPushButton::clicked, &dialog, [&] {
        delete resolvers->takeItem(resolvers->currentRow());
        refresh();
    });
    const auto move = [&](int delta) {
        const int row = resolvers->currentRow();
        auto *item = resolvers->takeItem(row);
        resolvers->insertItem(row + delta, item);
        resolvers->setCurrentItem(item);
    };
    connect(up, &QPushButton::clicked, &dialog, [&] { move(-1); });
    connect(down, &QPushButton::clicked, &dialog, [&] { move(1); });
    const auto showResolvers = [=](int index) {
        body->setVisible(index != 0);
        actionBar->setVisible(index != 0);
        problem->clear();
    };
    connect(mode, &QComboBox::currentIndexChanged, &dialog, showResolvers);
    showResolvers(mode->currentIndex());
    resolvers->setCurrentRow(0);
    refresh();
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QStringList urls, bootstrap;
        if (mode->currentIndex() != 0) {
            if (resolvers->count() == 0 || resolvers->count() > 4) {
                problem->setText(tr("Enter one to four resolvers."));
                return;
            }
            for (int i = 0; i < resolvers->count(); ++i) {
                const auto *item = resolvers->item(i);
                const auto endpoint = item->text().trimmed();
                const QUrl parsed(endpoint, QUrl::StrictMode);
                const auto input = item->data(Qt::UserRole).toString();
                const auto addresses = DnsAddressEdit::addresses(input);
                QString error = ips->error(input);
                if (addresses.isEmpty()) error = tr("Enter at least one bootstrap IP address.");
                if (!parsed.isValid() || parsed.scheme() != "https" || parsed.host().isEmpty() || parsed.path().isEmpty()
                    || parsed.authority().contains('@') || parsed.hasQuery() || parsed.hasFragment()
                    || endpoint.contains(',') || endpoint.contains('|'))
                    error = tr("Enter an HTTPS URL with a hostname and path.");
                if (!error.isEmpty()) {
                    resolvers->setCurrentRow(i);
                    problem->setText(error);
                    return;
                }
                urls.append(endpoint);
                bootstrap.append(addresses.join('|'));
            }
        }
        if (urls != endpoints || bootstrap != groups) {
            dohEndpoints_ = urls.join(", ");
            dohBootstrap_ = bootstrap.join(", ");
        }
        dialog.accept();
    });
    dialog.resize(520, 350);
    if (dialog.exec() == QDialog::Accepted) refreshDohSummary();
}

QString ProfileEditorDialog::routingError() const {
    if (coreExecutable_.isEmpty()) return {};
    if (rawDirty_ || rawOnly_ || (isWireGuard() && allowedIpsMode_->currentData() == "only")) return {};
    for (int row = 0; row < rules_->rowCount(); ++row) {
        if (!rules_->cellWidget(row, 3)) continue;
        const Rule rule = tableRule(row);
        if (!rule.enabled) continue;
        const QString prefix = tr("Rule %1: ").arg(row + 1);
        if (!rule.editable) return prefix + tr("Unsafe rule fields require Raw config.");
        if (mode_->currentData() == "vpn" && killSwitch_->isChecked() && rule.matcher != "CIDR")
            return prefix + tr("The VPN kill switch permits CIDR rules only. Disable the conflicting rules or explicitly turn off the kill switch.");
        const QString restriction = matcherError(rule.matcher);
        if (!restriction.isEmpty()) return prefix + restriction;
        if (rule.matcher != "GeoIP" && rule.matcher != "GeoSite") continue;
        const QString kind = rule.matcher == "GeoIP" ? "geoip" : "geosite";
        // A missing or failed database does not block saving; connecting is blocked instead.
        if (datasetUnavailable_.value(kind) || !datasetErrors_.value(kind).isEmpty()) continue;
        for (const auto &id : rule.target.split(','))
            if (!routingGroupContains(datasetGroups_.value(kind), id)) return prefix + tr("Dataset group is unavailable: %1").arg(id.trimmed());
    }
    return {};
}

QString ProfileEditorDialog::matcherError(const QString &matcher) const {
    if (routingHasSection(loadedText_, "IKEv2")) return tr("IKEv2 does not support split routing.");
    if (mode_->currentData() == "vpn" && killSwitch_->isChecked() && matcher != "CIDR")
        return tr("The VPN kill switch permits CIDR rules only. Disable the conflicting rules or explicitly turn off the kill switch.");
    const QString flag = matcher == "CIDR" ? "ip_split" : matcher == "Domain" ? "domain_split" : matcher == "GeoIP" ? "geoip_split" : "geosite_split";
    if (!capabilities_.value(flag).toBool()
        || ((matcher == "GeoIP" || matcher == "GeoSite") && capabilities_.value("geodata_schema").toString() != "v2ray-protobuf")
        || (matcher == "GeoSite" && mode_->currentData() == "vpn" && !capabilities_.value("domain_split").toBool()))
        return tr("The current endpoint does not explicitly support this routing type and schema. Keep this rule disabled.");
    if ((matcher == "GeoIP" || matcher == "GeoSite") && !bridgeReady_)
        return tr("Core document bridge is unavailable. Use Raw config.");
    return {};
}

void ProfileEditorDialog::refreshRouting() {
    if (!routingStatus_ || populatingRules_) return;
    killSwitch_->setEnabled(capabilities_.value("kill_switch").toBool() && mode_->currentData() == "vpn");
    if (localRule_ != -1 && bridgeReady_) {
        bool found = false;
        for (int row = 0; row < rules_->rowCount(); ++row) {
            if (!rules_->cellWidget(row, 3)) continue;
            const Rule rule = tableRule(row);
            if (rule.index == localRule_ && rule.matcher == "CIDR" && rule.action == "direct"
                && normalizedCidrs(rule.target) == localNetworkCidrs()) {
                found = true;
                const QSignalBlocker blocker(ignoreLocalNetwork_);
                ignoreLocalNetwork_->setChecked(rule.enabled);
                break;
            }
        }
        if (!found) {
            localRule_ = -1;
            const QSignalBlocker blocker(ignoreLocalNetwork_); ignoreLocalNetwork_->setChecked(false);
        }
    }
    QStringList unavailable;
    for (const QString &matcher : {QString("CIDR"), QString("Domain"), QString("GeoIP"), QString("GeoSite")})
        if (!matcherError(matcher).isEmpty()) unavailable.append(matcher + ": " + matcherError(matcher));
    for (int row = 0; row < rules_->rowCount(); ++row) {
        if (!rules_->cellWidget(row, 3)) continue;
        auto *type = static_cast<QComboBox *>(rules_->cellWidget(row, 1));
        auto *enabled = ruleEnabled(rules_, row);
        if (auto *model = qobject_cast<QStandardItemModel *>(type->model()))
            for (int i = 0; i < type->count(); ++i) model->item(i)->setEnabled(matcherError(type->itemData(i).toString()).isEmpty());
        enabled->setEnabled(enabled->isChecked() || (rules_->cellWidget(row, 0)->property("editable").toBool()
            && matcherError(type->currentData().toString()).isEmpty()));
    }
    // Rules of a kind without a usable database keep Enabled and Type editable, so they can
    // be disabled or changed; their Target is disabled and a note explains the block.
    QStringList blocked;
    for (int row = 0; row < rules_->rowCount(); ++row) {
        if (!rules_->cellWidget(row, 3)) continue;
        auto *type = static_cast<QComboBox *>(rules_->cellWidget(row, 1));
        const QString matcher = type->currentData().toString();
        const QString kind = matcher == "GeoIP" ? "geoip" : matcher == "GeoSite" ? "geosite" : QString();
        const bool unusable = !kind.isEmpty() && datasetUnavailable_.value(kind);
        if (QWidget *target = rules_->cellWidget(row, 2)) target->setEnabled(!unusable);
        if (unusable && ruleEnabled(rules_, row)->isChecked() && !blocked.contains(kind)) blocked << kind;
    }
    QStringList notes;
    for (const QString &kind : blocked) {
        const bool countries = kind == "geoip";
        const bool selected = !datasetPaths_.value(kind).isEmpty();
        notes << (countries
            ? (selected ? tr("The country database failed its check: while a country rule is enabled, the profile will not connect. Choose a database in Manage databases or clear the rule's checkbox.")
                        : tr("No country database is selected: while a country rule is enabled, the profile will not connect. Choose a database in Manage databases or clear the rule's checkbox."))
            : (selected ? tr("The site database failed its check: while a site rule is enabled, the profile will not connect. Choose a database in Manage databases or clear the rule's checkbox.")
                        : tr("No site database is selected: while a site rule is enabled, the profile will not connect. Choose a database in Manage databases or clear the rule's checkbox.")));
    }
    const QString error = routingError();
    QString text = !notes.isEmpty() ? notes.join('\n') : error;
    if (text.isEmpty() && datasetChanged_) text = tr("New databases take effect on the next connection.");
    routingStatus_->setToolTip(unavailable.join('\n'));
    routingStatus_->setText(text);
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    routingStatus_->setStyleSheet(notes.isEmpty() ? QString() : QStringLiteral("color: %1").arg(dark ? "#f3cf72" : "#b54708"));
    auto *statusScroll = findChild<QScrollArea *>("routingStatusScroll");
    statusScroll->setVisible(!notes.isEmpty() && !(isWireGuard() && allowedIpsMode_->currentData() == "only"));
}

void ProfileEditorDialog::inspectDataset(const QString &kind) {
    ++datasetGeneration_[kind];
    datasetGroups_.remove(kind); datasetKeys_.remove(kind);
    const QString path = datasetPaths_.value(kind);
    if (path.isEmpty()) {
        datasetErrors_[kind] = tr("No shared database selected. Open Manage databases to set it up.");
        datasetUnavailable_[kind] = true;
        refreshRouting(); return;
    }
    if (coreExecutable_.isEmpty() || (QDir::isRelativePath(path) && profilePath_.isEmpty())) {
        datasetErrors_[kind] = tr("Select a Core executable and final profile path before validating datasets.");
        datasetUnavailable_[kind] = false;
        refreshRouting(); return;
    }
    const QString absolute = QDir::isAbsolutePath(path) ? QFileInfo(path).absoluteFilePath()
        : QFileInfo(QFileInfo(profilePath_).absoluteDir(), path).absoluteFilePath();
    datasetKeys_[kind] = GeoDataSelection::catalogKey(coreExecutable_, kind, absolute);
    datasetErrors_[kind] = tr("Checking file…");
    datasetUnavailable_[kind] = false;
    refreshRouting();
    GeoDataSelection::instance()->requestCatalog(coreExecutable_, kind, absolute);
}

void ProfileEditorDialog::selectGroups(QLineEdit *target, QComboBox *type) {
    const QString kind = type->currentData() == "GeoIP" ? "geoip" : "geosite";
    if (kind == "geosite") {
        QStringList selected = target->property("groupIds").toString().split(',', Qt::SkipEmptyParts);
        for (auto &id : selected) id = id.trimmed();
        GeoSitePicker popup(this, datasetGroups_.value(kind), selected, datasetErrors_.value(kind));
        if (popup.exec() == QDialog::Accepted) {
            showGroupSummary(target, popup.selectedIds().join(','));
            refreshRouting();
        }
        return;
    }
    QDialog popup(this);
    popup.setWindowTitle(tr("Choose groups"));
    popup.resize(440, 420);
    auto *layout = new QVBoxLayout(&popup);
    auto *search = new QLineEdit;
    search->setPlaceholderText(tr("Search names or IDs"));
    search->setAccessibleName(tr("Search names or IDs"));
    layout->addWidget(search);
    auto *notice = new QLabel(datasetErrors_.value(kind));
    notice->setWordWrap(true); notice->setTextFormat(Qt::PlainText);
    layout->addWidget(notice);
    notice->setVisible(!notice->text().isEmpty());
    auto *list = new QListWidget;
    list->setItemDelegate(new GeoSiteRowDelegate(list));
    list->setFont(QApplication::font());
    list->setStyleSheet(
        "QListWidget::item { min-height: 24px; padding: 2px 4px; border: none; }"
        "QListWidget::indicator { width: 16px; height: 16px; border: 1px solid #b8c6d6; border-radius: 3px; }"
        "QListWidget::indicator:checked { border-color: #1677ff; image: url(:/icons/check.xpm); }");
    list->setAccessibleName(tr("Available groups"));
    layout->addWidget(list);
    QStringList selected = target->property("groupIds").toString().split(',', Qt::SkipEmptyParts);
    for (auto &id : selected) id = id.trimmed();
    QStringList groups = datasetGroups_.value(kind);
    groups = routingGroupRows(groups, selected);
    if (kind == "geoip")
        std::stable_partition(groups.begin(), groups.end(), [](const QString &id) { return !geoIPSpecialName(id).isEmpty(); });
    std::stable_partition(groups.begin(), groups.end(), [&selected](const QString &id) { return routingGroupContains(selected, id); });
    for (const auto &id : groups) {
        QString label = groupLabel(id, kind == "geoip");
        if (!routingGroupContains(datasetGroups_.value(kind), id)) label += " — " + tr("Unavailable");
        auto *item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, id);
        item->setData(Qt::UserRole + 1, kind == "geoip" ? geoIPSearchText(id) : label);
        QString name = geoIPLabel(id, true);
        const QString suffix = " (" + id.toUpper() + ')';
        if (name.endsWith(suffix)) name.chop(suffix.size());
        item->setData(Qt::UserRole + 3, name == id ? QString{} : name);
        item->setData(Qt::UserRole + 4, routingGroupContains(datasetGroups_.value(kind), id) ? QString{} : " — " + tr("Unavailable"));
        item->setData(Qt::UserRole + 7, true);
        item->setToolTip(label);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(routingGroupContains(selected, id) ? Qt::Checked : Qt::Unchecked);
    }
    auto *empty = new QLabel;
    layout->addWidget(empty);
    auto *footer = new QHBoxLayout;
    auto *count = new QPushButton;
    count->setCheckable(true);
    count->setAutoDefault(false);
    count->setAccessibleName(tr("Only selected"));
    count->setToolTip(tr("Show only selected groups; typing searches within the selection"));
    count->setStyleSheet("QPushButton:checked { color: #1677ff; border-color: #1677ff; }");
    footer->addWidget(count);
    footer->addStretch();
    auto filter = [list, search, count, empty] {
        const QSignalBlocker blocker(list);
        int checkedCount = 0, visibleCount = 0;
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            const bool checked = item->checkState() == Qt::Checked;
            checkedCount += checked;
            const bool visible = (!count->isChecked() || checked)
                && (item->text() + '\n' + item->data(Qt::UserRole + 1).toString()).contains(search->text(), Qt::CaseInsensitive);
            item->setHidden(!visible);
            visibleCount += visible;
        }
        count->setText((count->isChecked() ? tr("Selected: %1 · Only selected") : tr("Selected: %1")).arg(checkedCount));
        empty->setText(count->isChecked() ? (search->text().isEmpty() ? tr("No selected groups") : tr("No matching selected groups")) : tr("No matching groups"));
        empty->setVisible(visibleCount == 0);
    };
    connect(search, &QLineEdit::textChanged, list, filter);
    connect(count, &QPushButton::toggled, list, filter);
    connect(list, &QListWidget::itemChanged, list, filter);
    filter();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    footer->addWidget(buttons);
    layout->addLayout(footer);
    connect(buttons, &QDialogButtonBox::accepted, &popup, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &popup, &QDialog::reject);
    search->setFocus();
    if (popup.exec() == QDialog::Accepted) {
        QStringList ids;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() == Qt::Checked) ids.append(list->item(i)->data(Qt::UserRole).toString());
        showGroupSummary(target, routingRetainedGroups(ids, selected).join(','));
        refreshRouting();
    }
}

void ProfileEditorDialog::manageDatabases() {
    QJsonArray active;
    for (int row = 0; row < rules_->rowCount(); ++row) {
        const auto rule = tableRule(row);
        active.append(QJsonObject{{"matcher", rule.matcher}, {"value", rule.target}, {"enabled", rule.enabled}});
    }
    manageSharedGeoData(this, coreExecutable_, profilePath_, active);
}

QDialog *ProfileEditorDialog::nativeConfirmation(QWidget *parent, const QString &profile,
                                                const QString &server, const QString &action, bool reviewable, const QJsonObject &plan) {
    auto *dialog = new QDialog(parent);
    dialog->setWindowTitle(tr("Windows profile"));
    auto *layout = new QVBoxLayout(dialog);
    const bool valid = reviewable && !profile.isEmpty() && !server.isEmpty()
        && (action == "create" || action == "remove" || action == "unchanged");
    auto *summary = new QLabel(!valid ? tr("The Windows profile changes cannot be confirmed.")
        : action == "remove" ? tr("Remove this Windows VPN profile? The local configuration will be kept.")
        : action == "create" ? tr("Create this Windows VPN profile? This will not connect the VPN.")
        : tr("No Windows profile changes are needed."));
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    auto *form = new QFormLayout;
    for (const auto &field : {qMakePair(tr("Windows VPN profile"), profile), qMakePair(tr("Server name"), server)}) {
        auto *value = new QLabel(field.second);
        value->setTextFormat(Qt::PlainText);
        value->setWordWrap(true);
        value->setMaximumWidth(360);
        form->addRow(field.first, value);
    }
    if (plan.isEmpty()) {
        layout->addLayout(form);
    } else {
        const QList<QPair<QString, QString>> fields = {
            {tr("Action"), action}, {tr("Authentication"), plan.value("authentication").toString()},
            {tr("IPsec"), plan.value("ipsec").toString()},
            {tr("IPv6 routes"), [&plan] { QStringList routes; for (const auto &v : plan.value("ipv6_routes").toArray()) routes << v.toString(); return routes.join("\n"); }()},
            {tr("Credentials"), plan.value("credentials").toString()}, {tr("Trust"), plan.value("trust").toString()}};
        for (const auto &field : fields) {
            auto *value = new QLabel(field.second);
            value->setTextFormat(Qt::PlainText);
            value->setWordWrap(true);
            value->setMaximumWidth(360);
            form->addRow(field.first, value);
        }
        auto *content = new QWidget;
        content->setLayout(form);
        auto *scroll = new QScrollArea(dialog);
        scroll->setWidgetResizable(true);
        scroll->setWidget(content);
        scroll->setMinimumHeight(240);
        scroll->setMaximumHeight(420);
        layout->addWidget(scroll);
        dialog->resize(540, 520);
    }
    auto *buttons = new QDialogButtonBox(dialog);
    auto *cancel = buttons->addButton(valid && action != "unchanged" ? QDialogButtonBox::Cancel : QDialogButtonBox::Close);
    cancel->setDefault(true);
    cancel->setFocus();
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    if (valid && action != "unchanged") {
        auto *confirm = buttons->addButton(action == "create" ? tr("Create") : tr("Remove"), QDialogButtonBox::AcceptRole);
        confirm->setAutoDefault(false);
        connect(confirm, &QPushButton::clicked, dialog, [dialog, confirm] {
            confirm->setEnabled(false);
            dialog->accept(); // Local consent only; no Core or OS operation.
        });
    }
    layout->addWidget(buttons);
    return dialog;
}

// --- Core problem presentation ------------------------------------------------
// Maps Core problem locations (docs/ERRORS.MD in Core) to editor widgets. Several
// widgets can represent one canonical field; the first visible one wins, then the
// URI/key field that held the value, then the Raw config line.

namespace {

int tabOf(QTabWidget *tabs, QWidget *widget) {
    for (int i = 0; tabs && widget && i < tabs->count(); ++i)
        if (tabs->widget(i) == widget || tabs->widget(i)->isAncestorOf(widget)) return i;
    return -1;
}

bool shownInEditor(QTabWidget *tabs, QWidget *widget) {
    const int tab = tabOf(tabs, widget);
    return tab >= 0 && tabs->isTabVisible(tab) && widget->isVisibleTo(tabs->widget(tab)) && widget->isEnabled();
}

} // namespace

QWidget *ProfileEditorDialog::problemWidget(const CoreProblem &problem) const {
    if (problem.document == "wireguard") {
        // Collapsed AmneziaWG rows still count: focusProblem expands them.
        QWidget *widget = wgForm_ ? wgForm_->widget(problem.section + '/' + problem.field) : nullptr;
        const int tab = tabOf(tabs_, widget);
        return widget && tab >= 0 && tabs_->isTabVisible(tab) && tabs_->isTabEnabled(tab) && widget->isEnabled() ? widget : nullptr;
    }
    QMultiMap<QString, QWidget *> targets;
    const auto add = [&targets](const QString &key, QWidget *widget) { if (widget) targets.insert(key, widget); };
    // QMultiMap::values returns the most recently inserted value first, so insert
    // generic fallbacks before protocol-specific widgets.
    for (auto it = parameters_.cbegin(); it != parameters_.cend(); ++it) add("AccessKey." + it.key(), it.value());
    add("Profile.Name", name_);
    add("Interface.Mode", mode_);
    add("Interface.Listen", listen_);
    add("Interface.Name", interfaceName_);
    add("Interface.MTU", mtu_);
    add("Interface.DNS", dns_);
    add("Interface.DoH", configureDoh_);
    add("Interface.DoHBootstrap", configureDoh_);
    add("Interface.IPv6", ipv6_);
    add("Interface.KillSwitch", killSwitch_);
    add("Routing.Default", defaultRoute_);
    add("Routing.AllowedIPsMode", allowedIpsMode_);
    add("AccessKey.Type", keySource_);
    add("AccessKey.Server", server_);
    add("AccessKey.Port", port_);
    add("AccessKey.Method", method_);
    add("AccessKey.Password", password_);
    add("AccessKey.Username", username_);
    add("AccessKey.Prefix", prefix_);
    add("AccessKey.Plugin", ssPlugin_);
    add("AccessKey.PluginOpts", ssPluginOpts_);
    add("AccessKey.IdentityFile", sshIdentity_);
    add("AccessKey.KnownHostsFile", sshKnownHosts_);
    add("AccessKey.Network", trojanNetwork_);
    add("AccessKey.ID", vlessId_);
    add("AccessKey.Network", vlessNetwork_);
    add("AccessKey.Security", vlessSecurity_);
    add("AccessKey.Flow", vlessFlow_);
    add("AccessKey.ServerName", vlessServerName_);
    add("AccessKey.Fingerprint", vlessFingerprint_);
    add("AccessKey.PublicKey", vlessPublicKey_);
    add("AccessKey.ShortID", vlessShortId_);
    add("AccessKey.SpiderX", vlessSpiderX_);
    add("AccessKey.ALPN", vlessAlpn_);
    add("AccessKey.WSPath", vlessPath_);
    add("AccessKey.XHTTPPath", vlessPath_);
    add("AccessKey.WSHost", vlessHost_);
    add("AccessKey.XHTTPHost", vlessHost_);
    add("AccessKey.WSEarlyData", vlessEarlyData_);
    add("AccessKey.GRPCServiceName", vlessServiceName_);
    add("AccessKey.GRPCAuthority", vlessAuthority_);
    add("AccessKey.GRPCMultiMode", vlessMultiMode_);
    add("AccessKey.XHTTPMode", vlessXhttpMode_);
    // VMess fields stand for several Core options; hidden widgets are skipped below.
    add("AccessKey.Encryption", parameters_.value("VMessSecurity"));
    add("AccessKey.Network", parameters_.value("VMessNetwork"));
    add("AccessKey.Security", parameters_.value("VMessTLS"));
    for (const QString &network : {QString("ws"), QString("grpc"), QString("xhttp")}) {
        add("AccessKey." + vmessPathField(network), parameters_.value("VMessPath"));
        add("AccessKey." + vmessHostField(network), parameters_.value("VMessHost"));
    }
    add("AccessKey.XHTTPMode", parameters_.value("VMessMode"));
    add("AccessKey.GRPCMultiMode", parameters_.value("VMessMode"));
    add("AccessKey.XHTTPExtra", parameters_.value("VMessExtra"));
    add("ShadowTLS.ServerName", shadowName_);
    add("ShadowTLS.Password", shadowPassword_);
    add("IKEv2.WindowsProfile", windowsProfile_);
    add("IKEv2.ServerName", nativeServerName_);
    add("IKEv2.KillSwitch", nativeKillSwitch_);
    QStringList keys{problem.fieldKey()};
    for (const QString &related : problem.related) keys << problem.section + '.' + related;
    for (const QString &key : keys)
        for (QWidget *widget : targets.values(key))
            if (shownInEditor(tabs_, widget)) return widget;
    if (!problem.source.isEmpty() && shownInEditor(tabs_, key_)) return key_;
    if (problem.section == "AccessKey" && problem.field.isEmpty() && shownInEditor(tabs_, key_)) return key_;
    return nullptr;
}

QString ProfileEditorDialog::problemLabel(QWidget *widget) const {
    if (!widget) return {};
    if (auto *check = qobject_cast<QCheckBox *>(widget)) return check->text();
    for (QWidget *parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
        if (auto *form = qobject_cast<QFormLayout *>(parent->layout())) {
            QWidget *field = widget;
            while (field && field->parentWidget() != parent) field = field->parentWidget();
            if (auto *label = qobject_cast<QLabel *>(form->labelForField(field)))
                return label->text().remove('&').remove(QRegularExpression(QStringLiteral(":\\s*$"))).trimmed();
            if (auto *label = qobject_cast<QLabel *>(form->labelForField(widget)))
                return label->text().remove('&').remove(QRegularExpression(QStringLiteral(":\\s*$"))).trimmed();
        }
    }
    return {};
}

void ProfileEditorDialog::markProblemField(QWidget *widget) {
    if (!widget) return;
    widget->setProperty("coreProblem", true);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    // Clear on any value change: typing, paste, undo or programmatic updates.
    // loadText blocks signals, so it clears all marks explicitly.
    const auto clear = [widget] {
        if (!widget->property("coreProblem").toBool()) return;
        widget->setProperty("coreProblem", false);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    };
    if (auto *edit = qobject_cast<QLineEdit *>(widget)) connect(edit, &QLineEdit::textChanged, widget, clear, Qt::SingleShotConnection);
    else if (auto *combo = qobject_cast<QComboBox *>(widget)) connect(combo, &QComboBox::currentIndexChanged, widget, clear, Qt::SingleShotConnection);
    else if (auto *spin = qobject_cast<QSpinBox *>(widget)) connect(spin, &QSpinBox::valueChanged, widget, clear, Qt::SingleShotConnection);
    else if (auto *check = qobject_cast<QCheckBox *>(widget)) connect(check, &QCheckBox::toggled, widget, clear, Qt::SingleShotConnection);
    else if (auto *text = qobject_cast<QPlainTextEdit *>(widget)) connect(text, &QPlainTextEdit::textChanged, widget, clear, Qt::SingleShotConnection);
}

void ProfileEditorDialog::clearProblemMarks() {
    for (QWidget *widget : findChildren<QWidget *>()) {
        if (!widget->property("coreProblem").toBool()) continue;
        widget->setProperty("coreProblem", false);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
}

void ProfileEditorDialog::focusProblem(const CoreProblem &problem) {
    if (problem.document == "wireguard") {
        QWidget *widget = wgRawDirty_ || wgRawOnly_ ? nullptr : problemWidget(problem);
        if (widget) {
            if (WireGuardForm::amneziaFields().contains(problem.section + '/' + problem.field)) wgForm_->setAmneziaExpanded(true);
            tabs_->setCurrentIndex(tabOf(tabs_, widget));
            for (QWidget *parent = widget->parentWidget(); parent; parent = parent->parentWidget())
                if (auto *scroll = qobject_cast<QScrollArea *>(parent)) { scroll->ensureWidgetVisible(widget); break; }
            markProblemField(widget);
            widget->setFocus(Qt::OtherFocusReason);
            if (auto *edit = qobject_cast<QLineEdit *>(widget)) edit->selectAll();
            return;
        }
        tabs_->setCurrentIndex(tabs_->indexOf(wgRawPage_));
        if (problem.line > 0) {
            const QTextBlock block = wgRaw_->document()->findBlockByNumber(problem.line - 1);
            if (block.isValid()) {
                QTextCursor cursor(block);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                wgRaw_->setTextCursor(cursor);
                wgRaw_->centerCursor();
            }
        }
        wgRaw_->setFocus(Qt::OtherFocusReason);
        return;
    }
    // An unapplied Raw config draft is what Core validated; the visual fields
    // still show the previous values and leaving Raw would re-validate it.
    // Stay in Raw config and select the reported line instead.
    const bool rawDraft = rawOnly_ || rawDirty_;
    if (rawDraft) {
        tabs_->setCurrentIndex(tabs_->count() - 1);
        if (problem.line > 0) {
            const QTextBlock block = raw_->document()->findBlockByNumber(problem.line - 1);
            if (block.isValid()) {
                QTextCursor cursor(block);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                raw_->setTextCursor(cursor);
                raw_->centerCursor();
            }
        }
        raw_->setFocus(Qt::OtherFocusReason);
        return;
    }
    if (QWidget *widget = problemWidget(problem)) {
        tabs_->setCurrentIndex(tabOf(tabs_, widget));
        for (QWidget *parent = widget->parentWidget(); parent; parent = parent->parentWidget())
            if (auto *scroll = qobject_cast<QScrollArea *>(parent)) { scroll->ensureWidgetVisible(widget); break; }
        markProblemField(widget);
        widget->setFocus(Qt::OtherFocusReason);
        if (auto *edit = qobject_cast<QLineEdit *>(widget)) edit->selectAll();
        return;
    }
    if (problem.rule > 0 && rules_ && shownInEditor(tabs_, rules_)) {
        // Core numbers active rules only; disabled rows are not counted.
        int active = 0;
        for (int row = 0; row < rules_->rowCount(); ++row) {
            if (!tableRule(row).enabled || ++active != problem.rule) continue;
            tabs_->setCurrentIndex(tabOf(tabs_, rules_));
            rules_->setCurrentCell(row, 2);
            if (QWidget *target = rules_->cellWidget(row, 2)) {
                markProblemField(target);
                target->setFocus(Qt::OtherFocusReason);
            }
            return;
        }
    }
    if (problem.line > 0 && raw_) {
        tabs_->setCurrentIndex(tabs_->count() - 1);
        const QTextBlock block = raw_->document()->findBlockByNumber(problem.line - 1);
        if (block.isValid()) {
            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            raw_->setTextCursor(cursor);
            raw_->centerCursor();
        }
        raw_->setFocus(Qt::OtherFocusReason);
    }
}

void ProfileEditorDialog::showProblems(const CoreFailure &failure) {
    bool inline_ = !failure.problems.isEmpty() && wgForm_ && !(wgRawDirty_ || wgRawOnly_);
    for (const auto &problem : failure.problems) inline_ = inline_ && problem.document == "wireguard";
    if (inline_ && problemWidget(failure.problems.first()) && showWireGuardFieldProblems(failure.problems)) {
        const CoreErrorCatalog catalog = CoreErrorCatalog::forExecutable(coreExecutable_);
        QStringList lines;
        for (const auto &problem : failure.problems) lines << describeCoreProblem(problem, catalog, problemLabel(problemWidget(problem)));
        wgForm_->setStatus(lines.join('\n'), true);
        focusProblem(failure.problems.first());
        return;
    }
    if (failure.problems.isEmpty()) {
        QMessageBox box(QMessageBox::Warning, tr("Error"), safeCoreDiagnostic(failure.text.toUtf8()), QMessageBox::Ok, this);
        box.setTextFormat(Qt::PlainText);
        box.exec();
        return;
    }
    const CoreErrorCatalog catalog = CoreErrorCatalog::forExecutable(coreExecutable_);
    QDialog dialog(this);
    dialog.setObjectName("coreProblemDialog");
    dialog.setWindowTitle(tr("Cannot save profile"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *message = new QLabel;
    message->setObjectName("coreProblemText");
    message->setTextFormat(Qt::PlainText);
    message->setWordWrap(true);
    message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    message->setMinimumWidth(420);
    layout->addWidget(message);
    // Secret values are masked; Show reveals the value from this editor only.
    QWidget *secretWidget = nullptr;
    QString secretValue;
    bool revealed = false;
    const CoreProblem &first = failure.problems.first();
    QWidget *firstWidget = problemWidget(first);
    if (catalog.isSecret(first.field) && first.value == "present") {
        if (auto *edit = qobject_cast<QLineEdit *>(firstWidget)) { secretWidget = edit; secretValue = edit->text(); }
    }
    const auto render = [&] {
        QStringList lines;
        for (const auto &problem : failure.problems) {
            QWidget *widget = problemWidget(problem);
            QString label = problemLabel(widget);
            if (label.isEmpty() && problem.rule > 0) label = tr("Rule %1").arg(problem.rule);
            if (label.isEmpty() && problem.document.isEmpty() && problem.fieldKey() == "AccessKey.File" && isWireGuard())
                label = tr("WireGuard configuration file");
            QString line = describeCoreProblem(problem, catalog, label);
            if (revealed && &problem == &first && secretWidget) line.replace(coreSecretMask(), secretValue);
            if (!widget && problem.rule == 0 && problem.line > 0)
                line += '\n' + (problem.document == "wireguard" ? tr("Line %1 of the WG/AWG config.").arg(problem.line)
                                                                 : tr("Line %1 of the profile.").arg(problem.line));
            lines << line;
        }
        message->setText(lines.join("\n\n"));
    };
    render();
    QStringList details;
    for (const auto &problem : failure.problems) details << coreProblemDetails(problem);
    auto *detailsText = new QPlainTextEdit(details.join("\n\n"));
    detailsText->setObjectName("coreProblemDetails");
    detailsText->setReadOnly(true);
    detailsText->setMaximumHeight(120);
    detailsText->setVisible(false);
    layout->addWidget(detailsText);
    auto *buttons = new QDialogButtonBox;
    auto *detailsButton = buttons->addButton(tr("Details"), QDialogButtonBox::ActionRole);
    detailsButton->setObjectName("coreProblemDetailsButton");
    detailsButton->setCheckable(true);
    connect(detailsButton, &QPushButton::toggled, detailsText, &QWidget::setVisible);
    if (secretWidget) {
        auto *show = buttons->addButton(tr("Show value"), QDialogButtonBox::ActionRole);
        show->setObjectName("coreProblemReveal");
        connect(show, &QPushButton::clicked, &dialog, [&, show] { revealed = !revealed; show->setText(revealed ? tr("Hide value") : tr("Show value")); render(); });
    }
    QPushButton *go = nullptr;
    const bool rawDraft = first.document == "wireguard" ? (wgRawDirty_ || wgRawOnly_) : (rawOnly_ || rawDirty_);
    if (rawDraft ? first.line > 0 : (firstWidget || first.rule > 0 || first.line > 0)) {
        go = buttons->addButton(rawDraft ? tr("Go to line") : tr("Go to field"), QDialogButtonBox::AcceptRole);
        go->setObjectName("coreProblemGo");
        go->setDefault(true);
    }
    auto *close = buttons->addButton(QDialogButtonBox::Close);
    close->setText(tr("Close"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted && go) focusProblem(first);
    else if (firstWidget && !rawDraft) markProblemField(firstWidget);
}

// --- WireGuard/AmneziaWG documents ----------------------------------------------
// The WG/AWG text is a second document. Core decodes it (inspect) and applies
// field edits (edit/set_field); Desktop never interprets its syntax.

bool ProfileEditorDialog::isWireGuard() const {
    return wireGuard_ || (creating_ && keySource_ && keySource_->currentData().toString() == "WireGuard");
}

bool ProfileEditorDialog::validateDraft(const QString &text, CoreFailure *failure, const QString *wireGuardOverride) {
    if (!capabilities_.value("doh").toBool() && (hasDohFields(text) || hasDohFields(originalText_))) {
        failure->text = tr("Select a Core with DNS-over-HTTPS support to save this profile.");
        return false;
    }
    if (wireGuard_ && ProfileStore::keyFileReference(text) != ProfileStore::keyFileReference(originalText_)) {
        failure->text = tr("The WireGuard configuration file of this profile is managed by Bimeron. Keep File = %1 and edit the WG/AWG config tab instead.")
            .arg(ProfileStore::keyFileReference(originalText_));
        return false;
    }
    if (creating_ && isWireGuard() && ProfileStore::keyFileReference(text) != QStringLiteral("wireguard.conf")) {
        failure->text = tr("Keep File = wireguard.conf in the Bimeron config. Bimeron replaces it with the stored WG/AWG file when saving.");
        return false;
    }
    if (isWireGuard()) {
        if (wgForm_->keyGenerationPending()) {
            failure->text = tr("Wait for Core to finish generating the key before saving.");
            return false;
        }
        const QString wireGuard = wireGuardOverride ? *wireGuardOverride : wireGuardText();
        if (!wireGuardOverride && !wgBridgeError_.isEmpty()) {
            failure->text = wgBridgeError_;
            if (wgCacheRefused_) failure->problems = wgCacheProblems_;
            return false;
        }
        const auto detected = WireGuardCore::detect(coreExecutable_, wireGuard);
        if (!detected.answered()) { failure->text = detected.error; return false; }
        if (detected.format() != "wireguard") {
            failure->text = tr("The WG/AWG config is not a WireGuard or AmneziaWG configuration.");
            failure->problems = detected.problems;
            return false;
        }
        if (validatePair_) return validatePair_(text, wireGuard, failure);
    }
    return !validateRaw_ || validateRaw_(text, failure);
}

QString ProfileEditorDialog::wireGuardText() const {
    if (!isWireGuard() || !wgForm_) return {};
    wgBridgeError_.clear();
    wgCacheRefused_ = false;
    if (wgRawDirty_) {
        // QPlainTextEdit normalizes line endings; untouched text keeps the source bytes.
        QString normalized = wgLoaded_;
        normalized.replace("\r\n", "\n");
        normalized.replace('\r', '\n');
        QString text = wgRaw_->toPlainText();
        if (text == normalized) return wgLoaded_;
        // WireGuard files from Windows clients often use CRLF; keep the document convention.
        if (wgLoaded_.contains(QStringLiteral("\r\n"))) text.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
        return text;
    }
    if (wgRawOnly_) return wgLoaded_;
    QJsonArray operations;
    const QJsonObject fields = wgInspection_.value("fields").toObject();
    const auto values = wgForm_->values();
    QStringList changed = wgForm_->changedFields();
    changed.sort();
    for (const QString &id : std::as_const(changed)) {
        if (values.value(id) == fields.value(id).toString()) continue;
        const qsizetype slash = id.indexOf('/');
        operations.append(WireGuardCore::setField(id.left(slash), id.mid(slash + 1), values.value(id)));
    }
    if (operations.isEmpty()) {
        wgCacheKey_.clear();
        return wgLoaded_;
    }
    const QString key = wgLoaded_ + '\n' + QString::fromUtf8(QJsonDocument(operations).toJson(QJsonDocument::Compact));
    if (key != wgCacheKey_) {
        const auto reply = WireGuardCore::edit(coreExecutable_, wgLoaded_, operations);
        wgCacheKey_ = key;
        wgCacheError_.clear();
        wgCacheRefusedState_ = false;
        wgCacheProblems_ = reply.problems;
        wgCacheText_ = wgLoaded_;
        wgCacheInspection_ = wgInspection_;
        if (!reply.answered()) {
            wgCacheError_ = reply.error;
            // A transient bridge failure is retried on the next request for this draft.
            if (!reply.definitive) wgCacheKey_.clear();
        } else if (reply.object.contains("error") || !reply.object.value("text").isString()) {
            // A refused operation returns error, the failing edit problem and the original text.
            wgCacheError_ = tr("Core refused a WireGuard field edit. The draft has been retained.");
            wgCacheRefusedState_ = true;
        } else {
            wgCacheText_ = reply.text();
            wgCacheInspection_ = reply.object;
        }
    }
    wgBridgeError_ = wgCacheError_;
    wgCacheRefused_ = wgCacheRefusedState_;
    return wgCacheText_;
}

void ProfileEditorDialog::loadWireGuard(const QString &text, const QJsonObject &inspection) {
    wgLoaded_ = text;
    wgInspection_ = inspection;
    wgCacheKey_.clear();
    // Without a usable inspection, or for an ambiguous document, only the
    // WG/AWG config tab edits the text.
    wgRawOnly_ = (inspection.isEmpty() && !text.isEmpty()) || !inspection.value("fields_error").toString().isEmpty();
    wgForm_->setValues(inspection.value("fields").toObject());
    wgForm_->setAmneziaVersion(inspection.value("awg_version").toString());
    {
        const QSignalBlocker blocker(wgRaw_);
        wgRaw_->setPlainText(text);
    }
    wgRawDirty_ = false;
    refreshWireGuard();
}

void ProfileEditorDialog::syncWireGuardRaw() {
    const QString text = wireGuardText();
    const QSignalBlocker blocker(wgRaw_);
    wgRaw_->setPlainText(text);
    wgRawDirty_ = false;
}

bool ProfileEditorDialog::applyWireGuardRaw() {
    const QString text = wireGuardText();
    const auto reply = WireGuardCore::inspect(coreExecutable_, text);
    if (!reply.answered()) {
        QMessageBox::warning(this, tr("Error"), reply.error);
        return false;
    }
    if (reply.format() != "wireguard") {
        // A problems-only reply (for example file_too_large) has no inspection to show.
        QStringList lines;
        const CoreErrorCatalog catalog = CoreErrorCatalog::forExecutable(coreExecutable_);
        for (const auto &problem : reply.problems) lines << describeCoreProblem(problem, catalog, coreFieldLabel(problem));
        QMessageBox box(QMessageBox::Warning, tr("WG/AWG config"), lines.isEmpty()
            ? tr("Core could not read this WG/AWG configuration. Continue editing the WG/AWG config tab.") : lines.join('\n'), QMessageBox::Ok, this);
        box.setObjectName("wireGuardRawRejected");
        box.setTextFormat(Qt::PlainText);
        box.exec();
        return false;
    }
    if (!reply.fieldsError().isEmpty()) {
        QMessageBox box(QMessageBox::Information, tr("WG/AWG config"),
            tr("This WG/AWG configuration cannot be shown as fields. Continue editing the WG/AWG config tab."), QMessageBox::Ok, this);
        box.setObjectName("wireGuardRawOnly");
        box.exec();
        return false;
    }
    loadWireGuard(text, reply.object);
    checkWireGuard();
    return true;
}

void ProfileEditorDialog::refreshWireGuard() {
    if (!wgPage_ || !allowedIpsGroup_) return;
    const bool wireGuard = isWireGuard() && !native_;
    const int formIndex = tabs_->indexOf(wgPage_), rawIndex = tabs_->indexOf(wgRawPage_);
    tabs_->setTabVisible(formIndex, wireGuard);
    tabs_->setTabVisible(rawIndex, wireGuard);
    tabs_->setTabEnabled(formIndex, wireGuard && !wgRawOnly_);
    tabs_->setTabEnabled(rawIndex, wireGuard);
    tabs_->setTabText(tabs_->count() - 1, wireGuard ? tr("Bimeron config") : tr("Raw config"));
    tabs_->setTabText(formIndex, wgForm_->amneziaVersion().isEmpty() ? QStringLiteral("WireGuard") : QStringLiteral("AmneziaWG"));
    if (auto *general = qobject_cast<QFormLayout *>(name_->parentWidget()->layout()))
        setFormRowVisible(general, wgServiceNotice_, wireGuard && !creating_ && !WireGuardCore::serviceSupported(capabilities_));
    auto *networkForm = qobject_cast<QFormLayout *>(mtu_->parentWidget()->layout());
    if (wireGuard) {
        tabs_->setTabEnabled(tabs_->indexOf(networkForm->parentWidget()), !rawOnly_ || !wgRawOnly_);
    }
    for (QWidget *control : QList<QWidget *>{interfaceName_, ipv6_, killSwitch_})
        setFormRowVisible(networkForm, control, !wireGuard || !rawOnly_);
    setFormRowVisible(networkForm, mtu_, !wireGuard);
    setFormRowVisible(networkForm, dns_, !wireGuard);
    setFormRowVisible(networkForm, configureDoh_, !rawOnly_);
    wgForm_->showNetworkFields(wireGuard, !wgRawOnly_ && !wgRawDirty_);
    allowedIpsGroup_->setVisible(wireGuard);
    const QString mode = allowedIpsMode_->currentData().toString();
    for (QWidget *widget : std::as_const(ruleControls_)) widget->setEnabled(!wireGuard || mode != "only");
    if (!wireGuard) return;
    allowedIpsHint_->setText(allowedIpsMode_->currentData(Qt::ToolTipRole).toString());
    const QString allowed = wgRawDirty_ || wgRawOnly_ ? wgInspection_.value("fields").toObject().value("Peer/AllowedIPs").toString()
                                                      : wgForm_->values().value("Peer/AllowedIPs");
    allowedIpsList_->setNetworks(allowed);
    // Never ask Core from a refresh: a stalled bridge would block the GUI again.
    const QJsonObject inspection = wgRawDirty_ || wgRawOnly_ || wgCacheKey_.isEmpty() || !wgCacheError_.isEmpty()
        ? wgInspection_ : wgCacheInspection_;
    const QString coverage = inspection.value("allowed_ips").toObject().value("coverage").toString();
    QStringList notes;
    if (mode == "only")
        notes << tr("Bimeron routing rules and the default action are inactive in this mode. They stay in the profile.");
    else if (coverage == "full")
        notes << tr("AllowedIPs covers all addresses, so the first three options behave the same.");
    if (coverage == "ipv4_only" && (mode == "direct" || mode == "only"))
        notes << tr("AllowedIPs covers IPv4 only: IPv6 traffic that would go directly only because it is outside AllowedIPs is blocked.");
    allowedIpsNote_->setText(notes.join('\n'));
    allowedIpsNote_->setVisible(!notes.isEmpty());
}

void ProfileEditorDialog::checkWireGuard() {
    if (!isWireGuard() || !wgForm_ || wgRawOnly_ || wgRawDirty_) return;
    wireGuardText();
    if (!wgBridgeError_.isEmpty() && !wgCacheRefused_) {
        wgForm_->setStatus(wgBridgeError_, true);
        refreshWireGuard();
        return;
    }
    const QList<CoreProblem> problems = wgCacheKey_.isEmpty() ? parseCoreProblems(wgInspection_.value("problems")) : wgCacheProblems_;
    const CoreErrorCatalog catalog = CoreErrorCatalog::forExecutable(coreExecutable_);
    QStringList lines;
    for (const auto &problem : problems) {
        if (lines.size() == 4) { lines << tr("…and %1 more.").arg(problems.size() - 4); break; }
        lines << describeCoreProblem(problem, catalog, problemLabel(problemWidget(problem)));
    }
    // Mark the fields Core names, so a rejected value is visible where it is edited.
    showWireGuardFieldProblems(problems);
    if (wgInspection_.isEmpty() && wgCacheKey_.isEmpty()) wgForm_->setStatus({}, false);
    else wgForm_->setStatus(lines.isEmpty() ? tr("Core accepts this WireGuard configuration.") : lines.join('\n'), !lines.isEmpty());
    refreshWireGuard();
}

bool ProfileEditorDialog::showWireGuardFieldProblems(const QList<CoreProblem> &problems) {
    // Clear earlier marks and messages of the WireGuard form, then mark the fields
    // Core names and show each message directly under its field.
    for (const QString &id : WireGuardForm::mainFields() + WireGuardForm::amneziaFields()) {
        QWidget *widget = wgForm_->widget(id);
        if (!widget->property("coreProblem").toBool()) continue;
        widget->setProperty("coreProblem", false);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    wgForm_->clearFieldProblems();
    const CoreErrorCatalog catalog = CoreErrorCatalog::forExecutable(coreExecutable_);
    bool shown = false;
    for (const auto &problem : problems) {
        QWidget *widget = problemWidget(problem);
        if (!widget || problem.document != "wireguard") continue;
        markProblemField(widget);
        const QString message = describeCoreProblem(problem, catalog, QString());
        wgForm_->setFieldProblem(problem.section + '/' + problem.field,
            message.isEmpty() ? tr("Core rejected this field value.") : message);
        shown = true;
    }
    return shown;
}
