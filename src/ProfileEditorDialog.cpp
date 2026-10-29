#include "ProfileEditorDialog.h"
#include "HiDpiCheckBox.h"
#include "HiDpiComboBox.h"
#include "ProfileStore.h"
#include "CoreDiagnostic.h"
#include <memory>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSet>
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

namespace {
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
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) current = trimmed.mid(1, trimmed.size() - 2);
        else if (current == section) {
            const auto match = QRegularExpression(QString("^%1\\s*=\\s*(.*)$").arg(QRegularExpression::escape(key))).match(trimmed);
            if (match.hasMatch()) return match.captured(1).trimmed();
        }
    }
    return {};
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
}

ProfileEditorDialog::ProfileEditorDialog(const QString &text, const QJsonObject &capabilities, QWidget *parent,
                                         std::function<bool(const QString &, QString *)> validateRaw)
    : QDialog(parent), originalText_(text), validateRaw_(validateRaw) {
    setWindowTitle(tr("Profile settings"));
    resize(680, 520);
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
    keySource_->addItem(tr("ss:// key"), "SS");
    keySource_->addItem(tr("Dynamic ssconf://"), "SSConf");
    keySource_->addItem(tr("Remote socks5://"), "SOCKS5");
    keySource_->addItem(tr("vless:// key"), "VLESS");
    keySource_->addItem(tr("Hysteria2 key"), "Hysteria2");
    keySource_->addItem(tr("SSH link"), "SSH");
    if (!valueFor(text, "AccessKey", "Key").isEmpty())
        keySource_->addItem(tr("Legacy key"), "Key");
    if (!valueFor(text, "AccessKey", "File").isEmpty())
        keySource_->addItem(tr("Key file"), "File");
    keySource_->addItem(tr("Explicit Shadowsocks settings"), "Explicit");
    keySource_->addItem(tr("Explicit SOCKS5 settings"), "ExplicitSOCKS5");
    keySource_->addItem(tr("Explicit VLESS settings"), "ExplicitVLESS");
    keySource_->addItem(tr("Explicit SSH settings"), "ExplicitSSH");
    key_ = new QLineEdit;
    auto *keyRow = new QWidget;
    auto *keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(key_);
    auto *browse = new QPushButton(tr("Browse…"));
    auto *reveal = new QPushButton(tr("Show"));
    keyLayout->addWidget(reveal);
    keyLayout->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Key file"));
        if (!path.isEmpty()) { keySource_->setCurrentIndex(keySource_->findData("File")); key_->setText(path); }
    });
    server_ = new QLineEdit;
    port_ = new QSpinBox;
    port_->setRange(0, 65535);
    port_->setSpecialValueText(QStringLiteral("—"));
    method_ = new HiDpiComboBox;
    method_->setObjectName("method");
    method_->addItems({"chacha20-ietf-poly1305", "aes-128-gcm", "aes-192-gcm", "aes-256-gcm", "2022-blake3-aes-128-gcm", "2022-blake3-aes-256-gcm", "2022-blake3-chacha20-poly1305"});
    password_ = new QLineEdit;
    password_->setEchoMode(QLineEdit::Password);
    username_ = new QLineEdit;
    sshIdentity_ = new QLineEdit;
    sshKnownHosts_ = new QLineEdit;
    auto *identityRow = new QWidget;
    auto *identityLayout = new QHBoxLayout(identityRow);
    identityLayout->setContentsMargins(0, 0, 0, 0);
    identityLayout->addWidget(sshIdentity_);
    auto *chooseIdentity = new QPushButton(tr("Browse…"));
    identityLayout->addWidget(chooseIdentity);
    connect(chooseIdentity, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("SSH identity file"));
        if (!path.isEmpty()) sshIdentity_->setText(QFileInfo(path).absoluteFilePath());
    });
    auto *hostsRow = new QWidget;
    auto *hostsLayout = new QHBoxLayout(hostsRow);
    hostsLayout->setContentsMargins(0, 0, 0, 0);
    hostsLayout->addWidget(sshKnownHosts_);
    auto *chooseHosts = new QPushButton(tr("Browse…"));
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
    mode_ = new HiDpiComboBox;
    if (capabilities.value("vpn").toBool()) mode_->addItem(tr("System VPN"), "vpn");
    if (capabilities.value("proxy").toBool()) mode_->addItem(tr("Local SOCKS5"), "proxy");
    listen_ = new QLineEdit;
    generalForm->addRow(tr("Name"), name_);
    generalForm->addRow(tr("Key source"), keySource_);
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
    tabs_->addTab(general, tr("General"));

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
    tabs_->addTab(vlessPage, tr("VLESS"));

    auto *network = new QWidget;
    auto *networkForm = new QFormLayout(network);
    interfaceName_ = new QLineEdit;
    mtu_ = new QSpinBox;
    mtu_->setRange(1280, 1500);
    dns_ = new QLineEdit;
    dns_->setPlaceholderText("1.1.1.1, 8.8.8.8");
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
    routingLayout->addLayout(defaultRow);
    rules_ = new QTableWidget(0, 4);
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
    routingLayout->addWidget(rules_);
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
    connect(add, &QPushButton::clicked, this, [this] { addRule(Rule{}); });
    connect(remove, &QPushButton::clicked, this, [this] { if (rules_->currentRow() >= 0) rules_->removeRow(rules_->currentRow()); });
    connect(up, &QPushButton::clicked, this, [this, updateRuleActions] { moveRule(-1); updateRuleActions(); });
    connect(down, &QPushButton::clicked, this, [this, updateRuleActions] { moveRule(1); updateRuleActions(); });
    for (auto *button : {add, remove, up, down}) ruleButtons->addWidget(button);
    ruleButtons->addStretch();
    routingLayout->addLayout(ruleButtons);
    tabs_->addTab(routing, tr("Routing"));

    auto *rawPage = new QWidget;
    auto *rawLayout = new QVBoxLayout(rawPage);
    raw_ = new QPlainTextEdit;
    raw_->setObjectName("rawConfig");
    raw_->setLineWrapMode(QPlainTextEdit::NoWrap);
    raw_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    new ConfigHighlighter(raw_->document());
    auto *rawNotice = new QLabel(tr("Raw config contains credentials. Settings unavailable in the visual editor must be edited here."));
    rawNotice->setWordWrap(true);
    auto *showRaw = new QPushButton(tr("Show/Edit raw config"));
    showRaw->setObjectName("showRaw");
    raw_->hide();
    rawLayout->addWidget(rawNotice);
    rawLayout->addWidget(showRaw);
    auto *useVisual = new QPushButton(tr("Validate and use visual editor"));
    useVisual->setObjectName("useVisual");
    rawLayout->addWidget(useVisual);
    connect(useVisual, &QPushButton::clicked, this, [this] {
        if (applyRaw()) tabs_->setCurrentIndex(0);
    });
    connect(showRaw, &QPushButton::clicked, this, [this, showRaw] {
        raw_->setVisible(!raw_->isVisible());
        showRaw->setText(raw_->isVisible() ? tr("Hide raw config") : tr("Show/Edit raw config"));
    });
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
        if (rawOnly_ && tabs_->currentIndex() != tabs_->count() - 1) {
            tabs_->setCurrentIndex(tabs_->count() - 1);
            return;
        }
        if (tabs_->currentIndex() != tabs_->count() - 1
            && keySource_->currentData().toString() == "Explicit"
            && method_->currentText().startsWith("2022-") && !prefix_->text().isEmpty()) {
            QMessageBox::warning(this, tr("Error"), tr("SS2022 does not support Prefix. Clear Prefix before saving."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    loadText(text);
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
        const bool uriVless = (source == "VLESS" || source == "Key") && url.scheme() == "vless"
            && ProfileStore::connectionInfo("[AccessKey]\nKey = " + key_->text()).connectable;
        if (!explicitVless && !uriVless) return;
        vlessForm->setRowVisible(vlessEndpoint, uriVless);
        vlessForm->setRowVisible(vlessEncryption, uriVless);
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
        if (network == "ws" && vlessSecurity_->currentText() == "reality") {
            const QSignalBlocker blocker(vlessSecurity_);
            vlessSecurity_->setCurrentText("tls");
        }
        const bool reality = vlessSecurity_->currentText() == "reality";
        if (network == "xhttp") {
            const QString current = vlessXhttpMode_->currentText();
            const QSignalBlocker blocker(vlessXhttpMode_);
            vlessXhttpMode_->clear();
            vlessXhttpMode_->addItems(reality ? QStringList{"stream-one", "stream-up", "auto"}
                                               : QStringList{"packet-up", "stream-up", "auto"});
            const int index = vlessXhttpMode_->findText(current);
            vlessXhttpMode_->setCurrentIndex(index < 0 ? 0 : index);
        }
        vlessForm->setRowVisible(vlessFlow_, network == "tcp" && reality);
        vlessForm->setRowVisible(vlessPublicKey_, reality);
        vlessForm->setRowVisible(vlessShortId_, reality);
        vlessForm->setRowVisible(vlessSpiderX_, reality);
        vlessForm->setRowVisible(vlessAlpn_, !(network == "tcp" && reality));
        vlessForm->setRowVisible(vlessPath_, network == "ws" || network == "xhttp");
        vlessForm->setRowVisible(vlessHost_, network == "ws" || network == "xhttp");
        vlessForm->setRowVisible(vlessEarlyData_, network == "ws");
        vlessForm->setRowVisible(vlessServiceName_, network == "grpc");
        vlessForm->setRowVisible(vlessAuthority_, network == "grpc");
        vlessForm->setRowVisible(vlessMultiMode_, network == "grpc");
        vlessForm->setRowVisible(vlessXhttpMode_, network == "xhttp");
        vlessForm->setRowVisible(vlessExtra, uriVless && network == "xhttp" && !vlessExtra->toPlainText().isEmpty());
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
        const bool showParameters = ss || explicitKey || explicitSocks5 || explicitVless || explicitSsh;
        mode_->setToolTip(ssh ? tr("SSH VPN carries TCP and captured DNS only; other proxied UDP is dropped.") : QString());

        keyLabel->setText(file ? tr("File path") : tr("URL"));
        key_->setEnabled(true);
        generalForm->setRowVisible(keyRow, !explicitKey && !explicitSocks5 && !explicitVless && !explicitSsh);
        key_->setVisible(file || keyRevealed_);
        browse->setVisible(file);
        reveal->setVisible(ss || ssconf || socks5 || source == "VLESS" || source == "Hysteria2" || source == "SSH" || source == "Key");
        reveal->setText(keyRevealed_ ? tr("Hide") : tr("Show"));

        generalForm->setRowVisible(server_, showParameters);
        generalForm->setRowVisible(port_, showParameters);
        generalForm->setRowVisible(method_, ss || explicitKey);
        generalForm->setRowVisible(username_, explicitSocks5 || explicitSsh);
        generalForm->setRowVisible(identityRow, ssh);
        generalForm->setRowVisible(hostsRow, ssh);
        generalForm->setRowVisible(password_, showParameters && !explicitVless && !explicitSsh);
        generalForm->setRowVisible(prefix_, ss || explicitKey);
        generalForm->setRowVisible(ssPlugin_, explicitKey);
        generalForm->setRowVisible(ssPluginOpts_, explicitKey);
        for (auto *widget : {static_cast<QWidget *>(server_), static_cast<QWidget *>(port_), static_cast<QWidget *>(method_), static_cast<QWidget *>(username_), static_cast<QWidget *>(password_), static_cast<QWidget *>(prefix_)})
            widget->setEnabled(explicitKey || explicitSocks5 || explicitVless || explicitSsh);
        if (ss) applySs(key_->text());
        if (explicitKey && port_->value() == 0) port_->setValue(8388);
        if (explicitSocks5 && port_->value() == 0) port_->setValue(1080);
        if (explicitVless && port_->value() == 0) port_->setValue(443);
        if (explicitSsh && port_->value() == 0) port_->setValue(22);
        updateVlessControls();
    };
    connect(keySource_, &QComboBox::currentIndexChanged, this, [this, updateKeyControls] {
        keyRevealed_ = false;
        updateKeyControls();
    });
    const auto updateSourceLock = [this] {
        const QString source = keySource_->currentData().toString();
        const QString accessKey = source.startsWith("Explicit")
            ? "Type = " + QString(source == "ExplicitVLESS" ? "vless" : source == "ExplicitSOCKS5" ? "socks5" : source == "ExplicitSSH" ? "ssh" : "shadowsocks")
            : source + " = " + key_->text();
        keySource_->setEnabled(!ProfileStore::connectionInfo("[AccessKey]\n" + accessKey).connectable);
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
    connect(raw_, &QPlainTextEdit::textChanged, this, [this] { rawDirty_ = true; });
    assessRepresentation();
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        const int rawIndex = tabs_->count() - 1;
        if (index == rawIndex) {
            if (!rawDirty_) syncRaw();
        } else if (rawDirty_) {
            if (!applyRaw()) {
                const QSignalBlocker blocker(tabs_);
                tabs_->setCurrentIndex(rawIndex);
            }
        }
    });
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
    rules_->setRowCount(0);
    ignoreLocalNetwork_->setChecked(false);
    hasUnknownSettings_ = false;
    rawOnly_ = false;
    name_->setText(valueFor(text, "Profile", "Name"));
    QString source = "SS", value = valueFor(text, "AccessKey", "SS");
    if (value.isEmpty()) { source = "SSConf"; value = valueFor(text, "AccessKey", "SSConf"); }
    if (value.isEmpty()) { source = "SOCKS5"; value = valueFor(text, "AccessKey", "SOCKS5"); }
    if (value.isEmpty()) { source = "VLESS"; value = valueFor(text, "AccessKey", "VLESS"); }
    if (value.isEmpty()) { source = "Hysteria2"; value = valueFor(text, "AccessKey", "Hysteria2"); }
    if (value.isEmpty()) { source = "SSH"; value = valueFor(text, "AccessKey", "SSH"); }
    if (value.isEmpty()) { source = "Key"; value = valueFor(text, "AccessKey", "Key"); }
    if (value.isEmpty()) { source = "File"; value = valueFor(text, "AccessKey", "File"); }
    if (value.isEmpty() && !valueFor(text, "AccessKey", "Server").isEmpty()) {
        const QString type = valueFor(text, "AccessKey", "Type");
        source = type == "socks5" ? "ExplicitSOCKS5" : type == "vless" ? "ExplicitVLESS" : type == "ssh" ? "ExplicitSSH"
            : (type == "shadowsocks" || type == "ss") ? "Explicit" : QString();
        if (source.isEmpty()) rawOnly_ = true;
    }
    keySource_->setCurrentIndex(keySource_->findData(source)); key_->setText(value);
    server_->setText(valueFor(text, "AccessKey", "Server"));
    port_->setValue(valueFor(text, "AccessKey", "Port").toInt());
    const int methodIndex = method_->findText(valueFor(text, "AccessKey", "Method"));
    method_->setCurrentIndex(methodIndex);
    if (source == "Explicit" && methodIndex < 0) rawOnly_ = true;
    password_->setText(valueFor(text, "AccessKey", "Password"));
    username_->setText(valueFor(text, "AccessKey", "Username"));
    sshIdentity_->setText(valueFor(text, "AccessKey", "IdentityFile"));
    sshKnownHosts_->setText(valueFor(text, "AccessKey", "KnownHostsFile"));
    prefix_->setText(valueFor(text, "AccessKey", "Prefix"));
    ssPlugin_->setCurrentIndex(ssPlugin_->findText(valueFor(text, "AccessKey", "Plugin")));
    ssPluginOpts_->setText(valueFor(text, "AccessKey", "PluginOpts"));
    if (source == "Explicit" && (ssPlugin_->currentIndex() < 0
        || (method_->currentText().startsWith("2022-") && !prefix_->text().isEmpty()))) rawOnly_ = true;
    vlessId_->setText(valueFor(text, "AccessKey", "ID"));
    const auto setChoice = [this, &text, &source](QComboBox *combo, const QString &field) {
        const QString value = valueFor(text, "AccessKey", field);
        if (value.isEmpty()) return;
        const int index = combo->findText(value);
        if (index < 0) {
            hasUnknownSettings_ = true;
            if (source == "ExplicitVLESS") rawOnly_ = true;
            return;
        }
        combo->setCurrentIndex(index);
    };
    setChoice(vlessNetwork_, "Network");
    setChoice(vlessSecurity_, "Security");
    setChoice(vlessFlow_, "Flow");
    setChoice(vlessFingerprint_, "Fingerprint");
    setChoice(vlessXhttpMode_, "XHTTPMode");
    vlessServerName_->setText(valueFor(text, "AccessKey", "ServerName"));
    vlessPublicKey_->setText(valueFor(text, "AccessKey", "PublicKey"));
    vlessShortId_->setText(valueFor(text, "AccessKey", "ShortID"));
    vlessSpiderX_->setText(valueFor(text, "AccessKey", "SpiderX"));
    vlessAlpn_->setText(valueFor(text, "AccessKey", "ALPN"));
    const QString network = vlessNetwork_->currentText();
    vlessPath_->setText(valueFor(text, "AccessKey", network == "xhttp" ? "XHTTPPath" : "WSPath"));
    vlessHost_->setText(valueFor(text, "AccessKey", network == "xhttp" ? "XHTTPHost" : "WSHost"));
    vlessEarlyData_->setValue(valueFor(text, "AccessKey", "WSEarlyData").toInt());
    vlessServiceName_->setText(valueFor(text, "AccessKey", "GRPCServiceName"));
    vlessAuthority_->setText(valueFor(text, "AccessKey", "GRPCAuthority"));
    const QString multiMode = valueFor(text, "AccessKey", "GRPCMultiMode");
    vlessMultiModeConfigured_ = !multiMode.isEmpty();
    vlessMultiMode_->setChecked(multiMode == "true");
    if (source == "ExplicitVLESS" && valueFor(text, "AccessKey", "Encryption") != "none")
        rawOnly_ = true;
    const QString configuredMode = valueFor(text, "Interface", "Mode");
    const int modeIndex = mode_->findData(configuredMode);
    if (!configuredMode.isEmpty() && modeIndex < 0) hasUnknownSettings_ = true;
    mode_->setCurrentIndex(qMax(0, modeIndex));
    listen_->setText(valueFor(text, "Interface", "Listen").isEmpty() ? "127.0.0.1:1080" : valueFor(text, "Interface", "Listen"));
    interfaceName_->setText(valueFor(text, "Interface", "Name").isEmpty() ? "Bimeron" : valueFor(text, "Interface", "Name"));
    const int mtu = valueFor(text, "Interface", "MTU").toInt();
    mtu_->setValue(mtu ? mtu : 1420);
    dns_->setText(valueFor(text, "Interface", "DNS").isEmpty() ? "1.1.1.1" : valueFor(text, "Interface", "DNS"));
    ipv6_->setCurrentIndex(qMax(0, ipv6_->findData(valueFor(text, "Interface", "IPv6"))));
    killSwitch_->setChecked(valueFor(text, "Interface", "KillSwitch").compare("true", Qt::CaseInsensitive) == 0);
    defaultRoute_->setCurrentIndex(qMax(0, defaultRoute_->findData(valueFor(text, "Routing", "Default"))));
    QString section; Rule rule;
    const auto finishRule = [this](const Rule &finished) {
        if (finished.target.isEmpty()) return;
        if (finished.enabled && !finished.domain && finished.action == "direct"
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
        if (trimmed == "[Rule]") { finishRule(rule); rule = {}; rule.enabled = !commented; section = "Rule"; }
        else if (trimmed.startsWith('[')) section.clear();
        else if (section == "Rule" && trimmed.startsWith("CIDR")) { rule.target = trimmed.mid(trimmed.indexOf('=') + 1).trimmed(); rule.domain = false; }
        else if (section == "Rule" && trimmed.startsWith("Domain")) { rule.target = trimmed.mid(trimmed.indexOf('=') + 1).trimmed(); rule.domain = true; }
        else if (section == "Rule" && trimmed.startsWith("Action")) rule.action = trimmed.mid(trimmed.indexOf('=') + 1).trimmed();
    }
    finishRule(rule);
    const QHash<QString, QSet<QString>> known{
        {"Profile", {"Version", "Name"}},
        {"AccessKey", {"SS", "SSConf", "SOCKS5", "VLESS", "Hysteria2", "SSH", "Key", "File", "Type", "Server", "Port", "Method", "Username", "Password", "Prefix", "Plugin", "PluginOpts", "IdentityFile", "KnownHostsFile", "ID", "Encryption", "Flow", "Network", "Security", "ServerName", "PublicKey", "ShortID", "Fingerprint", "SpiderX", "ALPN", "WSPath", "WSHost", "WSEarlyData", "GRPCServiceName", "GRPCAuthority", "GRPCMultiMode", "XHTTPPath", "XHTTPHost", "XHTTPMode"}},
        {"Interface", {"Mode", "Name", "MTU", "DNS", "IPv6", "KillSwitch", "Listen"}},
        {"Routing", {"Default"}}, {"Rule", {"CIDR", "Domain", "Action"}}};
    QString currentSection;
    for (const QString &sourceLine : text.split('\n')) {
        const QString line = QString(sourceLine).remove(QChar(0xfeff)).trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.size() - 2).trimmed();
            if (!known.contains(currentSection)) hasUnknownSettings_ = true;
        } else if (line.contains('=')) {
            const QString key = line.left(line.indexOf('=')).trimmed();
            if (!known.value(currentSection).contains(key)) {
                hasUnknownSettings_ = true;
                if (source == "ExplicitVLESS" && currentSection == "AccessKey") rawOnly_ = true;
            }
        }
    }
    raw_->setPlainText(text);
    rawDirty_ = false;
}

void ProfileEditorDialog::addRule(const Rule &rule) {
    if ((rule.domain && !rules_->property("domainSplit").toBool()) || (!rule.domain && !rules_->property("ipSplit").toBool()))
        hasUnknownSettings_ = true;
    const int row = rules_->rowCount(); rules_->insertRow(row);
    auto *enabledCell = new QWidget;
    auto *enabledLayout = new QHBoxLayout(enabledCell);
    enabledLayout->setContentsMargins(0, 0, 0, 0);
    enabledLayout->setAlignment(Qt::AlignCenter);
    auto *enabled = new HiDpiCheckBox;
    enabled->setChecked(rule.enabled);
    enabledLayout->addWidget(enabled);
    rules_->setCellWidget(row, 0, enabledCell);
    auto *type = new QComboBox;
    type->setObjectName("tableEditor");
    if (rules_->property("ipSplit").toBool()) type->addItem(tr("IP / CIDR"), "CIDR");
    if (rules_->property("domainSplit").toBool()) type->addItem(tr("Domain"), "Domain");
    type->setCurrentIndex(qMax(0, type->findData(rule.domain ? "Domain" : "CIDR")));
    rules_->setCellWidget(row, 1, type);
    auto *target = new QLineEdit(rule.target);
    target->setObjectName("tableEditor");
    const auto updatePlaceholder = [type, target] {
        target->setPlaceholderText(type->currentData().toString() == "Domain"
            ? QObject::tr("Domain: example.com")
            : QObject::tr("IP or CIDR: 1.1.1.1, 10.0.0.0/8"));
    };
    connect(type, &QComboBox::currentIndexChanged, target, updatePlaceholder);
    updatePlaceholder();
    rules_->setCellWidget(row, 2, target);
    auto *action = new QComboBox;
    action->setObjectName("tableEditor");
    action->addItem(tr("Via VPN"), "proxy"); action->addItem(tr("Direct"), "direct"); action->addItem(tr("Block"), "block");
    action->setCurrentIndex(qMax(0, action->findData(rule.action)));
    rules_->setCellWidget(row, 3, action);
}

void ProfileEditorDialog::moveRule(int delta) {
    const int row = rules_->currentRow(), target = row + delta;
    if (row < 0 || target < 0 || target >= rules_->rowCount()) return;
    Rule a{static_cast<QLineEdit *>(rules_->cellWidget(row, 2))->text(), static_cast<QComboBox *>(rules_->cellWidget(row, 3))->currentData().toString(), ruleEnabled(rules_, row)->isChecked(), static_cast<QComboBox *>(rules_->cellWidget(row, 1))->currentData().toString() == "Domain"};
    Rule b{static_cast<QLineEdit *>(rules_->cellWidget(target, 2))->text(), static_cast<QComboBox *>(rules_->cellWidget(target, 3))->currentData().toString(), ruleEnabled(rules_, target)->isChecked(), static_cast<QComboBox *>(rules_->cellWidget(target, 1))->currentData().toString() == "Domain"};
    ruleEnabled(rules_, row)->setChecked(b.enabled); static_cast<QComboBox *>(rules_->cellWidget(row, 1))->setCurrentIndex(b.domain ? 1 : 0); static_cast<QLineEdit *>(rules_->cellWidget(row, 2))->setText(b.target); static_cast<QComboBox *>(rules_->cellWidget(row, 3))->setCurrentIndex(static_cast<QComboBox *>(rules_->cellWidget(row, 3))->findData(b.action));
    ruleEnabled(rules_, target)->setChecked(a.enabled); static_cast<QComboBox *>(rules_->cellWidget(target, 1))->setCurrentIndex(a.domain ? 1 : 0); static_cast<QLineEdit *>(rules_->cellWidget(target, 2))->setText(a.target); static_cast<QComboBox *>(rules_->cellWidget(target, 3))->setCurrentIndex(static_cast<QComboBox *>(rules_->cellWidget(target, 3))->findData(a.action));
    rules_->selectRow(target);
}

QString ProfileEditorDialog::buildText() const {
    QString accessKey;
    if (keySource_->currentData().toString() == "ExplicitSSH")
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
            + (method_->currentText().startsWith("2022-") && prefix_->text().isEmpty() ? QString() : "\nPrefix = " + prefix_->text())
            + (ssPlugin_->currentText().isEmpty() ? QString() : "\nPlugin = " + ssPlugin_->currentText())
            + (ssPluginOpts_->text().isEmpty() ? QString() : "\nPluginOpts = " + ssPluginOpts_->text());
    else if (keySource_->currentData().toString() == "ExplicitVLESS") {
        const QString network = vlessNetwork_->currentText();
        const bool reality = vlessSecurity_->currentText() == "reality";
        accessKey = "Type = vless\nServer = " + server_->text()
            + "\nPort = " + QString::number(port_->value())
            + "\nID = " + vlessId_->text()
            + "\nEncryption = none\nNetwork = " + network
            + "\nSecurity = " + vlessSecurity_->currentText();
        const auto append = [&accessKey](const QString &field, const QString &value) {
            if (!value.isEmpty()) accessKey += "\n" + field + " = " + value;
        };
        if (network == "tcp" && reality) append("Flow", vlessFlow_->currentText());
        append("ServerName", vlessServerName_->text());
        append("Fingerprint", vlessFingerprint_->currentText());
        if (reality) {
            append("PublicKey", vlessPublicKey_->text());
            append("ShortID", vlessShortId_->text());
            append("SpiderX", vlessSpiderX_->text());
        }
        if (!(network == "tcp" && reality)) append("ALPN", vlessAlpn_->text());
        if (network == "ws" || network == "xhttp") {
            append(network == "ws" ? "WSPath" : "XHTTPPath", vlessPath_->text());
            append(network == "ws" ? "WSHost" : "XHTTPHost", vlessHost_->text());
        }
        if (network == "ws" && vlessEarlyData_->value()) append("WSEarlyData", QString::number(vlessEarlyData_->value()));
        if (network == "grpc") {
            append("GRPCServiceName", vlessServiceName_->text());
            append("GRPCAuthority", vlessAuthority_->text());
            if (vlessMultiMode_->isChecked() || vlessMultiModeConfigured_)
                append("GRPCMultiMode", vlessMultiMode_->isChecked() ? "true" : "false");
        }
        if (network == "xhttp") append("XHTTPMode", vlessXhttpMode_->currentText());
    }
    else
        accessKey = keySource_->currentData().toString() + " = " + key_->text();
    const QString source = keySource_->currentData().toString();
    if (source == "SSH" || source == "ExplicitSSH" || (source == "Key" && QUrl(key_->text()).scheme() == "ssh")) {
        if (!sshIdentity_->text().isEmpty()) accessKey += "\nIdentityFile = " + sshIdentity_->text();
        if (!sshKnownHosts_->text().isEmpty()) accessKey += "\nKnownHostsFile = " + sshKnownHosts_->text();
    }
    QString text = "[Profile]\nVersion = 1\nName = " + name_->text()
        + "\n\n[AccessKey]\n" + accessKey
        + "\n\n[Interface]\nMode = " + mode_->currentData().toString()
        + "\nName = " + interfaceName_->text()
        + "\nMTU = " + QString::number(mtu_->value())
        + "\nDNS = " + dns_->text()
        + "\nIPv6 = " + ipv6_->currentData().toString()
        + "\nKillSwitch = " + QString(killSwitch_->isChecked() ? "true" : "false")
        + "\nListen = " + listen_->text()
        + "\n\n[Routing]\nDefault = " + defaultRoute_->currentData().toString() + "\n";
    if (ignoreLocalNetwork_->isChecked())
        text += QString("\n[Rule]\nCIDR = %1\nAction = direct\n").arg(localNetworkCidrs());
    for (int row = 0; row < rules_->rowCount(); ++row) {
        const QString prefix = ruleEnabled(rules_, row)->isChecked() ? QString() : "# ";
        const QString type = static_cast<QComboBox *>(rules_->cellWidget(row, 1))->currentData().toString();
        const QString target = static_cast<QLineEdit *>(rules_->cellWidget(row, 2))->text();
        text += QString("\n%1[Rule]\n%1%2 = %3\n%1Action = %4\n")
            .arg(prefix, type, type == "CIDR" ? normalizedCidrs(target) : target.trimmed(), static_cast<QComboBox *>(rules_->cellWidget(row, 3))->currentData().toString());
    }
    return text;
}

void ProfileEditorDialog::assessRepresentation() {
    const QSignalBlocker tabBlocker(tabs_);
    if (refreshControls_) refreshControls_();
    visualBaseline_ = buildText();
    // This is a conservative serializer guard, not a second Core validator.
    // Compare configured tokens with the actual serializer, including ordered rules.
    const auto tokens = [](const QString &text, bool *valid) {
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
                if (line != "[Rule]" && !section.startsWith("DisabledRule")) continue;
                if (line == "[Rule]") {
                    section = "DisabledRule" + QString::number(++rule);
                    result.insert(section + "/", {});
                    continue;
                }
            }
            if (line.startsWith('[') && line.endsWith(']')) {
                section = line.mid(1, line.size() - 2);
                if (section == "Rule") section += QString::number(++rule);
                else if (sections.contains(section)) *valid = false;
                sections.insert(section);
                result.insert(section + "/", {});
                continue;
            }
            const auto equal = line.indexOf('=');
            if (section.isEmpty() || equal < 1) { *valid = false; continue; }
            const QString key = section + "/" + line.left(equal).trimmed();
            const QString value = line.mid(equal + 1).trimmed();
            if (result.contains(key) || value.startsWith('"')) *valid = false;
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
    for (auto it = original.begin(); it != original.end(); ++it)
        if (!generated.contains(it.key()) || generated.value(it.key()) != it.value()) valid = false;
    // Omitted settings may only acquire the documented Core default, never the
    // first available GUI/capability choice. Unknown defaults require Raw.
    const QMap<QString, QString> defaults{
        {"Profile/Version", "1"}, {"Profile/Name", ""}, {"AccessKey/Prefix", ""},
        {"Interface/Mode", "vpn"}, {"Interface/Name", "Bimeron"}, {"Interface/MTU", "1420"},
        {"Interface/DNS", "1.1.1.1"}, {"Interface/IPv6", "enable"},
        {"Interface/KillSwitch", "false"}, {"Interface/Listen", "127.0.0.1:1080"},
        {"Routing/Default", "proxy"}
    };
    for (auto it = generated.begin(); it != generated.end(); ++it) {
        if (original.contains(it.key())) continue;
        if (QStringList{"Profile/", "AccessKey/", "Interface/", "Routing/"}.contains(it.key())) continue;
        if (!defaults.contains(it.key()) || defaults.value(it.key()) != it.value()) valid = false;
    }
    rawOnly_ = rawOnly_ || hasUnknownSettings_ || !valid;
    const int rawIndex = tabs_->count() - 1;
    for (int i = 0; i < rawIndex; ++i) tabs_->setTabEnabled(i, !rawOnly_);
    tabs_->widget(1)->setEnabled(!rawOnly_ && keySource_->currentData().toString() == "ExplicitVLESS");
    if (rawOnly_) {
        const QSignalBlocker blocker(tabs_);
        tabs_->setCurrentIndex(rawIndex);
    }
}

bool ProfileEditorDialog::applyRaw() {
    const QString text = configText();
    QString error;
    if (!validateRaw_ || !validateRaw_(text, &error)) {
        QMessageBox::warning(this, tr("Error"), error.isEmpty()
            ? tr("The selected Core CLI must validate this configuration before visual editing.")
            : safeCoreDiagnostic(error.toUtf8()));
        return false;
    }
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
    const QSignalBlocker blocker(raw_);
    raw_->setPlainText(text);
}

QString ProfileEditorDialog::configText() const {
    if (rawDirty_) {
        // QPlainTextEdit normalizes line endings; untouched Raw retains the source.
        QString normalized = loadedText_;
        normalized.replace("\r\n", "\n");
        normalized.replace('\r', '\n');
        if (raw_->toPlainText() == normalized) return loadedText_;
        return raw_->toPlainText();
    }
    if (rawOnly_ || buildText() == visualBaseline_) return loadedText_;
    return buildText();
}

void ProfileEditorDialog::reject() {
    if (configText() != originalText_) {
        QMessageBox prompt(QMessageBox::Question, tr("Discard changes"), tr("Discard unsaved changes?"), QMessageBox::NoButton, this);
        auto *discard = prompt.addButton(tr("Discard changes"), QMessageBox::DestructiveRole);
        prompt.addButton(tr("Continue editing"), QMessageBox::RejectRole);
        prompt.exec();
        if (prompt.clickedButton() != discard) return;
    }
    QDialog::reject();
}
