#include "ProfileEditorDialog.h"
#include "HiDpiCheckBox.h"
#include "HiDpiComboBox.h"
#include "ProfileStore.h"
#include "GeoDataSelection.h"
#include "GeoDataDialog.h"
#include "GeoIPLabels.h"
#include "GeoSitePicker.h"
#include "CoreDiagnostic.h"
#include <algorithm>
#include <QApplication>
#include <memory>
#include <vector>

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
            fields.insert(section + "/" + line.left(line.indexOf('=')).trimmed(), value);
        }
    }
    return fields;
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
            if (match.hasMatch()) return match.captured(1).trimmed();
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
}

ProfileEditorDialog::ProfileEditorDialog(const QString &text, const QJsonObject &capabilities, QWidget *parent,
                                         std::function<bool(const QString &, QString *)> validateRaw,
                                         const QString &coreExecutable, const QString &profilePath)
    : QDialog(parent), coreExecutable_(coreExecutable), profilePath_(profilePath), capabilities_(capabilities),
      originalText_(text), validateRaw_(validateRaw) {
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
    keySource_->addItem(tr("Explicit Trojan settings"), "ExplicitTrojan");
    keySource_->addItem(tr("Explicit HTTP settings"), "Explicithttp");
    keySource_->addItem(tr("Explicit HTTPS settings"), "Explicithttps");
    keySource_->addItem(tr("Explicit Naive H2 settings"), "Explicitnaive");
    keySource_->addItem(tr("Explicit MASQUE settings"), "Explicitmasque");
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
    auto *sourceNotice = new QLabel(tr("Changing the key source or removing configured fields requires Raw config; the document bridge cannot delete fields."));
    sourceNotice->setWordWrap(true);
    sourceNotice->setObjectName("sourceEditRestriction");
    sourceNotice->setVisible(!coreExecutable_.isEmpty());
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
    auto *choose = new QPushButton(tr("Choose…"));
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
        authForm->setRowVisible(nativeAuthSource_, password);
        for (QWidget *widget : QList<QWidget *>{authName, authPassword, ask, remember}) authForm->setRowVisible(widget, enter);
        authForm->setRowVisible(identity, !password);
        authForm->setRowVisible(choose, !password);
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
    for (const auto &text : {tr("Review changes…"), tr("Remove Windows profile…")}) {
        auto *button = new QPushButton(text);
        button->setEnabled(false);
        nativeActionRow->addWidget(button);
    }
    nativeActions->addLayout(nativeActionRow);
    int serverRow = 0;
    QFormLayout::ItemRole serverRole;
    generalForm->getWidgetPosition(nativeServerName_, &serverRow, &serverRole);
    generalForm->insertRow(serverRow + 1, nativeProfileActions_);
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
    tabs_->addTab(vlessScroll, tr("VLESS"));

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
    routingLayout->addWidget(rules_, 1);
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
    routingLayout->addLayout(ruleButtons);
    auto *firstMatch = new QLabel(tr("Rules run top to bottom; the first match wins. New local-network bypass rules are inserted first; imported rules keep their positions. Edited IP literals use /32 or /128."));
    firstMatch->setWordWrap(true);
    routingLayout->addWidget(firstMatch);
    auto *limitations = new QPushButton(tr("Routing limitations…"));
    connect(limitations, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Routing limitations…"),
            tr("GeoSite VPN routing uses DNS metadata (TTL, shared IPs and caches); DoH/DoT and bare IPs may not match. SOCKS GeoIP uses known IPs only, without local DNS lookup. IPv6 disable remains dominant."));
    });
    routingLayout->addWidget(limitations);
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
    routingLayout->addWidget(routingStatusScroll);
    auto *manage = new QPushButton(tr("Manage databases…"));
    manage->setObjectName("manageGeodata");
    manage->setObjectName("manageDatabases");
    connect(manage, &QPushButton::clicked, this, &ProfileEditorDialog::manageDatabases);
    routingLayout->addWidget(manage);
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
        if (!native_ && !rawOnly_ && !rawDirty_ && !parameterError().isEmpty()) {
            QMessageBox::warning(this, tr("Error"), parameterError());
            return;
        }
        const QString draft = configText();
        QString error = (rawDirty_ || rawOnly_ || !bridgeReady_) ? QString() : bridgeError_;
        if (error.isEmpty()) error = routingError();
        if (!native_ && error.isEmpty() && validateRaw_ && !validateRaw_(draft, &error) && error.isEmpty())
            error = tr("Core validation failed. The draft has been retained.");
        if (!error.isEmpty()) {
            QMessageBox::warning(this, tr("Error"), safeCoreDiagnostic(error.toUtf8()));
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
        {"MASQUEPath", tr("MASQUE UDP path")}};
    for (const auto &entry : labels) {
        auto *edit = new QLineEdit;
        edit->setObjectName("parameter" + entry.first);
        parameters_.insert(entry.first, edit);
        form->addRow(entry.second, edit);
    }
    parameters_["Security"]->setValidator(new QRegularExpressionValidator(QRegularExpression("tls"), this));
    parameters_["GRPCMultiMode"]->setValidator(new QRegularExpressionValidator(QRegularExpression("true|false"), this));
    parameters_["Security"]->setPlaceholderText(tr("Empty uses TLS"));
    parameters_["GRPCMultiMode"]->setPlaceholderText(tr("Empty, true or false"));
    auto *browseCa = new QPushButton(tr("Choose CA file…"));
    browseCa->setObjectName("browseParameterCa");
    form->addRow(browseCa);
    connect(browseCa, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("CA file"));
        if (!path.isEmpty()) parameters_["CAFile"]->setText(QFileInfo(path).absoluteFilePath());
    });
    shadowTls_ = new HiDpiCheckBox(tr("Enable ShadowTLS v3"));
    shadowTls_->setObjectName("shadowTls");
    shadowName_ = new QLineEdit;
    shadowName_->setObjectName("shadowName");
    shadowPassword_ = new QLineEdit;
    shadowPassword_->setObjectName("shadowPassword");
    shadowPassword_->setEchoMode(QLineEdit::Password);
    form->addRow(shadowTls_);
    form->addRow(tr("ShadowTLS handshake name"), shadowName_);
    form->addRow(tr("ShadowTLS password"), shadowPassword_);
    scroll->setWidget(fields);
    parameterLayout->addWidget(scroll);
    tabs_->insertTab(tabs_->count() - 1, parameterPage_, tr("Parameters / ShadowTLS"));
    connect(trojanNetwork_, &QComboBox::currentTextChanged, this, [this] { refreshParameters(); });
    connect(shadowTls_, &QCheckBox::toggled, this, [this] { refreshParameters(); });
    connect(keySource_, &QComboBox::currentIndexChanged, this, [this, browseCa] {
        refreshParameters();
        browseCa->setVisible(keySource_->currentData() == "Explicitmasque");
    });
    browseCa->setVisible(valueFor(text, "AccessKey", "Type") == "masque");
    connect(GeoDataSelection::instance(), &GeoDataSelection::catalogResult, this,
        [this](const QString &key, const QStringList &groups, const QString &error) {
            for (const QString &kind : {QString("geoip"), QString("geosite")}) if (datasetKeys_.value(kind) == key) {
                datasetGroups_[kind] = groups; datasetErrors_[kind] = error; refreshRouting();
            }
        });
    connect(GeoDataSelection::instance(), &GeoDataSelection::catalogInvalidated, this, [this](const QString &key) {
        for (const QString &kind : {QString("geoip"), QString("geosite")}) if (datasetKeys_.value(kind) == key) {
            datasetChanged_ = true; inspectDataset(kind);
        }
    });
    loadText(text);
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
        vlessForm->setRowVisible(vlessFlow_, (network == "tcp" && reality) || !vlessFlow_->currentText().isEmpty());
        vlessForm->setRowVisible(vlessPublicKey_, reality || !vlessPublicKey_->text().isEmpty());
        vlessForm->setRowVisible(vlessShortId_, reality || !vlessShortId_->text().isEmpty());
        vlessForm->setRowVisible(vlessSpiderX_, reality || !vlessSpiderX_->text().isEmpty());
        vlessForm->setRowVisible(vlessAlpn_, !(network == "tcp" && reality));
        vlessForm->setRowVisible(vlessPath_, network == "ws" || network == "xhttp");
        vlessForm->setRowVisible(vlessHost_, network == "ws" || network == "xhttp");
        vlessForm->setRowVisible(vlessEarlyData_, network == "ws" || vlessEarlyData_->value() != 0);
        vlessForm->setRowVisible(vlessServiceName_, network == "grpc" || !vlessServiceName_->text().isEmpty());
        vlessForm->setRowVisible(vlessAuthority_, network == "grpc" || !vlessAuthority_->text().isEmpty());
        vlessForm->setRowVisible(vlessMultiMode_, network == "grpc" || vlessMultiModeConfigured_ || vlessMultiMode_->isChecked());
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
        const bool added = source == "ExplicitTrojan" || QStringList{"Explicithttp", "Explicithttps", "Explicitnaive", "Explicitmasque"}.contains(source);
        const bool showParameters = ss || explicitKey || explicitSocks5 || explicitVless || explicitSsh || added;
        mode_->setToolTip(ssh ? tr("SSH VPN carries TCP and captured DNS only; other proxied UDP is dropped.") : QString());

        keyLabel->setText(file ? tr("File path") : tr("URL"));
        key_->setEnabled(true);
        generalForm->setRowVisible(keyRow, !explicitKey && !explicitSocks5 && !explicitVless && !explicitSsh && !added);
        key_->setVisible(file || keyRevealed_);
        browse->setVisible(file);
        reveal->setVisible(ss || ssconf || socks5 || source == "VLESS" || source == "Hysteria2" || source == "SSH" || source == "Key");
        reveal->setText(keyRevealed_ ? tr("Hide") : tr("Show"));

        generalForm->setRowVisible(server_, showParameters);
        generalForm->setRowVisible(port_, showParameters);
        generalForm->setRowVisible(method_, ss || explicitKey);
        generalForm->setRowVisible(username_, explicitSocks5 || explicitSsh || (added && source != "ExplicitTrojan"));
        generalForm->setRowVisible(identityRow, ssh);
        generalForm->setRowVisible(hostsRow, ssh);
        generalForm->setRowVisible(password_, showParameters && !explicitVless && !explicitSsh);
        generalForm->setRowVisible(prefix_, ss || explicitKey);
        generalForm->setRowVisible(ssPlugin_, explicitKey);
        generalForm->setRowVisible(ssPluginOpts_, explicitKey);
        for (auto *widget : {static_cast<QWidget *>(server_), static_cast<QWidget *>(port_), static_cast<QWidget *>(method_), static_cast<QWidget *>(username_), static_cast<QWidget *>(password_), static_cast<QWidget *>(prefix_)})
            widget->setEnabled(explicitKey || explicitSocks5 || explicitVless || explicitSsh || added);
        if (ss) applySs(key_->text());
        if (explicitKey && port_->value() == 0) port_->setValue(8388);
        if (explicitSocks5 && port_->value() == 0) port_->setValue(1080);
        if (explicitVless && port_->value() == 0) port_->setValue(443);
        if (explicitSsh && port_->value() == 0) port_->setValue(22);
        updateVlessControls();
        refreshParameters();
    };
    connect(keySource_, &QComboBox::currentIndexChanged, this, [this, updateKeyControls] {
        keyRevealed_ = false;
        updateKeyControls();
    });
    const auto updateSourceLock = [this] {
        const QString source = keySource_->currentData().toString();
        const QString accessKey = source.startsWith("Explicit")
            ? "Type = " + QString(source == "ExplicitVLESS" ? "vless" : source == "ExplicitSOCKS5" ? "socks5" : source == "ExplicitSSH" ? "ssh" : source == "ExplicitTrojan" ? "trojan" : source == "Explicit" ? "shadowsocks" : source.mid(8))
            : source + " = " + key_->text();
        keySource_->setEnabled(!bridgeReady_ && !ProfileStore::connectionInfo("[AccessKey]\n" + accessKey).connectable);
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
    if (source == "Explicitnaive") return {"ServerName"};
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
    findChild<QPushButton *>("browseParameterCa")->setVisible(keySource_->currentData() == "Explicitmasque");
    const auto fields = parameterFields();
    for (auto it = parameters_.cbegin(); it != parameters_.cend(); ++it)
        form->setRowVisible(it.value(), fields.contains(it.key()) || !it.value()->text().isEmpty());
    form->setRowVisible(trojanNetwork_, keySource_->currentData() == "ExplicitTrojan");
    shadowName_->setEnabled(shadowTls_->isChecked());
    shadowPassword_->setEnabled(shadowTls_->isChecked());
    const QString source = keySource_->currentData().toString();
    const bool wrapper = source == "Explicit" || source == "ExplicitVLESS" || source == "ExplicitSSH"
        || source == "ExplicitTrojan" || source == "Explicitnaive";
    const bool compatible = wrapper && !(source == "Explicit" && (method_->currentText() == "aes-192-gcm"
        || !ssPlugin_->currentText().isEmpty()))
        && !(source == "ExplicitVLESS" && vlessAlpn_->text().split(',').contains("h3"));
    shadowTls_->setEnabled(compatible || shadowTls_->isChecked());
    const bool added = source == "ExplicitTrojan" || source == "Explicitnaive" || source == "Explicitmasque"
        || source == "Explicithttp" || source == "Explicithttps";
    tabs_->setTabVisible(tabs_->indexOf(parameterPage_), added || wrapper || shadowTls_->isChecked());
}

QString ProfileEditorDialog::parameterError() const {
    const QString source = keySource_->currentData().toString();
    const bool added = source == "ExplicitTrojan" || QStringList{"Explicithttp", "Explicithttps", "Explicitnaive", "Explicitmasque"}.contains(source);
    const auto invalid = [this] { return tr("Unsupported parameter combination. Correct the settings or use protected Raw config; no values were discarded."); };
    if (added) {
        if (server_->text().isEmpty() || port_->value() < 1) return invalid();
        const auto allowed = parameterFields();
        for (auto it = parameters_.cbegin(); it != parameters_.cend(); ++it) {
            if ((!allowed.contains(it.key()) && !it.value()->text().isEmpty())
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
    if (value.isEmpty()) { source = "Key"; value = fieldValue("AccessKey", "Key"); }
    if (value.isEmpty()) { source = "File"; value = fieldValue("AccessKey", "File"); }
    if (value.isEmpty() && !fieldValue("AccessKey", "Server").isEmpty()) {
        const QString type = fieldValue("AccessKey", "Type");
        source = type == "socks5" ? "ExplicitSOCKS5" : type == "vless" ? "ExplicitVLESS" : type == "ssh" ? "ExplicitSSH"
            : type == "trojan" ? "ExplicitTrojan"
            : QStringList{"http", "https", "naive", "masque"}.contains(type) ? "Explicit" + type
            : (type == "shadowsocks" || type == "ss") ? "Explicit" : QString();
        if (source.isEmpty()) rawOnly_ = true;
    }
    keySource_->setCurrentIndex(keySource_->findData(source)); key_->setText(value);
    server_->setText(fieldValue("AccessKey", "Server"));
    port_->setValue(fieldValue("AccessKey", "Port").toInt());
    const int methodIndex = method_->findText(fieldValue("AccessKey", "Method"));
    method_->setCurrentIndex(methodIndex);
    if (source == "Explicit" && methodIndex < 0) rawOnly_ = true;
    for (auto it = parameters_.begin(); it != parameters_.end(); ++it)
        it.value()->setText(QStringList{"ExplicitTrojan", "Explicithttp", "Explicithttps", "Explicitnaive", "Explicitmasque"}.contains(source) ? fieldValue("AccessKey", it.key()) : QString());
    trojanNetwork_->setCurrentIndex(trojanNetwork_->findText(fieldValue("AccessKey", "Network")));
    shadowTls_->setChecked(text.contains("[ShadowTLS]"));
    shadowName_->setText(fieldValue("ShadowTLS", "ServerName"));
    shadowPassword_->setText(fieldValue("ShadowTLS", "Password"));
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
    dns_->setText(fieldValue("Interface", "DNS").isEmpty() ? "1.1.1.1" : fieldValue("Interface", "DNS"));
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
        {"AccessKey", {"SS", "SSConf", "SOCKS5", "VLESS", "Hysteria2", "SSH", "Key", "File", "Type", "Server", "Port", "Method", "Username", "Password", "Prefix", "Plugin", "PluginOpts", "IdentityFile", "KnownHostsFile", "CAFile", "MASQUEPath", "ID", "Encryption", "Flow", "Network", "Security", "ServerName", "PublicKey", "ShortID", "Fingerprint", "SpiderX", "ALPN", "WSPath", "WSHost", "WSEarlyData", "GRPCServiceName", "GRPCAuthority", "GRPCMultiMode", "XHTTPPath", "XHTTPHost", "XHTTPMode"}},
        {"ShadowTLS", {"ServerName", "Password"}},
        {"Interface", {"Mode", "Name", "MTU", "DNS", "IPv6", "KillSwitch", "Listen"}},
        {"Routing", {"Default"}}, {"Rule", {"CIDR", "Domain", "Action"}}};
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
            if (!known.value(currentSection).contains(key)) {
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
    auto *choose = new QAction(QIcon(arrowIcon), tr("Choose groups…"), target);
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
    if (selected == "ExplicitTrojan" || QStringList{"Explicithttp", "Explicithttps", "Explicitnaive", "Explicitmasque"}.contains(selected)) {
        const QString type = selected == "ExplicitTrojan" ? "trojan" : selected.mid(8);
        accessKey = "Type = " + type + "\nServer = " + server_->text() + "\nPort = " + QString::number(port_->value());
        if (type != "trojan" && !username_->text().isEmpty()) accessKey += "\nUsername = " + username_->text();
        if (!password_->text().isEmpty()) accessKey += "\nPassword = " + password_->text();
        if (type == "trojan" && !trojanNetwork_->currentText().isEmpty()) accessKey += "\nNetwork = " + trojanNetwork_->currentText();
        // Serialize inactive configured values too: the validation guard blocks
        // incompatible edits without silently deleting the user's draft.
        for (auto it = parameters_.cbegin(); it != parameters_.cend(); ++it)
            if (!it.value()->text().isEmpty()) accessKey += "\n" + it.key() + " = " + it.value()->text();
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
            + (method_->currentText().startsWith("2022-") && prefix_->text().isEmpty() ? QString() : "\nPrefix = " + prefix_->text())
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
    else
        accessKey = keySource_->currentData().toString() + " = " + key_->text();
    const QString source = keySource_->currentData().toString();
    if (source == "SSH" || source == "ExplicitSSH" || (source == "Key" && QUrl(key_->text()).scheme() == "ssh")) {
        if (!sshIdentity_->text().isEmpty()) accessKey += "\nIdentityFile = " + sshIdentity_->text();
        if (!sshKnownHosts_->text().isEmpty()) accessKey += "\nKnownHostsFile = " + sshKnownHosts_->text();
    }
    if (shadowTls_->isChecked())
        accessKey += "\n\n[ShadowTLS]\nServerName = " + shadowName_->text() + "\nPassword = " + shadowPassword_->text();
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
    if (!bridgeReady_ && ignoreLocalNetwork_->isChecked())
        text += QString("\n[Rule]\nCIDR = %1\nAction = direct\n").arg(localNetworkCidrs());
    for (int row = 0; row < rules_->rowCount(); ++row) {
        const QString prefix = ruleEnabled(rules_, row)->isChecked() ? QString() : "# ";
        const QString type = static_cast<QComboBox *>(rules_->cellWidget(row, 1))->currentData().toString();
        const QString target = tableRule(row).target;
        text += QString("\n%1[Rule]\n%1%2 = %3\n%1Action = %4\n")
            .arg(prefix, type, type == "CIDR" ? normalizedCidrs(target) : target.trimmed(), static_cast<QComboBox *>(rules_->cellWidget(row, 3))->currentData().toString());
    }
    return text;
}

void ProfileEditorDialog::assessRepresentation() {
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
    for (int row = 0; row < general->rowCount(); ++row) general->setRowVisible(row, true);
    for (int i = 1; i < tabs_->count() - 1; ++i) tabs_->setTabVisible(i, true);
    if (refreshControls_) refreshControls_();
    general->setRowVisible(windowsProfile_, native_);
    general->setRowVisible(nativeServerName_, native_);
    general->setRowVisible(nativeKillSwitch_, native_);
    general->setRowVisible(nativeMode_, native_);
    general->setRowVisible(nativeProfileActions_, native_);
    general->setRowVisible(nativeAuthentication_, native_);
    nativeAuthMethod_->setCurrentIndex(0);
    nativeAuthSource_->setCurrentIndex(0);
    refreshNativeAuthentication_();
    if (native_) {
        for (int row = 0; row < general->rowCount(); ++row) {
            auto *item = general->itemAt(row, QFormLayout::FieldRole);
            auto *widget = item ? item->widget() : nullptr;
            general->setRowVisible(row, widget == name_ || widget == windowsProfile_
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
        return;
    }
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
    if (bridgeReady_ && !bridgeError_.isEmpty() && !rawOnly_) {
        routingStatus_->setText(bridgeError_);
        findChild<QScrollArea *>("routingStatusScroll")->show();
        return;
    }
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
    if (rawOnly_) return loadedText_;
    if (bridgeReady_) return documentText();
    if (buildText() == visualBaseline_) return loadedText_;
    return buildText();
}

void ProfileEditorDialog::reject() {
    bool reordered = false;
    int previousIndex = -1;
    for (int row = 0; row < rules_->rowCount(); ++row) {
        const int index = tableRule(row).index;
        if (index >= 0 && index < previousIndex) reordered = true;
        if (index >= 0) previousIndex = index;
    }
    if (reordered || buildText() != visualBaseline_ || configText() != originalText_) {
        QMessageBox prompt(QMessageBox::Question, tr("Discard changes"), tr("Discard unsaved changes?"), QMessageBox::NoButton, this);
        auto *discard = prompt.addButton(tr("Discard changes"), QMessageBox::DestructiveRole);
        prompt.addButton(tr("Continue editing"), QMessageBox::RejectRole);
        prompt.exec();
        if (prompt.clickedButton() != discard) return;
    }
    QDialog::reject();
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
    const auto before = visualFields(visualBaseline_), after = visualFields(visual);
    for (auto it = before.cbegin(); it != before.cend(); ++it) {
        if (!after.contains(it.key())) {
            bridgeError_ = tr("Removing a configured field requires Raw config. The draft has been retained.");
            return loadedText_;
        }
    }
    for (auto it = after.cbegin(); it != after.cend(); ++it) {
        if (before.contains(it.key()) && before.value(it.key()) == it.value()) continue;
        const int slash = it.key().indexOf('/');
        operations.append(QJsonObject{{"op", "set_field"}, {"section", it.key().left(slash)},
                          {"key", it.key().mid(slash + 1)}, {"value", it.value()}});
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
            operations.append(QJsonObject{{"op", "set_field"}, {"section", "GeoData"}, {"key", key}, {"value", it.value()}});
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
            operations.append(QJsonObject{{"op", "set_rule"}, {"index", row}, {"matcher", rule.matcher}, {"value", target}, {"action", rule.action}});
        if (rule.enabled != old.value("enabled").toBool())
            operations.append(QJsonObject{{"op", "toggle"}, {"index", row}, {"enabled", rule.enabled}});
    }
    if (operations.isEmpty()) return loadedText_;
    const auto result = document(operations, &bridgeError_);
    if (!bridgeError_.isEmpty()) return loadedText_;
    cachedVisual_ = cacheKey;
    cachedDocument_ = result.value("profile").toString();
    return cachedDocument_;
}

QString ProfileEditorDialog::routingError() const {
    if (coreExecutable_.isEmpty()) return {};
    if (rawDirty_ || rawOnly_) return {};
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
        if (!datasetErrors_.value(kind).isEmpty()) return prefix + datasetErrors_.value(kind);
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
    const QString error = routingError();
    QString text = error;
    if (text.isEmpty() && datasetChanged_) text = tr("New databases take effect on the next connection.");
    routingStatus_->setToolTip(unavailable.join('\n'));
    routingStatus_->setText(text);
    auto *statusScroll = findChild<QScrollArea *>("routingStatusScroll");
    statusScroll->hide();
}

void ProfileEditorDialog::inspectDataset(const QString &kind) {
    ++datasetGeneration_[kind];
    datasetGroups_.remove(kind); datasetKeys_.remove(kind);
    const QString path = datasetPaths_.value(kind);
    if (path.isEmpty()) {
        datasetErrors_[kind] = tr("No shared database selected. Open Manage databases to set it up.");
        refreshRouting(); return;
    }
    if (coreExecutable_.isEmpty() || (QDir::isRelativePath(path) && profilePath_.isEmpty())) {
        datasetErrors_[kind] = tr("Select a Core executable and final profile path before validating datasets.");
        refreshRouting(); return;
    }
    const QString absolute = QDir::isAbsolutePath(path) ? QFileInfo(path).absoluteFilePath()
        : QFileInfo(QFileInfo(profilePath_).absoluteDir(), path).absoluteFilePath();
    datasetKeys_[kind] = GeoDataSelection::catalogKey(coreExecutable_, kind, absolute);
    datasetErrors_[kind] = tr("Checking file…"); refreshRouting();
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
    popup.setWindowTitle(tr("Choose groups…"));
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
