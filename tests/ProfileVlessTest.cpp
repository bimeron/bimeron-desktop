#include "GeoIPLabels.h"
#include "ProfileEditorDialog.h"
#include "GeoSitePicker.h"
#include <QSettings>
#include <QCryptographicHash>
#include "GeoDataSelection.h"
#include "ProfileStore.h"
#include "CoreDiagnostic.h"
#include "RoutingCompatibility.h"

#include <QApplication>
#include <QScrollArea>
#include <QAction>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QJsonArray>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QTabWidget>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QDir>
#include <QJsonDocument>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <aclapi.h>
#endif
#include <QTimer>
#include <QTranslator>

#include <cstdlib>
#include <cstdio>

namespace {
void requireAt(bool condition, int line) { if (!condition) { std::fprintf(stderr, "Profile regression failed at line %d\n", line); std::fflush(stderr); qFatal("Profile regression failed at line %d", line); } }
#define require(condition) requireAt(bool(condition), __LINE__)

void check(const QString &profile, const QStringList &fields, const QString &core, const QString &path) {
    ProfileEditorDialog editor(profile, QJsonObject{{"vpn", true}, {"proxy", true}, {"ip_split", true}, {"domain_split", true}});
    const QString output = editor.configText();
    for (const QString &field : fields) require(output.contains(field));
    if (core.isEmpty()) return;
    QFile file(path);
    require(file.open(QIODevice::WriteOnly));
    require(file.write(output.toUtf8()) == output.toUtf8().size());
    file.close();
    QProcess process;
    process.start(core, {"validate", path});
    require(process.waitForStarted(5000) && process.waitForFinished(20000));
    if (process.exitCode() != 0) qWarning("Core rejected fixture: %s", qPrintable(QFileInfo(path).fileName()));
    require(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0);
    QFile reopened(path);
    require(reopened.open(QIODevice::ReadOnly));
    ProfileEditorDialog reopenedEditor(QString::fromUtf8(reopened.readAll()),
        QJsonObject{{"vpn", true}, {"proxy", true}, {"ip_split", true}, {"domain_split", true}});
    require(reopenedEditor.configText() == output);
}

void rejectWithCore(const QString &uri, const QString &core, const QString &path, const QString &expected = {}) {
    if (core.isEmpty()) return;
    QFile file(path);
    require(file.open(QIODevice::WriteOnly));
    const QByteArray profile = ("[Profile]\nVersion = 1\nName = Invalid\n[AccessKey]\nHysteria2 = " + uri + "\n").toUtf8();
    require(file.write(profile) == profile.size());
    file.close();
    QProcess process;
    process.start(core, {"validate", path});
    require(process.waitForStarted(5000) && process.waitForFinished(20000));
    require(process.exitStatus() == QProcess::NormalExit && process.exitCode() != 0);
    const QString diagnostic = QString::fromUtf8(process.readAllStandardError() + process.readAllStandardOutput());
    require(!diagnostic.contains("secret") && (expected.isEmpty() || diagnostic.contains(expected)));
}
}

static void preservationRegressions(const QString &directory) {
    const QJsonObject caps{{"vpn", true}, {"proxy", true}, {"ip_split", true}, {"domain_split", true}};
    const QString base = "[Profile]\nVersion = 1\nName = Preserve\n[AccessKey]\nType = shadowsocks\nServer = example.invalid\nPort = 443\nMethod = aes-256-gcm\nPassword = SENTINEL\n";
    QStringList inputs{
        QString(base).replace("aes-256-gcm", "unknown-cipher"),
        QString(base).replace("Type = shadowsocks", "Type = trojan"),
        QString(base).replace("Type = shadowsocks\n", ""),
        base + "Plugin = unknown-plugin\nPluginOpts = tls;mux=0;path=/ws\n",
        base + "[ShadowTLS]\nServerName = example.invalid\nPassword = OUTER_SENTINEL\nUnknown = true\n",
        base + "[Future]\nOpaque = FUTURE_SENTINEL\n",
        base + "Password = DUPLICATE_SENTINEL\n",
        base + "Key = future://SENTINEL\n",
        "[Profile]\nVersion = 1\nName = Mixed\n[AccessKey]\nSS = ss://aes-256-gcm:secret@example.invalid:443\nPlugin = v2ray-plugin\n",
        base + "Plugin = v2ray-plugin\nPluginOpts = \"quoted-options\"\n",
        QString(base).replace("Password = SENTINEL", "Password = \" padded=secret;# \""),
        base + "[Interface]\nMTU = 2000\n",
        base + "[Routing]\nDefault = future\n",
        base + "[Rule]\nDomain = example.invalid\nAction = future\n",
        "[Profile]\nVersion = 1\nName = Native\n[IKEv2]\nWindowsProfile = Native\nServerName = example.invalid\nUnknown = retained\n"
    };
    for (const QString &method : {"2022-blake3-aes-128-gcm", "2022-blake3-aes-256-gcm", "2022-blake3-chacha20-poly1305"}) {
        const QString input = QString(base).replace("aes-256-gcm", method);
        ProfileEditorDialog editor(input, caps);
        require(editor.findChild<QTabWidget *>()->isTabEnabled(0));
        require(editor.configText() == input);
        editor.findChild<QLineEdit *>("profileName")->setText("Edited");
        require(editor.configText().contains("Method = " + method));
        require(!editor.configText().contains("Prefix ="));
        inputs.append(input + "Prefix = incompatible\n");
    }
    for (const QString &plugin : {"v2ray-plugin", "obfs-local"}) {
        const QString input = base + "Plugin = " + plugin + "\nPluginOpts = "
            + (plugin == "v2ray-plugin" ? "tls=false;mux=0;path=/a\\;b" : "obfs=tls") + "\n";
        ProfileEditorDialog editor(input, caps);
        require(editor.findChild<QTabWidget *>()->isTabEnabled(0));
        require(editor.configText() == input);
        editor.findChild<QLineEdit *>("profileName")->setText("Edited");
        require(editor.configText().contains("PluginOpts = " + editor.findChild<QLineEdit *>("ssPluginOpts")->text()));
        auto *options = editor.findChild<QLineEdit *>("ssPluginOpts");
        require(options->echoMode() == QLineEdit::Password);
        options->setText(plugin == "v2ray-plugin" ? "mux=8;path=/new" : "obfs=http;http-method=POST");
        const QString edited = editor.configText();
        ProfileEditorDialog reopened(edited, caps);
        require(reopened.configText() == edited);
        require(reopened.findChild<QLineEdit *>("ssPluginOpts")->text() == options->text());
    }
    // Changing a legacy profile to SS2022 must retain Prefix until explicitly cleared.
    ProfileEditorDialog conflict(base + "Prefix = retained\n", caps);
    conflict.findChild<QComboBox *>("method")->setCurrentText("2022-blake3-aes-128-gcm");
    bool explained = false;
    QTimer::singleShot(0, [&] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box);
        explained = box->text().contains("Prefix") && !box->text().contains("SENTINEL");
        box->accept();
    });
    conflict.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    require(explained && conflict.result() != QDialog::Accepted);
    require(conflict.configText().contains("Prefix = retained"));
    for (const QString &source : {"VMess", "Trojan", "AnyTLS", "TUIC", "SSH", "HTTP", "Naive", "MASQUE"})
        inputs.append("[Profile]\nName = Future\n[AccessKey]\n" + source + " = future://SENTINEL\n");
    for (const auto &input : inputs) {
        ProfileEditorDialog editor(input, caps);
        auto *tabs = editor.findChild<QTabWidget *>();
        auto *raw = editor.findChild<QPlainTextEdit *>("rawConfig");
        require(!tabs->isTabEnabled(0));
        require(tabs->currentIndex() == tabs->count() - 1);
        require(editor.configText() == input);
        require(!raw->isHidden());
        require(raw->toPlainText() == input);
        require(!editor.findChild<QPushButton *>("showRaw"));
        require(!editor.findChild<QPushButton *>("useVisual"));
        editor.findChild<QLineEdit *>("profileName")->setText("programmatic visual overwrite");
        require(editor.configText() == input);
        raw->appendPlainText("# intentional Raw edit");
        require(editor.configText().contains(input.trimmed()));
    }
    const QString uri = "[Profile]\nName = URI\n[AccessKey]\nKey = vless://123e4567-e89b-12d3-a456-426614174000@example.invalid:443?type=xhttp&security=tls&extra=%7B%22headers%22:%7B%22Authorization%22:%22SENTINEL%22%7D%7D\n";
    for (QString input : {base, uri}) {
        input.replace("\n", "\r\n");
        input.prepend(QChar(0xfeff));
        ProfileEditorDialog editor(input, caps);
        auto *tabs = editor.findChild<QTabWidget *>();
        require(tabs->isTabEnabled(0));
        tabs->setCurrentIndex(tabs->count() - 1);
        tabs->setCurrentIndex(0);
        require(editor.configText() == input);
        editor.findChild<QLineEdit *>("profileName")->setText("Changed");
        require(editor.configText().contains("Name = Changed"));
        require(editor.configText().contains(input.contains("Key =") ? "Key = vless://" : "Password = SENTINEL"));
    }
    ProfileEditorDialog recovery(inputs.first(), caps, nullptr, [](const QString &, QString *) { return true; });
    recovery.findChild<QPlainTextEdit *>("rawConfig")->setPlainText(base);
    recovery.findChild<QTabWidget *>()->setCurrentIndex(0);
    require(recovery.findChild<QTabWidget *>()->isTabEnabled(0));
    require(recovery.configText() == base);
    ProfileEditorDialog rejected(inputs.first(), caps, nullptr, [](const QString &, QString *error) {
        *error = "future://SENTINEL";
        return false;
    });
    auto *rejectedRaw = rejected.findChild<QPlainTextEdit *>("rawConfig");
    rejectedRaw->appendPlainText("# retain invalid draft");
    const QString rejectedDraft = rejected.configText();
    bool safeError = false;
    QTimer::singleShot(0, [&] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box);
        safeError = !box->text().contains("SENTINEL");
        box->accept();
    });
    rejected.findChild<QTabWidget *>()->setCurrentIndex(0);
    require(safeError && rejected.configText() == rejectedDraft);
    safeError = false;
    QTimer::singleShot(0, [&] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box);
        safeError = !box->text().contains("SENTINEL");
        box->accept();
    });
    rejected.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    require(safeError && rejected.result() != QDialog::Accepted && rejected.configText() == rejectedDraft);
    require(rejected.findChild<QTabWidget *>()->currentIndex() == rejected.findChild<QTabWidget *>()->count() - 1);
    const QString unavailable = base + "[Interface]\nMode = vpn\nIPv6 = disable\nKillSwitch = true\n";
    ProfileEditorDialog unavailableEditor(unavailable, QJsonObject{{"proxy", true}});
    require(unavailableEditor.configText() == unavailable);
    require(!unavailableEditor.findChild<QTabWidget *>()->isTabEnabled(0));

    const QString path = directory + "/atomic.conf";
    QString error;
    require(ProfileStore::writeConfig(path, base.toUtf8(), &error));
    QFile original(path);
    require(original.open(QIODevice::ReadOnly));
    require(original.readAll() == base.toUtf8());
    original.close();
#ifdef Q_OS_WIN
    PACL acl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    require(GetNamedSecurityInfoW(const_cast<wchar_t *>(native.c_str()), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &acl, nullptr, &descriptor) == ERROR_SUCCESS);
    SECURITY_DESCRIPTOR_CONTROL control{}; DWORD revision = 0;
    require(GetSecurityDescriptorControl(descriptor, &control, &revision));
    require(control & SE_DACL_PROTECTED);
    require(acl && acl->AceCount == 2);
    require(ImpersonateAnonymousToken(GetCurrentThread()));
    HANDLE anonymous = nullptr;
    const bool opened = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &anonymous);
    RevertToSelf();
    require(opened);
    GENERIC_MAPPING mapping{FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
    PRIVILEGE_SET privileges{}; DWORD privilegeSize = sizeof(privileges), granted = 0;
    BOOL allowed = FALSE;
    require(AccessCheck(descriptor, anonymous, FILE_READ_DATA, &mapping, &privileges,
        &privilegeSize, &granted, &allowed));
    require(!allowed);
    CloseHandle(anonymous);
    LocalFree(descriptor);
    HANDLE lock = CreateFileW(native.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    require(lock != INVALID_HANDLE_VALUE);
    require(!ProfileStore::writeConfig(path, "replacement", &error));
    CloseHandle(lock);
#else
    require((QFile::permissions(path) & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther)) == 0);
    require(!ProfileStore::writeConfig(directory + "/missing/atomic.conf", "replacement", &error));
    const QString locked = directory + "/locked";
    require(QDir().mkdir(locked));
    require(ProfileStore::writeConfig(locked + "/profile.conf", base.toUtf8(), &error));
    require(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    if (!QFileInfo(locked).isWritable())
        require(!ProfileStore::writeConfig(locked + "/profile.conf", "replacement", &error));
    require(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    QFile retained(locked + "/profile.conf");
    require(retained.open(QIODevice::ReadOnly) && retained.readAll() == base.toUtf8());
#endif
    require(original.open(QIODevice::ReadOnly));
    require(original.readAll() == base.toUtf8());
    original.close();
    ProfileStore store(directory + "/imported");
    const QByteArray imported = QByteArray::fromHex("efbbbf") + QByteArray(base.toUtf8()).replace("\n", "\r\n");
    require(ProfileStore::writeConfig(path, imported, &error));
    const QString copy = store.importConfig(path, &error);
    require(!copy.isEmpty());
    QFile copied(copy); require(copied.open(QIODevice::ReadOnly)); require(copied.readAll() == imported);
    for (const QString &field : {"File = key.txt", "IdentityFile = identity", "KnownHostsFile = hosts", "CAFile = ca.pem", "SSH = ssh://alice@example.invalid?identity=relative", "Key = ssh://alice@example.invalid?known_hosts=relative", "MASQUE = masque://example.invalid?ca=relative"}) {
        require(!ProfileStore::importReferencesSafe(("[AccessKey]\n" + field + "\n").toUtf8(), &error));
    }
    const QString relativeSource = directory + "/relative.conf";
    require(ProfileStore::writeConfig(relativeSource, "[Profile]\nName = Relative\n[AccessKey]\nFile = key.txt\n", &error));
    const auto before = QDir(directory + "/imported").entryList(QDir::Files | QDir::Hidden);
    require(store.importConfig(relativeSource, &error).isEmpty());
    require(QDir(directory + "/imported").entryList(QDir::Files | QDir::Hidden) == before);
    const QString absoluteFile = directory + "/key-uri.txt";
    require(ProfileStore::writeConfig(absoluteFile, "ssh://alice@example.invalid?identity=relative", &error));
    require(!ProfileStore::importReferencesSafe(("[AccessKey]\nFile = " + absoluteFile + "\n").toUtf8(), &error));
    const QString embeddedBom = "ssh://alice@example.invalid?identity=" + QString(QChar(0xfeff)) + "/relative-to-profile";
    require(ProfileStore::writeConfig(absoluteFile, embeddedBom.toUtf8(), &error));
    require(!ProfileStore::importReferencesSafe(("[AccessKey]\nFile = " + absoluteFile + "\n").toUtf8(), &error));
    require(ProfileStore::importReferencesSafe(base.toUtf8(), &error));
    for (const QByteArray &secret : {QByteArray("SENTINEL"), QByteArray("aGVsbG8=:d29ybGQ="), QByteArray("eyJpZCI6IlNFTlRJTkVMIn0=")}) {
        for (const QByteArray &prefix : {QByteArray("ss://"), QByteArray("ssconf://"), QByteArray("socks5://"),
                QByteArray("vless://"), QByteArray("vmess://"), QByteArray("trojan://"), QByteArray("anytls://"),
                QByteArray("tuic://"), QByteArray("ssh://"), QByteArray("http://"), QByteArray("https://"),
                QByteArray("hysteria2://"), QByteArray("hy2://"), QByteArray("masque://"),
                QByteArray("future://"), QByteArray("naive+https://"), QByteArray("naive+quic://"),
                QByteArray("Password = "), QByteArray("{\"answers\":[\""), QByteArray("XHTTPExtra = {\"Authorization\":\"")}) {
            require(!safeCoreDiagnostic(prefix + secret).contains(QString::fromUtf8(secret)));
            require(!safeCoreDiagnostic(QByteArray(999, 'x') + prefix + secret).contains(QString::fromUtf8(secret)));
        }
    }
    require(safeCoreCode("invalid_auth_response") == "invalid_auth_response");
    require(safeCoreEvent("state", "connected") == "state: connected");
    require(!safeCoreEvent("state", "SENTINEL").contains("SENTINEL"));
    require(safeCoreCode("SENTINEL") == "redacted_error");
    require(!safeLogMessage("future://SENTINEL").contains("SENTINEL"));
    require(!safeLogMessage("Password = SENTINEL").contains("SENTINEL"));
    const auto report = safeDiagnosticReport(R"({"schema":1,"platform":"windows","status":{"state":"connected","last_error":"SENTINEL"},"auth":{"answers":["SENTINEL"]},"events":[{"code":"SENTINEL","error":"SENTINEL"}],"unknown":"SENTINEL"})");
    require(!report.isEmpty() && !report.contains("SENTINEL"));
    require(safeDiagnosticReport("invalid SENTINEL").isEmpty());
    require(safeDiagnosticReport("{\"schema\":2}").isEmpty());
    require(safeDiagnosticReport(QByteArray(256 * 1024 + 1, ' ')).isEmpty());
    const QString large = base + "[Future]\nOpaque = " + QString(64 * 1024, 'x');
    ProfileEditorDialog largeEditor(large, caps);
    require(largeEditor.configText() == large);

    const QString contractCore = qEnvironmentVariable("BIMERON_PRESERVATION_CORE");
    if (!contractCore.isEmpty()) {
        const QString key16 = "AAAAAAAAAAAAAAAAAAAAAA==", key32 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
        QStringList fixtures{
            QString(base).replace("aes-256-gcm", "2022-blake3-aes-128-gcm").replace("SENTINEL", key16 + ":" + key16),
            QString(base).replace("aes-256-gcm", "2022-blake3-aes-256-gcm").replace("SENTINEL", key32 + ":" + key32),
            QString(base).replace("aes-256-gcm", "2022-blake3-chacha20-poly1305").replace("SENTINEL", key32),
            base + "Plugin = v2ray-plugin\nPluginOpts = mode=websocket;tls;host=example.invalid;path=/ws;mux=0\n",
            base + "Plugin = obfs-local\nPluginOpts = obfs=http;obfs-host=example.invalid;obfs-uri=/;http-method=GET\n",
            base + "[ShadowTLS]\nServerName = example.invalid\nPassword = OUTER_SENTINEL\n"
        };
        fixtures.append(base + "Plugin = v2ray-plugin\n");
        fixtures.append(base + "Plugin = v2ray-plugin\nPluginOpts = tls=false;mux=4294967295;path=/a\\;b\n");
        fixtures.append(base + "Plugin = obfs-local\nPluginOpts = obfs=tls\n");
        const QString ssUri = "ss://2022-blake3-aes-128-gcm:" + key16 + ":" + key16
            + "@[2001:db8::1]:443/?plugin=v2ray-plugin%3Btls%3Bmux%3D0#Encoded%20name";
        for (const QString &source : {"SS", "Key"})
            fixtures.append("[Profile]\nVersion = 1\nName = URI\n[AccessKey]\n" + source + " = " + ssUri + "\n");
        QStringList invalid{
            QString(base).replace("aes-256-gcm", "2022-blake3-aes-128-gcm"),
            QString(base).replace("aes-256-gcm", "2022-blake3-chacha20-poly1305").replace("SENTINEL", key32 + ":" + key32),
            fixtures.first() + "Prefix = forbidden\n",
            base + "Plugin = /tmp/plugin\n",
            base + "Plugin = v2ray-plugin\nPluginOpts = tls;tls\n",
            base + "Plugin = v2ray-plugin\nPluginOpts = mux=4294967296\n",
            base + "Plugin = v2ray-plugin\nPluginOpts = unknown=value\n",
            base + "Plugin = obfs-local\n",
            base + "Plugin = obfs-local\nPluginOpts = obfs=tls;http-method=GET\n",
            base + "Plugin = v2ray-plugin\n[ShadowTLS]\nServerName = example.invalid\nPassword = OUTER_SENTINEL\n"
        };
        for (const auto &input : invalid) {
            const QString path = directory + "/invalid-ss.conf";
            require(ProfileStore::writeConfig(path, input.toUtf8()));
            QProcess process;
            process.start(contractCore, {"validate", path});
            require(process.waitForStarted(5000) && process.waitForFinished(20000));
            require(process.exitStatus() == QProcess::NormalExit && process.exitCode() != 0);
            require(!safeCoreDiagnostic(process.readAllStandardError() + process.readAllStandardOutput()).contains("SENTINEL"));
        }
        const QByteArray vmess = R"({"v":"2","add":"example.invalid","port":"443","id":"123e4567-e89b-12d3-a456-426614174000","aid":"0","scy":"auto","net":"tcp","tls":"tls","sni":"example.invalid"})";
        const QStringList sources{
            "VMess = vmess://" + QString::fromLatin1(vmess.toBase64()),
            "Trojan = trojan://SENTINEL@example.invalid:443",
            "AnyTLS = anytls://SENTINEL@example.invalid:443",
            "TUIC = tuic://123e4567-e89b-12d3-a456-426614174000:SENTINEL@example.invalid:443?udp_relay_mode=quic",
            "SSH = ssh://alice@example.invalid:22",
            "HTTP = http://alice:SENTINEL@example.invalid:8080",
            "HTTP = https://alice:SENTINEL@example.invalid:443",
            "Naive = naive+https://alice:SENTINEL@example.invalid:443",
            "Naive = naive+quic://alice:SENTINEL@example.invalid:443",
            "MASQUE = masque://alice:SENTINEL@example.invalid:443"
        };
        ProfileStore targetStore(directory + "/target-import");
        for (const auto &source : sources) {
            if (source.startsWith("SSH")) continue;
            const QString uri = source.mid(source.indexOf(" = ") + 3);
            QString error;
            const QString path = targetStore.createConfig("Target Core", uri, &error);
            require(!path.isEmpty());
            QFile file(path);
            require(file.open(QIODevice::ReadOnly));
            const QString created = QString::fromUtf8(file.readAll());
            check(created, {uri}, contractCore, path + ".validated");
        }
        for (const auto &source : sources)
            fixtures.append("[Profile]\nVersion = 1\nName = Preserve\n[AccessKey]\n" + source + "\n");
        for (const QString &uri : {"trojan://SENTINEL@host:443?security=none",
                "tuic://bad:SENTINEL@host:443", "anytls://SENTINEL@host?insecure=1",
                "naive+https://alice:SENTINEL@host?insecure=1", "masque://host?unknown=SENTINEL",
                "vmess://not-base64"}) {
            const QString field = ProfileStore::uriSourceField(uri);
            const QString path = directory + "/invalid-protocol.conf";
            require(ProfileStore::writeConfig(path, ("[Profile]\nVersion = 1\nName = Invalid\n[AccessKey]\n" + field + " = " + uri + "\n").toUtf8()));
            QProcess process;
            process.start(contractCore, {"validate", path});
            require(process.waitForStarted(5000) && process.waitForFinished(20000));
            require(process.exitStatus() == QProcess::NormalExit && process.exitCode() != 0);
            require(!safeCoreDiagnostic(process.readAllStandardError() + process.readAllStandardOutput()).contains("SENTINEL"));
        }
        int index = 0;
        for (const auto &fixture : fixtures)
            check(fixture, {fixture}, contractCore, directory + "/contract-" + QString::number(index++) + ".conf");
    }
}

static void sshProfiles(const QString &directory) {
    ProfileStore store(directory + "/ssh-profiles");
    const QString uri = "ssh://alice@[2001:db8::1]?identity=%2Ftmp%2Fkey&known_hosts=%2Ftmp%2Fhosts#SSH";
    QString error;
    const QString path = store.createConfig("SSH", uri, &error);
    require(!path.isEmpty());
    QFile file(path);
    require(file.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(file.readAll());
    require(text.contains("SSH = " + uri) && text.contains("Mode = proxy"));
    const QJsonObject caps{{"vpn", true}, {"proxy", true}};
    for (QString input : {text, QString(text).replace("SSH =", "Key ="),
        QString("[Profile]\nName = SSH\n[AccessKey]\nType = ssh\nServer = example.invalid\nPort = 22\nUsername = alice\n[Interface]\nMode = vpn\n")}) {
        require(ProfileStore::connectionInfo(input).protocol == "SSH");
        require(ProfileStore::connectionInfo(input).connectable);
        ProfileEditorDialog editor(input, caps);
        require(editor.configText() == input);
        require(editor.findChild<QTabWidget *>()->isTabEnabled(0));
    }
    for (const QString &bad : {"ssh://alice:password@example.invalid", "ssh://alice:@example.invalid", "ssh://alice@example.invalid:0", "ssh://alice@example.invalid/path", "ssh://alice@example.invalid?unknown=value"})
        require(!ProfileStore::connectionInfo("[AccessKey]\nSSH = " + bad).connectable);
    require(!ProfileStore::sshFilesReadable(text, path, &error));
    const QString hosts = directory + "/known_hosts";
    require(ProfileStore::writeConfig(hosts, "synthetic-host-key"));
    require(ProfileStore::sshFilesReadable("[AccessKey]\nSSH = ssh://alice@example.invalid\nKnownHostsFile = " + hosts, path, &error));
    const QString core = qEnvironmentVariable("BIMERON_SSH_CORE");
    if (!core.isEmpty()) {
        QProcess process;
        process.start(core, {"validate", path});
        require(process.waitForFinished(15000) && process.exitCode() == 0);
    }
}

static void parameterForms(const QString &directory) {
    const QJsonObject caps{{"vpn", true}, {"proxy", true}};
    const QString head = "[Profile]\nVersion = 1\nName = Parameters\n[AccessKey]\n";
    const QString endpoint = "\nServer = example.invalid\nPort = 443\n";
    const QString wrapper = "\n[ShadowTLS]\nServerName = outer.invalid\nPassword = OUTER_SENTINEL\n";
    const QString core = qEnvironmentVariable("BIMERON_PRESERVATION_CORE");
    int index = 0;
    const QStringList inputs{
        "Type = trojan" + endpoint + "Password = INNER_SENTINEL\n",
        "Type = trojan" + endpoint + "Password = INNER_SENTINEL\nNetwork = ws\nSecurity = tls\nALPN = http/1.1\nWSPath = /literal%20path\nWSHost = inner.invalid\nServerName = inner.invalid\n",
        "Type = trojan" + endpoint + "Password = INNER_SENTINEL\nNetwork = grpc\nALPN = h2\nGRPCServiceName = proxy\nGRPCAuthority = inner.invalid\nGRPCMultiMode = false\n",
        "Type = http" + endpoint,
        "Type = https" + endpoint + "Username = USER_SENTINEL\nPassword = INNER_SENTINEL\n",
        "Type = naive" + endpoint + "Username = USER_SENTINEL\nPassword = INNER_SENTINEL\nServerName = inner.invalid\n",
        "Type = masque" + endpoint + "ServerName = inner.invalid\nMASQUEPath = /.well-known/masque/udp/{target_host}/{target_port}/\n",
        "Type = shadowsocks" + endpoint + "Method = aes-256-gcm\nPassword = INNER_SENTINEL\n" + wrapper,
        "Type = ssh" + endpoint + "Username = synthetic\n" + wrapper
    };
    for (const QString &access : inputs) {
        const QString input = head + access;
        ProfileEditorDialog editor(input, caps);
        require(editor.findChild<QTabWidget *>()->isTabEnabled(0));
        require(editor.configText() == input);
        editor.findChild<QLineEdit *>("profileName")->setText("Edited");
        const QString output = editor.configText();
        require(output != input && output.contains("Name = Edited"));
        require(ProfileStore::connectionInfo(output).connectable);
        ProfileEditorDialog reopened(output, caps);
        require(reopened.findChild<QTabWidget *>()->isTabEnabled(0));
        require(reopened.configText() == output);
        check(output, {"Name = Edited"}, core, directory + "/parameters-" + QString::number(index++) + ".conf");
        require(editor.findChild<QLineEdit *>("shadowPassword")->echoMode() == QLineEdit::Password);
        if (access.startsWith("Type = trojan") || access.startsWith("Type = naive"))
            check(output + wrapper, {"INNER_SENTINEL", "OUTER_SENTINEL"}, core, directory + "/wrapped-" + QString::number(index++) + ".conf");
    }
    const QStringList invalid{
        "Type = http" + endpoint + "Username = unpaired\n",
        "Type = https" + endpoint + "ServerName = forbidden.invalid\n",
        "Type = naive" + endpoint + "Username = user\n",
        "Type = trojan" + endpoint + "Password = INNER_SENTINEL\nNetwork = ws\nSecurity = reality\n",
        "Type = trojan" + endpoint + "Password = INNER_SENTINEL\nNetwork = grpc\nGRPCMultiMode = invalid\n",
        "Type = http" + endpoint + wrapper,
        "Type = masque" + endpoint + wrapper,
        "Type = shadowsocks" + endpoint + "Method = aes-192-gcm\nPassword = INNER_SENTINEL\n" + wrapper,
        "Type = shadowsocks" + endpoint + "Method = aes-256-gcm\nPassword = INNER_SENTINEL\nPlugin = v2ray-plugin\n" + wrapper
    };
    for (const auto &access : invalid) {
        ProfileEditorDialog editor(head + access, caps);
        require(!editor.findChild<QTabWidget *>()->isTabEnabled(0));
        require(editor.configText() == head + access);
        if (!core.isEmpty()) {
            const QString path = directory + "/invalid-parameters-" + QString::number(index++) + ".conf";
            require(ProfileStore::writeConfig(path, (head + access).toUtf8()));
            QProcess process;
            process.start(core, {"validate", path});
            require(process.waitForFinished(15000) && process.exitCode() != 0);
            const auto safe = safeCoreDiagnostic(process.readAllStandardError() + process.readAllStandardOutput());
            require(!safe.contains("SENTINEL"));
        }
    }
    const QString vless = head + "Type = vless" + endpoint
        + "ID = 123e4567-e89b-12d3-a456-426614174000\nEncryption = none\nNetwork = xhttp\nSecurity = tls\nXHTTPMode = packet-up\n";
    for (const QString &extra : {QString("ALPN = h3\nFingerprint = chrome\n"), QString("ALPN = h3\n") + wrapper,
            QString("XHTTPExtra = {\"headers\":{\"Authorization\":\"SENTINEL\"}}\n")}) {
        ProfileEditorDialog editor(vless + extra, caps);
        require(!editor.findChild<QTabWidget *>()->isTabEnabled(0));
        require(editor.configText() == vless + extra);
    }
    for (const QString &extra : {QString("ALPN = h3\n"), QString("ALPN = h2\n") + wrapper}) {
        ProfileEditorDialog editor(vless + extra, caps);
        require(editor.findChild<QTabWidget *>()->isTabEnabled(0));
        editor.findChild<QLineEdit *>("profileName")->setText("Edited");
        check(editor.configText(), extra.contains("[ShadowTLS]") ? QStringList{"ALPN = h2", "[ShadowTLS]", "Password = OUTER_SENTINEL"} : QStringList{"ALPN = h3"}, core, directory + "/vless-parameters-" + QString::number(index++) + ".conf");
    }
    const QString relative = head + "Type = masque" + endpoint + "CAFile = relative.pem\n";
    ProfileEditorDialog ca(relative, caps);
    require(ca.findChild<QTabWidget *>()->isTabEnabled(0));
    ca.findChild<QLineEdit *>("profileName")->setText("Edited");
    require(ca.configText().contains("CAFile = relative.pem"));
    ProfileEditorDialog conflict(head + inputs.first(), caps);
    conflict.findChild<QCheckBox *>("shadowTls")->setChecked(true);
    conflict.findChild<QLineEdit *>("shadowName")->setText("outer.invalid");
    conflict.findChild<QLineEdit *>("shadowPassword")->setText("OUTER_SENTINEL");
    require(conflict.configText().contains("[ShadowTLS]"));
    auto *network = conflict.findChild<QComboBox *>("trojanNetwork");
    network->setCurrentText("ws");
    conflict.findChild<QLineEdit *>("parameterWSPath")->setText("/retained");
    network->setCurrentText("grpc");
    bool blocked = false;
    QTimer::singleShot(0, [&] {
        if (auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            blocked = !message->text().contains("SENTINEL"); message->accept();
        }
    });
    conflict.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    require(blocked && conflict.result() != QDialog::Accepted);
    require(conflict.configText().contains("WSPath = /retained"));
}

static void nativeRegressions(const QString &directory) {
    // Review acceptance: normalize section whitespace without changing stored bytes;
    // quoted native protection requires consent when newly enabled, preserves imported
    // enabled intent, and declined consent never accepts a replacement for storage.
    const QString base = "[Profile]\nVersion = 1\nName = Native\n[IKEv2]\nWindowsProfile = Corporate\nServerName = vpn.example.invalid\n[Interface]\nMode = vpn\n";
    const QJsonObject caps{{"proxy", true}}; // Packet capabilities do not govern native editing.
    {
        const QString packet = "[Profile]\nName = Packet\n[AccessKey]\nSOCKS5 = socks5://localhost:1080\n[Interface]\nMode = proxy\n";
        ProfileEditorDialog editor(packet, caps, nullptr, [](const QString &, QString *) { return true; });
        auto *tabs = editor.findChild<QTabWidget *>();
        auto *raw = editor.findChild<QPlainTextEdit *>("rawConfig");
        for (const QString &text : {base, packet, base}) {
            tabs->setCurrentIndex(tabs->count() - 1);
            raw->setPlainText(text);
            tabs->setCurrentIndex(0);
            require(tabs->currentIndex() == 0 && tabs->isTabEnabled(0));
            require(bool(qobject_cast<QScrollArea *>(tabs->widget(0))) == (text == base));
            require(editor.findChild<QComboBox *>("nativeAuthMethod")->currentIndex() == 0);
            require(editor.configText() == text);
        }
    }
    for (bool protection : {false, true}) {
        QString input = base;
        if (protection) input.replace("[Interface]", "KillSwitch = true\n[Interface]");
        input.replace("\n", "\r\n");
        input.prepend(QChar(0xfeff));
        const auto info = ProfileStore::connectionInfo(input);
        require(info.backend == ProfileConnectionInfo::Backend::NativeIKEv2);
        require(info.protocol == "IKEv2" && !info.connectable && info.keyFile.isEmpty());
        ProfileEditorDialog editor(input, caps, nullptr, {}, "missing-packet-document-cli", directory + "/native.conf");
        auto *tabs = editor.findChild<QTabWidget *>();
        require(tabs->isTabEnabled(0));
        require(editor.configText() == input);
        require(editor.findChild<QCheckBox *>("nativeKillSwitch")->isChecked() == protection);
        auto *authMethod = editor.findChild<QComboBox *>("nativeAuthMethod");
        auto *authSource = editor.findChild<QComboBox *>("nativeAuthSource");
        auto *saveAuth = editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
        for (int method = 0; method < 3; ++method) {
            authMethod->setCurrentIndex(method);
            require(saveAuth->isEnabled() == (method == 0));
            require(editor.configText() == input);
        }
        authMethod->setCurrentIndex(0);
        authSource->setCurrentIndex(1);
        require(!saveAuth->isEnabled());
        require(editor.findChild<QLineEdit *>("nativeAuthUsername")->isEnabled());
        editor.findChild<QLineEdit *>("nativeAuthUsername")->setText("synthetic-user");
        editor.findChild<QCheckBox *>("nativeAuthAsk")->setChecked(false);
        require(editor.findChild<QLineEdit *>("nativeAuthPassword")->isEnabled());
        editor.findChild<QLineEdit *>("nativeAuthPassword")->setText("synthetic-secret");
        require(editor.configText() == input);
        editor.findChild<QCheckBox *>("nativeAuthAsk")->setChecked(true);
        require(editor.findChild<QLineEdit *>("nativeAuthPassword")->text().isEmpty());
        require(!editor.findChild<QLineEdit *>("nativeAuthPassword")->isEnabled());
        require(editor.findChild<QLineEdit *>("nativeAuthPassword")->echoMode() == QLineEdit::Password);
        authSource->setCurrentIndex(0);
        require(saveAuth->isEnabled() && editor.configText() == input);
        tabs->setCurrentIndex(tabs->count() - 1);
        tabs->setCurrentIndex(0);
        require(editor.configText() == input);
        editor.findChild<QLineEdit *>("windowsProfile")->setText("Edited");
        const QString output = editor.configText();
        require(output.contains("WindowsProfile = Edited"));
        require(!output.contains("AccessKey") && !output.contains("Routing") && !output.contains("ShadowTLS")
            && !output.contains("DNS =") && !output.contains("MTU ="));
        require(output.contains("KillSwitch = true") == protection);
    }
    for (const QString &input : {QString(base).replace("[Interface]\nMode = vpn\n", ""),
            QString(base).replace("[Interface]", "KillSwitch = false\n[Interface]")}) {
        ProfileEditorDialog editor(input, caps);
        require(editor.findChild<QTabWidget *>()->isTabEnabled(0));
        require(editor.configText() == input);
    }
    const QString mixed = base + "[AccessKey]\nSSH = ssh://alice@example.invalid\n";
    require(ProfileStore::connectionInfo(mixed).backend == ProfileConnectionInfo::Backend::Unknown);
    require(!ProfileStore::connectionInfo(mixed).connectable);
    require(ProfileStore::connectionInfo("[AccessKey]\nSSH = ssh://alice@example.invalid\n").backend
        == ProfileConnectionInfo::Backend::Packet);
    for (const QString &input : {mixed, QString(base).replace("WindowsProfile = Corporate\n", ""), base + "[Routing]\nDefault = proxy\n",
            base + "[Interface]\nDNS = 1.1.1.1\n", base + "[Future]\nSecret = SENTINEL\n",
            QString(base).replace("Mode = vpn", "Mode = proxy"),
            QString(base).replace("WindowsProfile = Corporate", "WindowsProfile = \"Corporate\""),
            QString(base).replace("ServerName = vpn.example.invalid", "ServerName = vpn.example.invalid\nServerName = other.invalid")}) {
        ProfileEditorDialog editor(input, caps);
        require(!editor.findChild<QTabWidget *>()->isTabEnabled(0));
        require(!editor.findChild<QPlainTextEdit *>("rawConfig")->isHidden());
        editor.findChild<QLineEdit *>("windowsProfile")->setText("Do not overwrite");
        require(editor.configText() == input);
    }
    ProfileEditorDialog enabling(base, caps);
    enabling.findChild<QCheckBox *>("nativeKillSwitch")->setChecked(true);
    bool confirmed = false;
    QTimer::singleShot(0, [&] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box);
        confirmed = box->text().contains("unprotected") && box->text().contains("offline");
        box->button(QMessageBox::No)->click();
    });
    enabling.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    require(confirmed && enabling.result() != QDialog::Accepted);
    QTimer::singleShot(0, [&] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box);
        box->button(QMessageBox::Yes)->click();
    });
    enabling.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    require(enabling.result() == QDialog::Accepted);
    ProfileEditorDialog importedProtection(QString(base).replace("[Interface]", "KillSwitch = true\n[Interface]"), caps);
    importedProtection.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    require(importedProtection.result() == QDialog::Accepted);
    const QString consentPath = directory + "/consent.conf";
    require(ProfileStore::writeConfig(consentPath, base.toUtf8()));
    const QString spaced = QString(base).replace("[Profile]", "[ Profile ]").replace("[IKEv2]", "[ IKEv2 ]").replace("[Interface]", "[ Interface ]");
    require(ProfileStore::connectionInfo(spaced).backend == ProfileConnectionInfo::Backend::NativeIKEv2);
    ProfileEditorDialog spacedEditor(spaced, caps);
    require(spacedEditor.findChild<QTabWidget *>()->isTabEnabled(0));
    require(spacedEditor.configText() == spaced);
    spacedEditor.findChild<QLineEdit *>("windowsProfile")->setText("Edited spaced native");
    require(spacedEditor.configText().contains("WindowsProfile = Edited spaced native"));
    require(ProfileStore::connectionInfo(spaced + "[ AccessKey ]\nSSH = ssh://alice@example.invalid\n").backend == ProfileConnectionInfo::Backend::Unknown);
    for (const QString &token : {QString("true"), QString("\"true\""), QString("\"\\x74rue\"")}) {
        const QString enabled = QString(spaced).replace("[ Interface ]", "KillSwitch = " + token + "\n[ Interface ]");
        ProfileEditorDialog rawEnabling(base, caps);
        rawEnabling.findChild<QPlainTextEdit *>("rawConfig")->setPlainText(enabled);
        bool asked = false;
        QTimer::singleShot(0, [&] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            require(box);
            asked = true;
            box->button(QMessageBox::No)->click();
        });
        rawEnabling.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(asked && rawEnabling.result() != QDialog::Accepted);
        require(rawEnabling.configText() == enabled);
        if (rawEnabling.result() == QDialog::Accepted)
            require(ProfileStore::writeConfig(consentPath, rawEnabling.configText().toUtf8()));
        QFile retained(consentPath);
        require(retained.open(QIODevice::ReadOnly) && retained.readAll() == base.toUtf8());
    }
    const QString quotedEnabled = QString(spaced).replace("[ Interface ]", "KillSwitch = \"true\"\n[ Interface ]");
    ProfileEditorDialog preserved(quotedEnabled, caps);
    require(preserved.findChild<QCheckBox *>("nativeKillSwitch")->isChecked());
    require(!preserved.findChild<QTabWidget *>()->isTabEnabled(0));
    require(preserved.configText() == quotedEnabled);
    preserved.findChild<QPlainTextEdit *>("rawConfig")->setPlainText(QString(base).replace("[Interface]", "KillSwitch = true\n[Interface]"));
    preserved.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    require(preserved.result() == QDialog::Accepted);
    ProfileEditorDialog rejected(base, caps, nullptr, [](const QString &, QString *error) {
        *error = "Rejected selected CLI validation";
        return false;
    });
    rejected.findChild<QTabWidget *>()->setCurrentIndex(rejected.findChild<QTabWidget *>()->count() - 1);
    rejected.findChild<QPlainTextEdit *>("rawConfig")->appendPlainText("# draft");
    const QString draft = rejected.configText();
    QTimer::singleShot(0, [] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box);
        box->accept();
    });
    rejected.findChild<QTabWidget *>()->setCurrentIndex(0);
    require(rejected.configText() == draft);
    bool validated = false;
    ProfileEditorDialog transition(base, caps, nullptr, [&](const QString &, QString *) { validated = true; return true; });
    const QString packet = "[Profile]\nName = Packet\n[AccessKey]\nSSH = ssh://alice@example.invalid\n[Interface]\nMode = proxy\n";
    transition.findChild<QTabWidget *>()->setCurrentIndex(transition.findChild<QTabWidget *>()->count() - 1);
    transition.findChild<QPlainTextEdit *>("rawConfig")->setPlainText(packet);
    transition.findChild<QTabWidget *>()->setCurrentIndex(0);
    require(validated && transition.findChild<QTabWidget *>()->isTabEnabled(0));
    require(transition.configText() == packet);
    transition.findChild<QLineEdit *>("profileName")->setText("Packet edit");
    require(transition.configText().contains("[AccessKey]") && !transition.configText().contains("[IKEv2]"));
    ProfileStore store(directory + "/native-imports");
    const QString source = directory + "/native-source.conf";
    QString error;
    const QByteArray bytes = QByteArray::fromHex("efbbbf") + spaced.toUtf8().replace("\n", "\r\n");
    require(ProfileStore::writeConfig(source, bytes, &error));
    const QString copied = store.importConfig(source, &error);
    require(!copied.isEmpty());
    QFile file(copied);
    require(file.open(QIODevice::ReadOnly) && file.readAll() == bytes);
    file.close();
    require(!ProfileStore::writeConfig(directory + "/missing/native.conf", bytes, &error));
    const QByteArray boundary = bytes + QByteArray(65536 - bytes.size(), '#');
    require(boundary.size() == 65536);
    require(ProfileStore::writeConfig(source, boundary, &error));
    const QString boundaryCopy = store.importConfig(source, &error);
    require(!boundaryCopy.isEmpty());
    QFile boundaryFile(boundaryCopy);
    require(boundaryFile.open(QIODevice::ReadOnly) && boundaryFile.readAll() == boundary);
    const QString blockedRoot = directory + "/blocked-native-imports";
    require(ProfileStore::writeConfig(blockedRoot, "occupied", &error));
    ProfileStore blockedStore(blockedRoot);
    require(blockedStore.importConfig(source, &error).isEmpty());
    const auto before = QDir(directory + "/native-imports").entryList(QDir::Files | QDir::Hidden);
    require(ProfileStore::writeConfig(source, bytes + QByteArray(65536, '#'), &error));
    require(store.importConfig(source, &error).isEmpty());
    require(QDir(directory + "/native-imports").entryList(QDir::Files | QDir::Hidden) == before);
    // The existing caller validates the selected copy and removes it on rejection.
    // Exercise that same contract with the exact opt-in offline Core CLI.
    const QString core = qEnvironmentVariable("BIMERON_IKEV2_CORE");
    if (!core.isEmpty()) {
        for (const QString &input : {base, spaced, quotedEnabled, mixed, spaced + "[AccessKey]\nSSH = ssh://alice@example.invalid\n"}) {
            require(ProfileStore::writeConfig(source, input.toUtf8(), &error));
            const QString path = store.importConfig(source, &error);
            require(!path.isEmpty());
            QProcess process;
            process.start(core, {"validate", path});
            require(process.waitForStarted(5000) && process.waitForFinished(20000));
            require(process.exitStatus() == QProcess::NormalExit);
            require((process.exitCode() == 0) == (!input.contains("[AccessKey]")));
            if (process.exitCode() != 0) require(QFile::remove(path));
        }
    }
}

static void routingHostRegressions(const QString &directory) {
    const QJsonObject caps{{"ip_split", true}, {"domain_split", true}, {"geoip_split", true},
        {"geosite_split", true}, {"geodata_schema", "v2ray-protobuf"}};
    const QString site = "[Rule]\nGeoSite=github\nAction=proxy\n";
    auto noDomains = caps;
    noDomains.remove("domain_split");
    require(!routingCompatibilityError(site, noDomains).isEmpty());
    require(routingCompatibilityError("[Interface]\nMode=\"proxy\"\n" + site, noDomains).isEmpty());
    for (const QString &settings : {"Mode=\"v\\x70n\"\n", "Mode=proxy\nMode=vpn\n",
            "KillSwitch=\"tr\\u0075e\"\n", "KillSwitch=\"true\n", "KillSwitch=yes\n",
            "KillSwitch=false\nKillSwitch=true\n", "Mode=unknown\n"})
        require(!routingCompatibilityError("[Interface]\n" + settings + site, caps).isEmpty());
    for (const QString &matcher : {"Domain", "GeoIP", "GeoSite"})
        require(!routingCompatibilityError("[Interface]\nKillSwitch=\"true\"\n[Rule]\n"
            + matcher + "=example\nAction=block\n", caps).isEmpty());
    const QString disabled = "# [Rule]\n# GeoIP=missing\n# Action=direct\n; [Rule]\n; GeoSite=missing\n";
    require(routingCompatibilityError("[Interface]\nKillSwitch=true\n" + disabled, {}).isEmpty());
    const QString mixed = "[Rule]\nCIDR=10.0.0.0/8\nAction=direct\n" + disabled
        + "[Rule]\nDomain=example.org\nAction=block\n[Rule]\nGeoIP=ru\nAction=direct\n" + site;
    require(routingCompatibilityError(mixed, caps).isEmpty());
    for (const QString &flag : {"geoip_split", "geosite_split", "geodata_schema"}) {
        auto missing = caps;
        missing.remove(flag);
        require(!routingCompatibilityError(mixed, missing).isEmpty());
        missing.insert(flag, "true"); // Strings never establish capability/schema support.
        require(!routingCompatibilityError(mixed, missing).isEmpty());
    }
    ProfileStore store(directory + "/relocation/managed");
    const QByteArray base = "[Profile]\nVersion=1\n[AccessKey]\nSOCKS5=socks5://localhost:1080\n";
    const QString absolute = QDir::fromNativeSeparators(directory + "/geoip.dat");
    for (const QString &reference : QStringList{"data/geoip.dat", "../geoip.dat", "\"data/geoip.dat\"",
            absolute + "\nGeoIPFile=data/geoip.dat",
            "data/geoip.dat\nGeoIPFile=" + absolute}) {
        const QByteArray bytes = base + ("[GeoData]\nGeoIPFile=" + reference + "\n").toUtf8();
        const QString source = directory + "/relocation/source.conf";
        require(ProfileStore::writeConfig(source, bytes));
        const auto before = QDir(directory + "/relocation/managed").entryList(QDir::Files);
        QString error;
        require(store.importConfig(source, &error).isEmpty());
        require(!error.isEmpty());
        require(QDir(directory + "/relocation/managed").entryList(QDir::Files) == before);
        QFile original(source);
        require(original.open(QIODevice::ReadOnly));
        require(original.readAll() == bytes);
    }
    require(ProfileStore::importReferencesSafe(("[GeoData]\nGeoIPFile=" + absolute
        + "\nGeoSiteFile=" + absolute + "\n").toUtf8()));
    require(ProfileStore::importReferencesSafe("# [GeoData]\n# GeoIPFile=data/geoip.dat\n"));
    // A same-directory copy preserves relative resolution and exact source bytes.
    const QString local = directory + "/relocation/managed/local.conf";
    const QByteArray bytes = base + "[GeoData]\nGeoSiteFile=data/geosite.dat\n" + disabled.toUtf8();
    require(ProfileStore::writeConfig(local, bytes));
    const QString imported = store.importConfig(local);
    require(!imported.isEmpty());
    QFile copy(imported);
    require(copy.open(QIODevice::ReadOnly));
    require(copy.readAll() == bytes);
}
// Optional native editor integration: set BIMERON_GEODATA_CORE to a Core
// executable implementing docs/DESKTOP_DOCUMENT_API.MD (protocol 1).
static void geodataBridgeRegressions(const QString &directory) {
    const QString core = qEnvironmentVariable("BIMERON_GEODATA_CORE");
    if (core.isEmpty()) return;
    require(QFileInfo(core).isExecutable());
    const QJsonObject caps{{"vpn", true}, {"proxy", true}, {"ip_split", true}, {"domain_split", true},
        {"geoip_split", true}, {"geosite_split", true}, {"geodata_schema", "v2ray-protobuf"}};
    const QString path = directory + "/bridge.conf";
    const auto call = [&](const QStringList &args, const QJsonObject &request = QJsonObject{}) {
        QProcess process;
        process.start(core, args);
        require(process.waitForStarted(5000));
        if (!request.isEmpty()) {
            const QByteArray input = QJsonDocument(request).toJson(QJsonDocument::Compact);
            require(process.write(input) == input.size());
        }
        process.closeWriteChannel();
        require(process.waitForFinished(20000));
        require(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0);
        QJsonParseError error;
        const auto response = QJsonDocument::fromJson(process.readAllStandardOutput(), &error).object();
        require(error.error == QJsonParseError::NoError && response.value("version").toInt() == 1);
        require(response.value("error").toString().isEmpty());
        return response;
    };
    const auto inspect = [&](const QString &text) {
        return call({"document"}, {{"version", 1}, {"profile", text}, {"operations", QJsonArray{}}});
    };
    // Same tiny wire messages as Core internal/routing/geodata_test.go helpers:
    // bytesField, geoGroup, geoCIDR and geoDomain. All lengths fit one byte.
    const auto field = [](char tag, const QByteArray &value) {
        require(value.size() < 128);
        return QByteArray(1, tag) + QByteArray(1, char(value.size())) + value;
    };
    const auto group = [&](const QByteArray &id, const QByteArray &entry) {
        return field(0x0a, field(0x0a, id) + field(0x12, entry));
    };
    const QByteArray ips = group("ru", field(0x0a, QByteArray::fromHex("cb007100")) + QByteArray::fromHex("1018"))
        + group("by", field(0x0a, QByteArray::fromHex("c6336400")) + QByteArray::fromHex("1018"))
        + group("cloudflare", field(0x0a, QByteArray::fromHex("c0000200")) + QByteArray::fromHex("1018"));
    const QByteArray sites = group("github", QByteArray::fromHex("0802") + field(0x12, "github.example"))
        + group("youtube", QByteArray::fromHex("0802") + field(0x12, "youtube.example"));
    for (const auto &kind : {QString("geoip"), QString("geosite")}) {
        QFile file(directory + '/' + kind + ".dat");
        require(file.open(QIODevice::WriteOnly));
        const auto bytes = kind == "geoip" ? ips : sites;
        require(file.write(bytes) == bytes.size()); file.close();
        const auto groups = call({"geodata", "groups", kind, file.fileName()}).value("groups").toArray();
        require(groups == (kind == "geoip" ? QJsonArray{"by", "cloudflare", "ru"} : QJsonArray{"github", "youtube"}));
    }
    const auto settle = [](ProfileEditorDialog &dialog) {
        QElapsedTimer elapsed; elapsed.start();
        while ((!dialog.findChildren<QProcess *>().isEmpty() || GeoDataSelection::instance()->catalogsBusy()) && elapsed.elapsed() < 20000) {
            QApplication::processEvents(QEventLoop::AllEvents, 50);
            QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        require(dialog.findChildren<QProcess *>().isEmpty());
    };
    const QString base = "[Profile]\nVersion=1\nName=Bridge\n[AccessKey]\nType=shadowsocks\n"
        "Server=example.invalid\nPort=443\nMethod=aes-256-gcm\nPassword=SYNTHETIC\n"
        "[GeoData]\nGeoIPFile=geoip.dat\nGeoSiteFile=geosite.dat\n[Routing]\nDefault=proxy\n";
    // Integration acceptance: imported IDs and commented sections survive a name-only save.
    for (const QString &rule : {QString("[Rule]\nGeoIP=RU\nAction=direct\n"), QString("# [IKEv2]\n[Rule]\nDomain=example.org\nAction=direct\n")}) {
        ProfileEditorDialog dialog(base + rule, caps, nullptr, {}, core, path);
        settle(dialog);
        dialog.findChild<QLineEdit *>("profileName")->setText("Renamed");
        bool warned = false;
        QTimer guard;
        QObject::connect(&guard, &QTimer::timeout, [&] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { warned = true; box->accept(); }
        });
        guard.start(10);
        dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        guard.stop();
        require(!warned && dialog.result() == QDialog::Accepted);
        const auto result = inspect(dialog.configText());
        require(result.value("fields").toObject().value("Profile/Name") == "Renamed");
        if (rule.contains("GeoIP")) require(result.value("rules").toArray().first().toObject().value("value") == "RU");
    }
    {
        GeoSitePicker picker(nullptr, {"youtube", "github"}, {"YOUTUBE", "youtube", "YouTube"}, {});
        auto *list = picker.findChild<QListWidget *>("geoSiteGroups");
        int checked = 0;
        for (int row = 0; row < list->count(); ++row) {
            auto *item = list->item(row);
            if (item->checkState() != Qt::Checked) continue;
            ++checked;
            require(item->data(Qt::UserRole) == "youtube");
            require(!item->data(Qt::UserRole + 2).toBool());
        }
        require(checked == 1);
        picker.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        require(picker.selectedIds() == QStringList{"YOUTUBE"});
    }
    {
        const QString source = QString(base).replace("Password=SYNTHETIC", "Password=SYNTHETIC\nPlugin=v2ray-plugin\nPluginOpts=tls;host=example.invalid");
        ProfileEditorDialog dialog(source, caps, nullptr, {}, core, path);
        settle(dialog); dialog.show();
        auto *name = dialog.findChild<QLineEdit *>("profileName");
        auto *options = dialog.findChild<QLineEdit *>("ssPluginOpts");
        require(name && options && !options->text().isEmpty());
        name->setText("Unsaved name"); options->clear();
        for (bool discard : {false, true}) {
            bool prompted = false;
            QTimer::singleShot(0, [&] {
                auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                require(box); prompted = true;
                for (auto *button : box->buttons())
                    if (box->buttonRole(button) == (discard ? QMessageBox::DestructiveRole : QMessageBox::RejectRole)) { button->click(); return; }
                require(false);
            });
            dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
            require(prompted && name->text() == "Unsaved name" && options->text().isEmpty());
            require(dialog.isVisible() != discard);
        }
        require(dialog.result() == QDialog::Rejected);
    }
    {
        QString quoted = base;
        quoted.replace("Password=SYNTHETIC", "Password=\"sec\\x72et\"");
        quoted.replace("Port=443", "Port=\"443\"");
        ProfileEditorDialog decoded(quoted, caps, nullptr, {}, core, path);
        require(decoded.configText() == quoted);
        QLineEdit *password = nullptr;
        for (auto *edit : decoded.findChildren<QLineEdit *>()) if (edit->text() == "secret") password = edit;
        require(password);
        password->setText("changed");
        const auto fields = inspect(decoded.configText()).value("fields").toObject();
        require(fields.value("AccessKey/Password") == "changed");
        require(fields.value("AccessKey/Port") == "443");
        settle(decoded);
    }
    {
        QWidget styledParent;
        QFile sourceFile(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../src/MainWindow.cpp"));
        require(sourceFile.open(QIODevice::ReadOnly));
        const QString sourceCode = QString::fromUtf8(sourceFile.readAll());
        const QString start = "QString applicationStyle = QStringLiteral(R\"(";
        const int from = sourceCode.indexOf(start) + start.size();
        const int to = sourceCode.indexOf(")\");", from);
        require(from >= start.size() && to > from);
        styledParent.setStyleSheet(sourceCode.mid(from, to - from));
        class SizeTranslator final : public QTranslator {
        public:
            QJsonObject strings;
            QString translate(const char *, const char *text, const char *, int) const override { return strings.value(QString::fromUtf8(text)).toString(); }
            bool isEmpty() const override { return strings.isEmpty(); }
        } sizeTranslator;
        QFile ru(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json"));
        require(ru.open(QIODevice::ReadOnly));
        sizeTranslator.strings = QJsonDocument::fromJson(ru.readAll()).object();
        QApplication::installTranslator(&sizeTranslator);
        ProfileEditorDialog styled("[Profile]\nVersion=1\nName=Size\n[AccessKey]\nSSConf=ssconf://example.invalid\n", caps, &styledParent, {}, core, path);
        styled.show();
        QElapsedTimer geometryTimer; geometryTimer.start();
        while (geometryTimer.elapsed() < 500) QApplication::processEvents(QEventLoop::AllEvents, 50);
        require(styled.findChild<QScrollArea *>("vlessScroll"));
        require(styled.size() == QSize(680, 520));
        auto *styledTabs = styled.findChild<QTabWidget *>();
        styledTabs->setCurrentIndex(styledTabs->count() - 1);
        require(styled.findChild<QPlainTextEdit *>("rawConfig")->isVisible());
        require(styled.findChild<QPlainTextEdit *>("rawConfig")->toPlainText().contains("SSConf=ssconf://example.invalid"));
        const QString sizePreview = qEnvironmentVariable("BIMERON_GEODATA_PREVIEW");
        if (!sizePreview.isEmpty()) require(styled.grab().save(sizePreview + "-styled.png"));
        settle(styled);
        QApplication::removeTranslator(&sizeTranslator);
    }
    const QString local = "[Rule]\nCIDR=10.0.0.0/8,172.16.0.0/12,192.168.0.0/16,169.254.0.0/16,fc00::/7,fe80::/10\nAction=direct\n";
    const QString first = "[Rule]\nGeoSite=github\nAction=block\n# attached site comment\n";
    const QString disabled = "# [Rule]\n# GeoIP=ru\n# Action=direct\n# # nested comment\n";
    const QString domain = "[Rule]\nDomain=example.org\nAction=proxy\n";
    const QString suffix = "[Future]\nOpaque = retained";
    const QString source = QString(QChar(0xfeff)) + QString(base + first + disabled + local + domain + suffix).replace("\n", "\r\n");
    const auto crlf = [](QString text) { return text.replace("\n", "\r\n"); };
    ProfileEditorDialog editor(source, caps, nullptr, {}, core, path);
    auto *tabs = editor.findChild<QTabWidget *>();
    auto *table = editor.findChild<QTableWidget *>();
    require(tabs && table && tabs->isTabEnabled(0) && table->rowCount() == 4);
    auto *statusScroll = editor.findChild<QScrollArea *>("routingStatusScroll");
    require(statusScroll && statusScroll->maximumHeight() == 64);
    auto *geometryStatus = editor.findChild<QLabel *>("routingStatus");
    require(geometryStatus);
    const QString previousStatus = geometryStatus->text();
    ProfileEditorDialog emptyStatus(base, caps, nullptr, {}, core, path);
    settle(emptyStatus);
    require(emptyStatus.findChild<QLabel *>("routingStatus")->text().isEmpty());
    require(emptyStatus.findChild<QScrollArea *>("routingStatusScroll")->isHidden());
    require(statusScroll->minimumHeight() == 0);
    geometryStatus->setText(QString("Long capability diagnostic\n").repeated(40));
    editor.resize(680, 520);
    editor.show(); QApplication::processEvents();
    require(editor.height() == 520);
    editor.hide(); geometryStatus->setText(previousStatus);

    require(editor.configText().toUtf8() == source.toUtf8());
    const QString preview = qEnvironmentVariable("BIMERON_GEODATA_PREVIEW");
    if (!preview.isEmpty()) {
        require(QDir::isAbsolutePath(preview));
        editor.resize(1100, 800);
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->widget(i)->isAncestorOf(table)) tabs->setCurrentIndex(i);
        editor.show();
        QApplication::processEvents();
        require(editor.grab().save(preview, "PNG"));
        editor.hide();
    }

    tabs->setCurrentIndex(tabs->count() - 1); tabs->setCurrentIndex(0);
    require(editor.configText().toUtf8() == source.toUtf8());
    const auto enabled = [&](int row) { return table->cellWidget(row, 0)->findChild<QCheckBox *>(); };
    for (int cycle = 0; cycle < 3; ++cycle) {
        enabled(1)->setChecked(true);
        const QString output = editor.configText();
        require(output == QString(source).replace(crlf(disabled), crlf("[Rule]\nGeoIP=ru\nAction=direct\n# nested comment\n")));
        require(!output.contains("Enabled="));
        enabled(1)->setChecked(false);
        require(editor.configText() == source);
    }
    // A later local block must stay after the earlier blocking GeoSite rule.
    auto *action = qobject_cast<QComboBox *>(table->cellWidget(0, 3));
    require(action); action->setCurrentIndex(action->findData("direct"));
    const QString edited = editor.configText();
    const QString editedExpected = QString(source)
        .replace("Default=proxy\r\n[Rule]", "Default=proxy\r\n\r\n[Rule]")
        .replace("GeoSite=github\r\nAction=block", "GeoSite=github\r\nAction=direct")
        .replace("# attached site comment\r\n# [Rule]", "# attached site comment\r\n\r\n# [Rule]");
    require(edited == editedExpected);
    require(inspect(edited).value("rules").toArray().at(2).toObject().value("matcher") == "CIDR");
    action->setCurrentIndex(action->findData("block"));
    require(editor.configText() == source);
    table->selectRow(1);
    QPushButton *down = nullptr;
    for (auto *button : editor.findChildren<QPushButton *>()) if (button->text() == "Move down") down = button;
    require(down && down->isEnabled()); down->click();
    const QString moved = QString(QChar(0xfeff)) + crlf(base + first + "\n" + local + "\n" + disabled + "\n" + domain + suffix);
    require(editor.configText() == moved && table->currentRow() == 2);
    const auto rows = inspect(moved).value("rules").toArray();
    require(rows.size() == 4 && !rows.at(2).toObject().value("enabled").toBool());
    require(rows.at(3).toObject().value("matcher") == "Domain");
    // Duplicated and disabled local blocks must remain visible, in place.
    for (const QString &tail : {local + local, disabled + local, first + local}) {
        const QString input = base + tail;
        ProfileEditorDialog imported(input, caps, nullptr, {}, core, path);
        auto *importedTable = imported.findChild<QTableWidget *>();
        require(importedTable && importedTable->rowCount() == 2);
        require(imported.configText() == input);
        auto *lastAction = qobject_cast<QComboBox *>(importedTable->cellWidget(1, 3));
        require(lastAction); lastAction->setCurrentIndex(lastAction->findData("block"));
        const QString output = imported.configText();
        const auto importedRows = inspect(output).value("rules").toArray();
        require(importedRows.size() == 2);
        require(importedRows.at(0).toObject() == inspect(input).value("rules").toArray().at(0).toObject());
        require(importedRows.at(1).toObject().value("action") == "block");
        require(output.startsWith(base));
        settle(imported);
    }
    // Catalog completion is asynchronous; drive the event loop with a bound.
    QElapsedTimer timer; timer.start();
    auto *status = editor.findChild<QLabel *>("routingStatus");
    require(status);
    while (status->text().contains("Validating dataset") && timer.elapsed() < 20000)
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    require(!status->text().contains("Validating dataset"));
    auto *target = qobject_cast<QLineEdit *>(table->cellWidget(0, 2));
    require(target && !target->actions().isEmpty());
    bool popupChecked = false;
    QTimer::singleShot(0, [&] {
        auto *popup = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        require(popup && popup != &editor);
        auto *search = popup->findChild<QLineEdit *>();
        auto *list = popup->findChild<QListWidget *>("geoSiteGroups");
        require(search && list);
        int entries = 0;
        for (int i = 0; i < list->count(); ++i) entries += list->item(i)->data(Qt::UserRole).isValid();
        require(entries == 2);
        search->setText("YouTube");
        QListWidgetItem *github = nullptr, *youtube = nullptr;
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            if (item->data(Qt::UserRole) == "github") github = item;
            if (item->data(Qt::UserRole) == "youtube") youtube = item;
        }
        require(github && youtube && github->isHidden() && github->checkState() == Qt::Checked);
        require(!youtube->isHidden()); list->setCurrentItem(youtube); list->setFocus();
        QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
        QApplication::sendEvent(list, &space);
        require(youtube->checkState() == Qt::Checked);
        search->setText("github");
        require(!github->isHidden() && youtube->isHidden() && youtube->checkState() == Qt::Checked);
        auto *buttons = popup->findChild<QDialogButtonBox *>(); require(buttons);
        buttons->button(QDialogButtonBox::Ok)->click(); popupChecked = true;
    });
    target->actions().first()->trigger();
    require(popupChecked && target->property("groupIds").toString() == "github,youtube" && editor.result() != QDialog::Accepted);
    require(target->text().contains("GitHub") && target->text().contains("YouTube"));
    require(inspect(editor.configText()).value("rules").toArray().at(0).toObject().value("value") == "github,youtube");
    target->setProperty("groupIds", "youtube,!unknown");
    QTimer::singleShot(0, [&] {
        auto *popup = qobject_cast<QDialog *>(QApplication::activeModalWidget()); require(popup);
        auto *list = popup->findChild<QListWidget *>("geoSiteGroups"); require(list);
        auto *sections = popup->findChild<QListWidget *>("geoSiteSections"); require(sections && sections->count() == 10);
        QListWidgetItem *youtube = nullptr, *github = nullptr, *unknown = nullptr;
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            if (item->data(Qt::UserRole) == "youtube") youtube = item;
            if (item->data(Qt::UserRole) == "github") github = item;
            if (item->data(Qt::UserRole) == "!unknown") unknown = item;
        }
        require(youtube && github && unknown && unknown->text().contains("Unavailable"));
        sections->setCurrentRow(2);
        require(!youtube->isHidden() && github->isHidden() && unknown->isHidden());
        auto *search = popup->findChild<QLineEdit *>(); search->setText("github");
        require(!github->isHidden() && youtube->isHidden() && youtube->checkState() == Qt::Checked);
        github->setCheckState(Qt::Checked);
        require(unknown->checkState() == Qt::Checked);
        search->clear();
        require(!youtube->isHidden() && github->isHidden());
        sections->setCurrentRow(0);
        require(!unknown->isHidden());
        popup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    target->actions().first()->trigger();
    require(target->property("groupIds").toString() == "youtube,!unknown,github");
    require(status->text().contains("!unknown"));
    const QString beforeCancel = target->property("groupIds").toString();
    QTimer::singleShot(0, [&] {
        auto *popup = qobject_cast<QDialog *>(QApplication::activeModalWidget()); require(popup);
        auto *list = popup->findChild<QListWidget *>("geoSiteGroups"); require(list);
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).isValid()) list->item(i)->setCheckState(Qt::Unchecked);
        popup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
    });
    target->actions().first()->trigger();
    require(target->property("groupIds").toString() == beforeCancel);

    auto *geoType = qobject_cast<QComboBox *>(table->cellWidget(0, 1));
    geoType->setCurrentIndex(geoType->findData("GeoIP"));
    target->setProperty("groupIds", "ru,cloudflare,private");
    QTimer::singleShot(0, [&] {
        auto *popup = qobject_cast<QDialog *>(QApplication::activeModalWidget()); require(popup);
        auto *list = popup->findChild<QListWidget *>(); require(list && list->count() >= 3);
        require(list->item(0)->data(Qt::UserRole) == "cloudflare");
        require(list->item(1)->data(Qt::UserRole) == "private");
        require(list->item(2)->data(Qt::UserRole) == "ru");
        require(list->item(2)->text().startsWith("Russia (RU)"));
        auto *search = popup->findChild<QLineEdit *>(); search->setText(QString::fromUtf8("Россия"));
        require(!list->item(2)->isHidden());
        search->setText("Russia");
        require(!list->item(2)->isHidden());
        search->setText("RU");
        require(!list->item(2)->isHidden());
        popup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    target->actions().first()->trigger();
    require(target->property("groupIds").toString() == "ru,cloudflare,private");
    require(target->text().contains("Russia (RU)"));
    target->setProperty("groupIds", "ru");
    QTimer::singleShot(0, [&] {
        auto *popup = qobject_cast<QDialog *>(QApplication::activeModalWidget()); require(popup);
        auto *list = popup->findChild<QListWidget *>(); require(list && list->count() == 3);
        auto *ru = list->item(0), *cloudflare = list->item(1), *by = list->item(2);
        require(ru->data(Qt::UserRole) == "ru" && ru->checkState() == Qt::Checked);
        require(cloudflare->data(Qt::UserRole) == "cloudflare" && cloudflare->checkState() == Qt::Unchecked);
        require(by->data(Qt::UserRole) == "by" && by->checkState() == Qt::Unchecked);
        auto *search = popup->findChild<QLineEdit *>(); require(search);
        search->setText("cloudflare");
        require(ru->isHidden() && !cloudflare->isHidden() && by->isHidden());
        cloudflare->setCheckState(Qt::Checked);
        search->clear();
        require(!ru->isHidden() && !cloudflare->isHidden() && !by->isHidden());
        require(list->item(0) == ru && list->item(1) == cloudflare && list->item(2) == by);
        ru->setCheckState(Qt::Unchecked);
        require(list->item(0) == ru && list->item(1) == cloudflare && list->item(2) == by);
        ru->setCheckState(Qt::Checked);
        popup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    target->actions().first()->trigger();
    require(target->property("groupIds").toString() == "ru,cloudflare");
    require(inspect(editor.configText()).value("rules").toArray().at(0).toObject().value("value") == "ru,cloudflare");
    for (const QString &flag : {QString("geoip_split"), QString("geosite_split"), QString("geodata_schema")}) {
        auto restricted = caps; restricted.remove(flag);
        const QString rule = flag == "geoip_split" ? "[Rule]\nGeoIP=ru\nAction=direct\n" : first;
        const QString input = base + rule;
        ProfileEditorDialog gated(input, restricted, nullptr, {}, core, path);
        require(gated.configText() == input);
        auto *reason = gated.findChild<QLabel *>("routingStatus");
        require(reason && reason->text().contains("does not explicitly support"));
        bool rejected = false;
        QTimer::singleShot(0, [&] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            require(box); rejected = true; box->accept();
        });
        gated.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(rejected && gated.result() != QDialog::Accepted && gated.configText() == input);
        settle(gated);
    }
    // Closing while asynchronous catalogs are still queued must not invoke
    // callbacks against a partially destroyed dialog.
    for (int attempt = 0; attempt < 3; ++attempt) {
        auto *closing = new ProfileEditorDialog(source, caps, nullptr, {}, core, path);
        delete closing;
        QApplication::processEvents();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

static void geoSiteClassificationTest() {
    require(geoSiteEntries().size() == 1561 && geoSiteTopics().size() == 46);
    require(geoSiteSections().size() == 8);
    require(geoSiteSections().at(5) == "Ecosystems" && geoSiteSections().at(6) == "Artificial intelligence"
        && geoSiteSections().last() == "General lists");
    const QSet<QString> ecosystems{
        "adguard", "adobe", "alibaba", "alphabet", "amazon", "apple",
        "autodesk", "auxgroup", "baidu", "broadcom", "bytedance", "canonical",
        "cheetahmobile", "cisco", "dell", "didi", "disney", "dmm",
        "douyin", "epicgames", "facebook", "gmo-internet", "google", "haier",
        "hkt", "intuit", "jd", "kakao", "kingsoft", "kuaishou",
        "lenovo", "line", "mailru", "mailru-group", "meitu", "meta",
        "microsoft", "mozilla", "mts-ru", "naver", "nbcuniversal", "netease",
        "newscorp", "oracle", "pccw", "progress", "protonmail", "rakuten",
        "razer", "rostelecom", "ruanmei", "samsung", "sber", "sony",
        "tencent", "xiaomi", "yandex"};
    QByteArray baseline;
    int ecosystemCount = 0, aiCount = 0;
    for (auto it = geoSiteEntries().cbegin(); it != geoSiteEntries().cend(); ++it) {
        int section = it->section;
        QString topic = QString::fromUtf8(it->topic);
        if (ecosystems.contains(it.key())) {
            require(section == 5 && topic.isEmpty() && it->collection);
            section = 5; topic = "6-organizations"; ++ecosystemCount;
        } else if (section == 6) {
            require(topic == "4-ai" || ((it.key() == "category-ai-!cn" || it.key() == "category-ai-chat-!cn") && topic.isEmpty()));
            section = 3; ++aiCount;
            if (it.key() == "cursor") topic = "4-development";
            if (it.key() == "xiaomi-ai") { section = 5; topic = "6-organizations"; }
        } else if (section == 7) section = 5;
        else require(section < 5);
        const QString baselineName = it.key() == "openai" ? "OpenAI and ChatGPT (Codex)"
            : it.key() == "anthropic" ? "Anthropic AI tools (Claude)" : QString::fromUtf8(it->name);
        baseline += (it.key() + '|' + baselineName + '|' + QString::number(section)
            + '|' + QString::number(it->collection ? 1 : 0) + '|' + topic + '\n').toUtf8();
    }
    require(ecosystemCount == 57 && aiCount == 28);
    require(QCryptographicHash::hash(baseline, QCryptographicHash::Sha256).toHex() == "829ab62fa3ebfceb0214fac754d0176d8b662af80e30f86b3b9e93dd2838974d");
    require(geoSiteEntries().value("youtube").section == 1 && QString(geoSiteEntries().value("youtube").topic) == "2-video");
    require(geoSiteEntries().value("github").section == 3 && QString(geoSiteEntries().value("github").topic) == "4-development");
    const auto xiaomiAi = geoSiteEntries().value("xiaomi-ai");
    require(xiaomiAi.section == 6 && xiaomiAi.collection && QString(xiaomiAi.topic) == "4-ai");
    require(QString::fromUtf8(xiaomiAi.name) == QString::fromUtf8("Xiaomi AI — MiMo tools and XiaoAI device assistant"));
    require(geoSiteSearchText("xiaomi-ai").contains(QString::fromUtf8("помощник XiaoAI")));
    // Known mixed purposes receive explicit editorial navigation, not unknown status.
    require(geoSiteEntries().value("8btc").section == 7 && geoSiteEntries().value("8btc").collection);
    require(geoSiteEntries().value("cnet").section == 1 && geoSiteEntries().value("cnet").collection);
    require(geoSiteEntries().value("falungong").section == 7 && geoSiteEntries().value("falungong").collection);
    require(geoSiteEntries().value("filimo").section == 1 && geoSiteEntries().value("filimo").collection);
    require(geoSiteEntries().value("hainanairlines").section == 7 && geoSiteEntries().value("hainanairlines").collection);
    require(geoSiteEntries().value("hinet").section == 2 && geoSiteEntries().value("hinet").collection);
    require(geoSiteEntries().value("hisense").section == 7 && geoSiteEntries().value("hisense").collection);
    require(geoSiteEntries().value("hkbn").section == 7 && geoSiteEntries().value("hkbn").collection);
    require(geoSiteEntries().value("hketgroup").section == 7 && geoSiteEntries().value("hketgroup").collection);
    require(geoSiteEntries().value("hupu").section == 1 && geoSiteEntries().value("hupu").collection);
    require(geoSiteEntries().value("icable").section == 7 && geoSiteEntries().value("icable").collection);
    require(geoSiteEntries().value("idg").section == 7 && geoSiteEntries().value("idg").collection);
    require(geoSiteEntries().value("ifanr").section == 7 && geoSiteEntries().value("ifanr").collection);
    require(geoSiteEntries().value("kkbox").section == 1 && geoSiteEntries().value("kkbox").collection);
    require(geoSiteEntries().value("kuaishou").section == 5 && geoSiteEntries().value("kuaishou").collection);
    require(geoSiteEntries().value("ltn").section == 7 && geoSiteEntries().value("ltn").collection);
    require(geoSiteEntries().value("madshi").section == 3 && geoSiteEntries().value("madshi").collection);
    require(geoSiteEntries().value("mega").section == 2 && geoSiteEntries().value("mega").collection);
    require(geoSiteEntries().value("nvidia").section == 7 && geoSiteEntries().value("nvidia").collection);
    require(geoSiteEntries().value("okaapps").section == 3 && geoSiteEntries().value("okaapps").collection);
    require(geoSiteEntries().value("oschina").section == 3 && geoSiteEntries().value("oschina").collection);
    require(geoSiteEntries().value("paypal").section == 4 && geoSiteEntries().value("paypal").collection);
    require(geoSiteEntries().value("phoenix").section == 7 && geoSiteEntries().value("phoenix").collection);
    require(geoSiteEntries().value("pornhub").section == 1 && geoSiteEntries().value("pornhub").collection);
    require(geoSiteEntries().value("protonmail").section == 5 && geoSiteEntries().value("protonmail").collection);
    require(geoSiteEntries().value("quora").section == 7 && geoSiteEntries().value("quora").collection);
    require(geoSiteEntries().value("renren").section == 7 && geoSiteEntries().value("renren").collection);
    require(geoSiteEntries().value("secom").section == 7 && geoSiteEntries().value("secom").collection);
    require(geoSiteEntries().value("sharethis").section == 7 && geoSiteEntries().value("sharethis").collection);
    require(geoSiteEntries().value("sina").section == 7 && geoSiteEntries().value("sina").collection);
    require(geoSiteEntries().value("sohu").section == 7 && geoSiteEntries().value("sohu").collection);
    require(geoSiteEntries().value("tbank-ru").section == 4 && geoSiteEntries().value("tbank-ru").collection);
    require(geoSiteEntries().value("uc").section == 2 && geoSiteEntries().value("uc").collection);
    require(geoSiteEntries().value("verizon").section == 7 && geoSiteEntries().value("verizon").collection);
    require(geoSiteEntries().value("vivo").section == 4 && geoSiteEntries().value("vivo").collection);
    require(geoSiteEntries().value("wiwide").section == 7 && geoSiteEntries().value("wiwide").collection);
    require(geoSiteEntries().value("x").section == 7 && geoSiteEntries().value("x").collection);
    require(geoSiteEntries().value("xiaoheihe").section == 1 && geoSiteEntries().value("xiaoheihe").collection);
    require(geoSiteEntries().value("xiaohongshu").section == 7 && geoSiteEntries().value("xiaohongshu").collection);
    require(geoSiteEntries().value("zhangtao").section == 7 && geoSiteEntries().value("zhangtao").collection);
    for (const auto &decision : {qMakePair(QString("neuralink"), QString("6-organizations")),
                                 qMakePair(QString("redtube"), QString("2-adult"))}) {
        const auto entry = geoSiteEntries().value(decision.first);
        require(entry.collection && QString(entry.topic) == decision.second);
        require(entry.section == (decision.first == "neuralink" ? 7 : 1));
    }
    require(QString(geoSiteEntries().value("neuralink").name) == "Neuralink");
    require(QString(geoSiteEntries().value("redtube").name) == "RedTube adult content");
    require(geoSiteSearchText("redtube").contains(QString::fromUtf8("Контент для взрослых RedTube")));
    QMap<QString, int> topics;
    for (const auto &topic : geoSiteTopics()) {
        require(!topics.contains(topic.id) && topic.section >= 0 && topic.section < 8);
        topics.insert(topic.id, topic.section);
    }
    int assigned = 0;
    for (auto it = geoSiteEntries().cbegin(); it != geoSiteEntries().cend(); ++it) {
        require(!it.key().isEmpty() && it.key() == it.key().toLower() && it->section >= 0 && it->section < 8);
        require(it->name && *it->name && geoSiteSearchText(it.key()).contains(it->name));
        if (it->topic) { require(topics.contains(it->topic) && topics.value(it->topic) == it->section); ++assigned; }
    }
    require(assigned == 1156);
    for (const auto &pair : {qMakePair(QString("category-ai-chat-!cn"), QString("category-ai-!cn")),
                             qMakePair(QString("archive"), QString("category-web-archive")),
                             qMakePair(QString("google-gemini"), QString("google-deepmind"))}) {
        const auto a = geoSiteEntries().value(pair.first), b = geoSiteEntries().value(pair.second);
        require(a.section == b.section && a.collection == b.collection && QString::fromUtf8(a.topic) == QString::fromUtf8(b.topic));
    }
    const QMap<QString, QStringList> splitTopics{
        {"3-operating-systems", {"android", "archlinux", "debian", "fedora", "grapheneos", "kernel", "openwrt", "ubuntu", "ubuntukylin"}},
        {"3-updates-drivers", {"apple-update", "win-update"}},
        {"3-app-packages", {"apkcombo", "apkmirror", "apkpure", "aptoide", "decryptipastore", "google-play", "paskoocheh", "setapp", "f-droid", "flatpak", "homebrew", "snapcraft", "category-android-app-download"}},
        {"3-browsers", {"ceno", "epicbrowser", "firefox", "librewolf", "category-browser-!cn"}}
    };
    require(!topics.contains("3-browser-os"));
    QStringList splitIds;
    for (auto it = splitTopics.cbegin(); it != splitTopics.cend(); ++it) {
        splitIds.append(it.value());
        QStringList actual;
        for (auto entry = geoSiteEntries().cbegin(); entry != geoSiteEntries().cend(); ++entry)
            if (QString::fromUtf8(entry->topic) == it.key()) actual.append(entry.key());
        auto expected = it.value(); expected.sort();
        require(actual == expected && topics.value(it.key()) == 2);
    }
    require(QString::fromUtf8(geoSiteEntries().value("win-spy").topic) == "1-telemetry");
    for (const QString &id : {QString("apple-pki"), QString("microsoft-pki")})
        require(QString::fromUtf8(geoSiteEntries().value(id).topic) == "3-security");
    for (const QString &id : {QString("deepin")})
        require(QString::fromUtf8(geoSiteEntries().value(id).topic) == "6-organizations");
    auto checkSplitNavigation = [&] {
        GeoSitePicker split(nullptr, splitIds, {"win-update", "android", "missing-id"}, {});
        auto *rows = split.findChild<QListWidget *>("geoSiteGroups");
        auto *sections = split.findChild<QListWidget *>("geoSiteSections");
        auto *combo = split.findChild<QComboBox *>("geoSiteTopics");
        auto *query = split.findChild<QLineEdit *>();
        auto *selected = split.findChild<QPushButton *>("geoSiteSelectionCount");
        auto visible = [&] {
            QStringList ids;
            for (int i = 0; i < rows->count(); ++i) {
                auto *row = rows->item(i);
                if (row->data(Qt::UserRole).isValid() && !row->isHidden()) ids.append(row->data(Qt::UserRole).toString());
            }
            ids.sort(); return ids;
        };
        sections->setCurrentRow(3);
        for (auto it = splitTopics.cbegin(); it != splitTopics.cend(); ++it) {
            combo->setCurrentIndex(combo->findData(it.key()));
            auto expected = it.value(); expected.sort(); require(visible() == expected);
            selected->click(); require(selected->isChecked());
            query->setText(QString::fromUtf8("обновлений Windows")); require(visible() == QStringList{"win-update"});
            query->setText("Windows update"); require(visible() == QStringList{"win-update"});
            query->clear(); require(visible() == QStringList({"android", "missing-id", "win-update"}));
            selected->click(); require(visible() == expected);
            for (int i = 0; i < rows->count(); ++i)
                if (rows->item(i)->data(Qt::UserRole) == it.value().first()) rows->item(i)->setCheckState(Qt::Checked);
            require(split.selectedIds().mid(0, 3) == QStringList({"win-update", "android", "missing-id"}));
            // Restore initial selection for the next topic's selected-only assertion.
            for (int i = 0; i < rows->count(); ++i) {
                auto *row = rows->item(i);
                if (row->data(Qt::UserRole).isValid())
                    row->setCheckState(QStringList{"win-update", "android", "missing-id"}.contains(row->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
            }
        }
        sections->setCurrentRow(0); require(combo->count() == 5 && combo->currentIndex() == 0);
        require(split.selectedIds() == QStringList({"win-update", "android", "missing-id"}));
    };
    auto checkMixedNavigation = [&] {
        const QStringList ids{"cnet", "madshi", "paypal", "nvidia", "mega", "xiaomi-ai"};
        GeoSitePicker popup(nullptr, ids, {"paypal", "missing-mixed"}, {});
        auto *rows = popup.findChild<QListWidget *>("geoSiteGroups");
        auto *sections = popup.findChild<QListWidget *>("geoSiteSections");
        auto *query = popup.findChild<QLineEdit *>();
        for (const QString &id : ids) {
            sections->setCurrentRow(geoSiteEntries().value(id).section + 1);
            for (const QString &text : {id, QString::fromUtf8(geoSiteEntries().value(id).name), geoSiteName(id)}) {
                query->setText(text);
                bool matched = false;
                for (int i = 0; i < rows->count(); ++i)
                    if (rows->item(i)->data(Qt::UserRole) == id) matched = !rows->item(i)->isHidden();
                require(matched);
            }
            query->clear();
            require(popup.selectedIds() == QStringList({"paypal", "missing-mixed"}));
        }
    };
    auto checkTopicSearch = [&] {
        const QStringList ids = geoSiteEntries().keys();
        GeoSitePicker popup(nullptr, ids, {"openai", "missing-ai"}, {});
        auto *rows = popup.findChild<QListWidget *>("geoSiteGroups");
        auto *query = popup.findChild<QLineEdit *>();
        auto *sections = popup.findChild<QListWidget *>("geoSiteSections");
        auto *combo = popup.findChild<QComboBox *>("geoSiteTopics");
        auto *selected = popup.findChild<QPushButton *>("geoSiteSelectionCount");
        auto visible = [&](const QString &id) {
            for (int i = 0; i < rows->count(); ++i)
                if (rows->item(i)->data(Qt::UserRole) == id) return !rows->item(i)->isHidden();
            return false;
        };
        sections->setCurrentRow(3);
        combo->setCurrentIndex(combo->findData("3-security"));
        QFile file(":/translations/ru.json"); require(file.open(QIODevice::ReadOnly));
        const auto russianTopics = QJsonDocument::fromJson(file.readAll()).object();
        for (const auto &topic : geoSiteTopics()) {
            for (const QString &label : {QString::fromUtf8(topic.name), russianTopics.value(topic.name).toString()}) {
                if (label.isEmpty()) continue;
                query->setText(label);
                for (auto it = geoSiteEntries().cbegin(); it != geoSiteEntries().cend(); ++it)
                    if (QString::fromUtf8(it->topic) == topic.id) require(visible(it.key()));
            }
        }
        for (const QString &label : {QString("AI"), QString::fromUtf8("ИИ")}) {
            query->setText(label);
            require(visible("openai"));
            for (auto it = geoSiteEntries().cbegin(); it != geoSiteEntries().cend(); ++it)
                if (QString::fromUtf8(it->topic) == "4-ai") require(visible(it.key()));
        }
        for (const QString &label : {QString("Ecosystems"), QString::fromUtf8("Экосистемы")}) {
            query->setText(label); require(visible("google") && visible("apple") && !visible("youtube") && !visible("neuralink"));
        }
        for (const QString &label : {QString("Artificial intelligence"), QString::fromUtf8("Искусственный интеллект")}) {
            query->setText(label); require(visible("openai") && visible("xiaomi-ai") && !visible("google"));
        }
        query->setText("Codex"); require(visible("openai") && !visible("anthropic"));
        query->setText("Claude"); require(visible("anthropic"));
        query->setText(russianTopics.value("Security").toString()); require(visible("apple-pki"));
        query->clear(); require(combo->currentData() == "3-security" && visible("apple-pki") && !visible("openai"));
        selected->click(); query->setText("Codex"); require(visible("openai") && !visible("missing-ai"));
        query->clear(); require(visible("missing-ai") && visible("openai") && !visible("anthropic"));
        require(popup.selectedIds() == QStringList({"openai", "missing-ai"}));
    };
    auto checkInteraction = [&] {
        QStringList ids = geoSiteEntries().keys(); ids.append("future-unknown");
        GeoSitePicker popup(nullptr, ids, {"openai", "anthropic", "youtube", "missing-ai", "openai"}, {});
        auto *rows = popup.findChild<QListWidget *>("geoSiteGroups");
        auto *query = popup.findChild<QLineEdit *>();
        auto *sections = popup.findChild<QListWidget *>("geoSiteSections");
        auto *combo = popup.findChild<QComboBox *>("geoSiteTopics");
        auto *selected = popup.findChild<QPushButton *>("geoSiteSelectionCount");
        auto visible = [&] {
            QStringList result;
            for (int i = 0; i < rows->count(); ++i)
                if (!rows->item(i)->isHidden() && rows->item(i)->data(Qt::UserRole).isValid())
                    result.append(rows->item(i)->data(Qt::UserRole).toString());
            return result;
        };
        auto checkOrdering = [&](bool grouped) {
            int lastSection = -1; QString previous, previousId; bool unavailable = false;
            int headings = 0;
            for (int i = 0; i < rows->count(); ++i) {
                auto *item = rows->item(i); if (item->isHidden()) continue;
                if (!item->data(Qt::UserRole).isValid()) {
                    require(item->flags() == Qt::NoItemFlags); ++headings; previous.clear(); continue;
                }
                const QString id = item->data(Qt::UserRole).toString();
                const bool missing = item->data(Qt::UserRole + 2).toBool();
                require(!unavailable || missing); unavailable = missing;
                const int section = missing ? 9 : item->data(Qt::UserRole + 1).toInt() < 0 ? 8 : item->data(Qt::UserRole + 1).toInt();
                if (grouped) { require(section >= lastSection); lastSection = section; }
                const QString name = geoSiteName(id).isEmpty() ? id : geoSiteName(id);
                if (!previous.isEmpty()) {
                    const int cmp = QString::localeAwareCompare(previous, name);
                    require(cmp < 0 || (cmp == 0 && previousId < id));
                }
                previous = name; previousId = id;
            }
            if (grouped && !visible().isEmpty()) require(headings > 0);
        };
        sections->setCurrentRow(7); combo->setCurrentIndex(combo->findData("4-ai"));
        require(visible().mid(0, 3) == QStringList({"openai", "anthropic", "cursor"}));
        for (int i = 0; i < rows->count(); ++i)
            if (rows->item(i)->data(Qt::UserRole) == "openai") rows->setCurrentItem(rows->item(i));
        auto *current = rows->currentItem();
        query->setText("a"); require(!sections->isEnabled() && !combo->isEnabled() && combo->count() == 1);
        checkOrdering(true); require(rows->currentItem() == current);
        selected->click(); checkOrdering(true);
        selected->click(); require(query->text() == "a" && visible().size() > 4); checkOrdering(true);
        selected->click();
        require(selected->isChecked() && query->text() == "a" && selected->text().contains('4'));
        for (const auto &id : visible()) require(popup.selectedIds().contains(id));
        query->clear(); require(selected->isChecked() && !sections->isEnabled()); checkOrdering(false);
        require(query->placeholderText() == QObject::tr("Search selected names or IDs"));
        selected->click(); require(sections->isEnabled() && combo->currentData() == "4-ai");
        require(visible().mid(0, 3) == QStringList({"openai", "anthropic", "cursor"}));
        sections->setCurrentRow(0); combo->setCurrentIndex(combo->findData("4-ai"));
        require(visible().mid(0, 3) == QStringList({"openai", "anthropic", "cursor"}));
        query->setText("future-unknown"); require(visible() == QStringList{"future-unknown"}); checkOrdering(true);
        query->setText("missing-ai"); require(visible() == QStringList{"missing-ai"}); checkOrdering(true);
        query->setText("fixture-no-match"); require(visible().isEmpty());
        for (int i = 0; i < rows->count(); ++i) require(rows->item(i)->isHidden());
        query->setText("Codex"); selected->click(); require(visible() == QStringList{"openai"});
        for (int i = 0; i < rows->count(); ++i)
            if (rows->item(i)->data(Qt::UserRole) == "openai") rows->item(i)->setCheckState(Qt::Unchecked);
        require(visible().isEmpty() && selected->isChecked() && selected->text().contains('3'));
        query->clear();
        for (int i = 0; i < rows->count(); ++i)
            if (rows->item(i)->data(Qt::UserRole).isValid()) rows->item(i)->setCheckState(Qt::Unchecked);
        require(visible().isEmpty() && selected->isChecked() && selected->isEnabled());
        auto emptyText = [&] {
            for (auto *label : popup.findChildren<QLabel *>())
                if (!label->isHidden() && label->text() != QObject::tr("Topic")) return label->text();
            return QString{};
        };
        require(emptyText() == QObject::tr("No selected groups"));
        query->setText("none"); require(emptyText() == QObject::tr("No matching selected groups"));
        auto *clear = query->findChild<QToolButton *>();
        require(clear && clear->toolTip() == QObject::tr("Clear search") && clear->focusPolicy() == Qt::StrongFocus);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QApplication::sendEvent(clear, &press); QApplication::sendEvent(clear, &release);
        require(query->text().isEmpty() && selected->isChecked());
        selected->click(); query->setText("   "); require(sections->isEnabled()); query->clear();
        sections->setCurrentRow(6); require(combo->count() == 1 && !combo->isEnabled());
        sections->setCurrentRow(9); require(combo->count() == 1 && !combo->isEnabled());
        sections->setCurrentRow(0); checkOrdering(false);
        query->setText("OpenAI and ChatGPT (Codex)"); require(visible().contains("openai"));
        query->setText(QString::fromUtf8("Инструменты ИИ Anthropic (Claude)")); require(visible().contains("anthropic"));
        popup.reject(); require(popup.result() == QDialog::Rejected);
        GeoSitePicker subset(nullptr, {"anthropic", "deepseek", "google-gemini", "google-deepmind"}, {}, {});
        subset.findChild<QListWidget *>("geoSiteSections")->setCurrentRow(7);
        auto *subsetRows = subset.findChild<QListWidget *>("geoSiteGroups");
        QString firstVisible;
        for (int i = 0; i < subsetRows->count(); ++i)
            if (!subsetRows->item(i)->isHidden()) { firstVisible = subsetRows->item(i)->data(Qt::UserRole).toString(); break; }
        require(firstVisible == "anthropic");
        require(subset.selectedIds().isEmpty());
    };
    checkInteraction();
    checkMixedNavigation();
    checkTopicSearch();
    checkSplitNavigation();
    require(geoSiteEntries().contains("neuralink") && geoSiteEntries().contains("redtube"));
    require(geoSiteEntries().contains("nic-ru") && geoSiteEntries().contains("nvidia"));
    require(geoSiteEntries().contains("anandtech") && !geoSiteEntries().value("anandtech").topic);
    for (const QString &id : {QString("category-social-media-ir"), QString("itunes")})
        require(geoSiteEntries().value(id).section == 1);
    require(geoSiteEntries().value("entrust").section == 2);
    for (const QString &id : {QString("category-urlshortner"), QString("category-pt"), QString("category-tm"), QString("category-cas")})
        require(geoSiteEntries().contains(id) && geoSiteSearchText(id).contains(id));
    const QMap<QString, QStringList> addedTopics{
        {"4-business-crm", {"beisen", "bitrix", "carrotquest", "clearbit", "envybox", "intercom", "mindbox", "salesforce", "zendesk", "zoho"}},
        {"5-weather", {"accuweather", "gismeteo", "moji", "openweather", "qweather", "weathercn", "windy"}}
    };
    auto checkAddedNavigation = [&] {
        QStringList ids;
        for (const auto &members : addedTopics) ids.append(members);
        GeoSitePicker picker(nullptr, ids, {"salesforce", "windy", "missing-id"}, {});
        auto *sections = picker.findChild<QListWidget *>("geoSiteSections");
        auto *combo = picker.findChild<QComboBox *>("geoSiteTopics");
        auto *rows = picker.findChild<QListWidget *>("geoSiteGroups");
        auto *query = picker.findChild<QLineEdit *>();
        auto visible = [&] {
            QStringList result;
            for (int i = 0; i < rows->count(); ++i)
                if (!rows->item(i)->isHidden() && rows->item(i)->data(Qt::UserRole).isValid())
                    result.append(rows->item(i)->data(Qt::UserRole).toString());
            result.sort(); return result;
        };
        for (int section = 0; section < 8; ++section) {
            sections->setCurrentRow(section + 1);
            const bool populated = section == 3 || section == 4;
            require(sections->item(section + 1)->isHidden() == !populated);
            if (!populated) { require(sections->currentRow() == 0 && combo->count() == 3); continue; }
            const bool hasTopic = true;
            require(combo->currentIndex() == 0 && combo->isEnabled() == hasTopic);
            require(combo->count() == (hasTopic ? 2 : 1));
            require(combo->findData("without") < 0);
        }
        for (auto it = addedTopics.cbegin(); it != addedTopics.cend(); ++it) {
            QStringList actual;
            for (auto entry = geoSiteEntries().cbegin(); entry != geoSiteEntries().cend(); ++entry)
                if (QString::fromUtf8(entry->topic) == it.key()) actual.append(entry.key());
            auto expected = it.value(); expected.sort(); require(actual == expected);
            sections->setCurrentRow(topics.value(it.key()) + 1);
            combo->setCurrentIndex(combo->findData(it.key())); require(visible() == expected);
            query->setText("Windy"); require(visible() == QStringList{"windy"});
            query->setText(QString::fromUtf8("Прогнозы погоды")); require(visible().contains("windy"));
            query->clear(); require(visible() == expected);
            picker.findChild<QPushButton *>("geoSiteSelectionCount")->click();
            require(visible() == QStringList({"missing-id", "salesforce", "windy"}));
            require(picker.selectedIds() == QStringList({"salesforce", "windy", "missing-id"}));
            picker.findChild<QPushButton *>("geoSiteSelectionCount")->click();
            sections->setCurrentRow(0); require(combo->currentIndex() == 0 && combo->count() == 3);
        }
    };
    checkAddedNavigation();
    require(geoSiteEntries().value("piratebay").section == 2);
    require(geoSiteEntries().value("beisen").section == 3);
    require(!geoSiteEntries().contains("future-unknown") && geoSiteName("future-unknown").isEmpty());
    GeoSitePicker picker(nullptr, {"github", "youtube", "abema", "future-unknown", "category-ads", "github"}, {"missing-id", "youtube"}, {});
    auto *list = picker.findChild<QListWidget *>("geoSiteGroups");
    auto *sections = picker.findChild<QListWidget *>("geoSiteSections");
    auto *topic = picker.findChild<QComboBox *>("geoSiteTopics");
    auto *search = picker.findChild<QLineEdit *>();
    auto *count = picker.findChild<QPushButton *>("geoSiteSelectionCount");
    auto row = [list](const QString &id) -> QListWidgetItem * {
        for (int i = 0; i < list->count(); ++i) if (list->item(i)->data(Qt::UserRole) == id) return list->item(i);
        return nullptr;
    };
    auto *github = row("github"), *youtube = row("youtube"), *abema = row("abema"), *unknown = row("future-unknown"), *missing = row("missing-id");
    require(github && youtube && abema && unknown && missing && topic->count() == 4);
    int groupCount = 0;
    for (int i = 0; i < list->count(); ++i) groupCount += list->item(i)->data(Qt::UserRole).isValid();
    require(groupCount == 6 && unknown->text() == "future-unknown");
    sections->setCurrentRow(2);
    topic->setCurrentIndex(topic->findData("2-video"));
    require(!youtube->isHidden() && abema->isHidden() && unknown->isHidden());
    search->setText("GitHub"); require(!github->isHidden() && youtube->isHidden());
    search->clear(); require(github->isHidden() && !youtube->isHidden());
    topic->setCurrentIndex(topic->findData("without")); require(!abema->isHidden() && youtube->isHidden() && unknown->isHidden());
    count->click(); require(!missing->isHidden() && !youtube->isHidden() && abema->isHidden());
    search->setText("future-unknown"); require(unknown->isHidden() && missing->isHidden());
    search->clear(); require(unknown->isHidden() && !missing->isHidden());
    missing->setCheckState(Qt::Unchecked); require(missing->isHidden());
    count->click();
    topic->setCurrentIndex(topic->findData("2-video"));
    require(!count->isChecked() && !youtube->isHidden());
    count->click(); require(count->isChecked() && count->text().contains("Only selected"));
    count->click();
    sections->setCurrentRow(9); require(!count->isChecked() && topic->currentIndex() == 0 && !topic->isEnabled());
    require(!unknown->isHidden() && missing->isHidden() && youtube->isHidden());
    sections->setCurrentRow(4); require(topic->isEnabled());
    search->setText(QString::fromUtf8("Реклама"));
    require(!row("category-ads")->isHidden());
    search->setText("Advertising"); require(!row("category-ads")->isHidden());
    search->setText("future-unknown"); unknown->setCheckState(Qt::Checked);
    search->setText("github"); github->setCheckState(Qt::Checked);
    require(picker.selectedIds() == QStringList({"youtube", "future-unknown", "github"}));
    for (int i = 0; i < list->count(); ++i) {
        auto *item = list->item(i);
        if (!item->data(Qt::UserRole).isValid()) {
            bool visible = false;
            for (int j = i + 1; j < list->count() && list->item(j)->data(Qt::UserRole).isValid(); ++j) visible |= !list->item(j)->isHidden();
            require(item->isHidden() == !visible);
        }
    }
    picker.reject(); require(picker.result() == QDialog::Rejected);
    class Translator final : public QTranslator {
    public:
        QJsonObject strings;
        QString translate(const char *, const char *text, const char *, int) const override { return strings.value(QString::fromUtf8(text)).toString(); }
        bool isEmpty() const override { return false; }
    } translator;
    QFile translations(":/translations/ru.json"); require(translations.open(QIODevice::ReadOnly));
    translator.strings = QJsonDocument::fromJson(translations.readAll()).object();
    for (auto it = geoSiteEntries().cbegin(); it != geoSiteEntries().cend(); ++it)
        require(!translator.strings.value(QString::fromUtf8(it->name)).toString().isEmpty());
    for (const auto &topic : geoSiteTopics()) require(!translator.strings.value(QString::fromUtf8(topic.name)).toString().isEmpty());
    qApp->installTranslator(&translator);
    checkInteraction();
    checkMixedNavigation();
    checkTopicSearch();
    checkSplitNavigation();
    checkAddedNavigation();
    GeoSitePicker russian(nullptr, {"category-ads", "github", "youtube"}, {"missing-id"}, {});
    auto *russianList = russian.findChild<QListWidget *>("geoSiteGroups");
    auto *russianSearch = russian.findChild<QLineEdit *>();
    for (const QString &query : {QString("Advertising"), QString::fromUtf8("Реклама"), QString("category-ads")}) {
        russianSearch->setText(query);
        bool matched = false;
        for (int i = 0; i < russianList->count(); ++i) {
            auto *item = russianList->item(i);
            if (item->data(Qt::UserRole) == "category-ads") matched = !item->isHidden();
        }
        require(matched);
    }
    for (int i = 0; i < russianList->count(); ++i)
        if (russianList->item(i)->data(Qt::UserRole).isValid()) russianList->item(i)->setCheckState(Qt::Checked);
    require(russian.selectedIds() == QStringList({"missing-id", "category-ads", "github", "youtube"}));
    require(geoSiteName("category-ads").contains(QString::fromUtf8("Реклама")));
    qApp->removeTranslator(&translator);
}

int main(int argc, char **argv) {
    require(routingGroupContains({"us", "google"}, " US "));
    require(routingGroupRows({"google", "GOOGLE", "us"}, {" Google ", "US"})
        == QStringList({"google", "us"}));
    require(routingRetainedGroups({"us", "google", "new"}, {" Google ", "US"})
        == QStringList({" Google ", "US", "new"}));
    require(!routingHasSection("# [IKEv2]\n; [IKEv2]\nName = \"[IKEv2]\"\n", "IKEv2"));
    require(routingHasSection(QString(QChar::ByteOrderMark) + " [IKEv2]\n", "IKEv2"));

    QApplication app(argc, argv);
    geoSiteClassificationTest();
    {
        QStringList catalog;
        const QString catalogPath = qEnvironmentVariable("BIMERON_GEOSITE_IDS");
        if (!catalogPath.isEmpty()) {
            QFile file(catalogPath); require(file.open(QIODevice::ReadOnly));
            for (const auto &id : QJsonDocument::fromJson(file.readAll()).array()) catalog.append(id.toString().toLower());
            require(catalog.size() == 1561);
        } else {
            for (int i = 0; i < 1558; ++i) catalog.append(QString("fixture-%1").arg(i));
            catalog.append({"category-games-!cn", "github", "youtube"});
        }
        QElapsedTimer performance; performance.start();
        GeoSitePicker picker(nullptr, catalog, {"youtube", "missing-fixture"}, {});
        const qint64 opening = performance.nsecsElapsed();
        auto *list = picker.findChild<QListWidget *>("geoSiteGroups");
        auto *sections = picker.findChild<QListWidget *>("geoSiteSections");
        auto *search = picker.findChild<QLineEdit *>();
        require(list && sections && search);
        require(picker.selectedIds() == QStringList({"youtube", "missing-fixture"}));
        sections->setCurrentRow(2);
        performance.restart(); search->setText("github");
        const qint64 filtering = performance.nsecsElapsed();
        int matches = 0;
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            if (item->data(Qt::UserRole).isValid() && !item->isHidden()) {
                require(item->text().contains("github", Qt::CaseInsensitive));
                if (item->data(Qt::UserRole) == "github") { item->setCheckState(Qt::Checked); ++matches; }
            }
        }
        require(matches == 1);
        require(picker.selectedIds() == QStringList({"youtube", "missing-fixture", "github"}));
        search->clear();
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole) == "github") require(list->item(i)->isHidden());
        require(picker.findChild<QPushButton *>("geoSiteSelectionCount")->text().contains('3'));
        search->setText("fixture-no-matches");
        picker.reject(); require(picker.result() == QDialog::Rejected);
        performance.restart();
        for (int i = 0; i < 100; ++i) search->setText(i % 2 ? "git" : "github");
        const qint64 repeatedQueries = performance.nsecsElapsed();
        search->clear();
        auto *selectedMode = picker.findChild<QPushButton *>("geoSiteSelectionCount");
        performance.restart();
        for (int i = 0; i < 100; ++i) selectedMode->click();
        const qint64 repeatedToggles = performance.nsecsElapsed();
        performance.restart();
        for (int i = 0; i < 100; ++i) sections->setCurrentRow(i % 2 ? 7 : 2);
        const qint64 repeatedModes = performance.nsecsElapsed();
        fprintf(stdout, "GeoSite repeated means: query %.3f ms, selected toggle %.3f ms, AI/browse %.3f ms (100 each)\n",
            repeatedQueries / 1e8, repeatedToggles / 1e8, repeatedModes / 1e8);
        fprintf(stdout, "GeoSite %d IDs: construct %.2f ms, filter %.2f ms\n", int(catalog.size()), opening / 1e6, filtering / 1e6);
    }
    require(geoIPLabel("us") == "United States (US)");
    require(geoIPLabel("ru") == "Russia (RU)");
    require(geoIPLabel("by") == "Belarus (BY)");
    require(geoIPLabel("cloudfront") == "Amazon CloudFront (CLOUDFRONT)");
    require(geoIPLabel("private") == "Private and special networks (PRIVATE)");
    require(geoIPLabel("unknown-id") == "unknown-id");
    require(geoIPLabel("xk") == "Kosovo (XK)");
    require(geoIPSearchText("ru").contains(QString::fromUtf8("Россия")));
    require(geoIPSearchText("ru").contains("Russia"));
    class CountryTranslator final : public QTranslator {
    public:
        QJsonObject strings;
        QString translate(const char *, const char *source, const char *, int) const override {
            return strings.value(QString::fromUtf8(source)).toString();
        }
        bool isEmpty() const override { return strings.isEmpty(); }
    } countryTranslator;
    QFile countryTranslations(":/translations/ru.json");
    require(countryTranslations.open(QIODevice::ReadOnly));
    countryTranslator.strings = QJsonDocument::fromJson(countryTranslations.readAll()).object();
    require(geoIPLabel("ru", true) == "Russia (RU)");
    app.installTranslator(&countryTranslator);
    require(geoIPLabel("ru", true) == QString::fromUtf8("Россия (RU)"));
    require(geoIPSearchText("ru").contains("Russia"));
    app.removeTranslator(&countryTranslator);
    const QLocale previousLocale;
    QLocale::setDefault(QLocale("ru_RU"));
    require(geoIPLabel("us") == "United States (US)");
    QLocale::setDefault(previousLocale);
    const QString catalogPath = qEnvironmentVariable("BIMERON_GEOIP_COUNTRY_CATALOG");
    if (!catalogPath.isEmpty()) {
        QFile catalog(catalogPath); require(catalog.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(catalog.readAll()).split('\n', Qt::SkipEmptyParts);
        int count = 0;
        for (const QString &line : lines) {
            const auto match = QRegularExpression("\\(([A-Z]{2})\\)").match(line);
            if (!match.hasMatch()) continue;
            const QString id = match.captured(1).toLower();
            require(geoIPLabel(id) != id && geoIPLabel(id).endsWith('(' + id.toUpper() + ')'));
            const QString english = geoIPLabel(id).chopped(5);
            require(!countryTranslator.strings.value(english).toString().isEmpty());
            require(geoIPSearchText(id).contains(countryTranslator.strings.value(english).toString()));
            ++count;
        }
        require(count == 250);
    }
    if (app.arguments().size() == 3 && app.arguments().at(1) == "--permission-fixture")
        return ProfileStore::writeConfig(app.arguments().at(2), "synthetic permission fixture") ? 0 : 1;
    QTemporaryDir temporary;
    require(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings().setValue("geodata/paths", QVariantMap{{"geoip", "geoip.dat"}, {"geosite", "geosite.dat"}});
    geodataBridgeRegressions(temporary.path());
    const QJsonObject geoCaps{{"ip_split", true}, {"domain_split", true}, {"geoip_split", true},
        {"geosite_split", true}, {"geodata_schema", "v2ray-protobuf"}};
    require(routingCompatibilityError("[Rule]\nGeoIP=ru\nAction=direct\n", geoCaps).isEmpty());
    require(!routingCompatibilityError("[Rule]\nGeoIP=ru\nAction=direct\n", {}).isEmpty());
    require(routingCompatibilityError("# [Rule]\n# GeoIP=ru\n# Action=direct\n", {}).isEmpty());
    require(!routingCompatibilityError("[Interface]\nKillSwitch=true\n[Rule]\nDomain=example.com\nAction=direct\n", geoCaps).isEmpty());
    require(routingCompatibilityError("[Interface]\nKillSwitch=true\n[Rule]\nCIDR=10.0.0.0/8\nAction=direct\n", geoCaps).isEmpty());
    require(!routingCompatibilityError("[IKEv2]\n[Rule]\nGeoSite=github\nAction=proxy\n", geoCaps).isEmpty());
    auto noSchema = geoCaps;
    noSchema.remove("geodata_schema");
    require(!routingCompatibilityError("[Rule]\nGeoSite=github\nAction=proxy\n", noSchema).isEmpty());
    require(geoDataValidationError("{\"version\":1}").isEmpty());
    require(!geoDataValidationError("not JSON").isEmpty());
    require(geoDataValidationError("{\"version\":1,\"error\":\"request failed\",\"details\":{\"code\":\"geodata_group_unknown\",\"group\":\"missing\",\"rule\":2}}").contains("Active rule 2"));
    require(routingInterfaceValue("[Interface]\nMode=\"proxy\"\n", "Mode", "vpn") == "proxy");
    require(routingInterfaceValue("[Interface]\nIPv6=\"disable\"\n", "IPv6", "enable") == "disable");
    require(routingInterfaceValue("[Interface]\nKillSwitch=\"true\"\n", "KillSwitch", "false") == "true");
    require(ProfileStore::connectionInfo("[AccessKey]\nSOCKS5=socks5://localhost:1080\n[Interface]\nMode=\"proxy\"\n").interfaceMode == "proxy");
    QString geoImportError;
    const QString quotedGeoPath = QString::fromUtf8(QJsonDocument(QJsonArray{temporary.filePath("geoip.dat")}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
    require(ProfileStore::importReferencesSafe(("[GeoData]\nGeoIPFile=" + quotedGeoPath + "\n").toUtf8(), &geoImportError));
    require(!ProfileStore::importReferencesSafe("[GeoData]\nGeoIPFile=\"data/geoip.dat\"\n", &geoImportError));

    require(!ProfileStore::importReferencesSafe("[GeoData]\nGeoIPFile=data/geoip.dat\n", &geoImportError));
    require(!geoImportError.isEmpty());
    require(ProfileStore::importReferencesSafe("[GeoData]\n# GeoIPFile=data/geoip.dat\n", &geoImportError));
    require(ProfileStore::importReferencesSafe(("[GeoData]\nGeoIPFile=" + temporary.filePath("geoip.dat") + "\n").toUtf8(), &geoImportError));

    if (app.arguments().size() == 4 && (app.arguments().at(1) == "--ss-preview" || app.arguments().at(1) == "--parameters-preview")) {
        class Translator final : public QTranslator {
        public:
            QJsonObject strings;
            QString translate(const char *, const char *source, const char *, int) const override {
                return strings.value(QString::fromUtf8(source)).toString();
            }
            bool isEmpty() const override { return strings.isEmpty(); }
        } translator;
        if (app.arguments().at(3) == "ru") {
            QFile file(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json"));
            require(file.open(QIODevice::ReadOnly));
            translator.strings = QJsonDocument::fromJson(file.readAll()).object();
            app.installTranslator(&translator);
        }
        ProfileEditorDialog editor("[Profile]\nVersion = 1\nName = SS2022\n[AccessKey]\nType = shadowsocks\nServer = example.invalid\nPort = 443\nMethod = 2022-blake3-aes-256-gcm\nPassword = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\nPlugin = v2ray-plugin\nPluginOpts = tls;host=example.invalid;mux=0\n",
            QJsonObject{{"vpn", true}, {"proxy", true}});
        if (app.arguments().at(1) == "--parameters-preview") {
            const QString kind = qEnvironmentVariable("BIMERON_PREVIEW_TYPE", "trojan");
            QString access = "Type = " + kind + "\nServer = example.invalid\nPort = 443\n";
            if (kind == "trojan") access += "Password = synthetic\nNetwork = grpc\nGRPCServiceName = proxy\nGRPCMultiMode = false\n";
            if (kind == "naive" || kind == "https") access += "Username = synthetic\nPassword = synthetic\n";
            if (kind == "masque") access += "MASQUEPath = /.well-known/masque/udp/{target_host}/{target_port}/\n";
            if (kind == "trojan" || kind == "naive") access += "[ShadowTLS]\nServerName = outer.invalid\nPassword = synthetic\n";
            ProfileEditorDialog parameters("[Profile]\nVersion = 1\nName = Parameters\n[AccessKey]\n" + access,
                QJsonObject{{"vpn", true}, {"proxy", true}});
            if (qEnvironmentVariable("BIMERON_PREVIEW_PAGE") != "general")
                parameters.findChild<QTabWidget *>()->setCurrentIndex(parameters.findChild<QTabWidget *>()->count() - 2);
            parameters.resize(720, 600);
            parameters.show(); app.processEvents();
            require(parameters.grab().save(app.arguments().at(2)));
        } else {
            editor.show(); app.processEvents();
            require(editor.grab().save(app.arguments().at(2)));
        }
        return 0;
    }
    routingHostRegressions(temporary.path());
    parameterForms(temporary.path());
    nativeRegressions(temporary.path());
    preservationRegressions(temporary.path());
    sshProfiles(temporary.path());
    ProfileStore uriStore(temporary.path() + "/uri-import");
    for (const QString &scheme : {"vmess", "trojan", "tuic", "http", "https", "anytls", "naive+https", "naive+quic", "masque"}) {
        const QString uri = scheme + "://synthetic:443";
        const QString field = ProfileStore::uriSourceField(uri);
        require(!field.isEmpty());
        QString error;
        const QString path = uriStore.createConfig("URI", uri, &error);
        require(!path.isEmpty());
        QFile file(path);
        require(file.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(file.readAll());
        require(text.contains(field + " = " + uri));
        require(ProfileStore::connectionInfo(text).protocol == field);
        ProfileEditorDialog editor(text, {});
        require(editor.configText() == text);
        require(!editor.findChild<QTabWidget *>()->isTabVisible(1));
    }
    require(ProfileStore::uriSourceField("trojan://secret\n[Future]").isEmpty());
    for (const QString &bad : {"http://host", "https://host:443/", "http://host:80?q=x",
            "https://host:443#secret", "https://user@host:443", "future://host:443",
            "masque://host?ca=relative.pem"}) {
        QString error;
        const auto before = QDir(temporary.path() + "/uri-import").entryList(QDir::Files);
        require(uriStore.createConfig("Invalid", bad, &error).isEmpty());
        require(!error.isEmpty());
        require(QDir(temporary.path() + "/uri-import").entryList(QDir::Files) == before);
    }
    QString oversizedError = "old error";
    require(uriStore.createConfig("Large", "vmess://" + QString(65536, 'A'), &oversizedError).isEmpty());
    require(oversizedError != "old error");


    const QString core = qEnvironmentVariable("BIMERON_VLESS_CORE");
    const QString uuid = "123e4567-e89b-12d3-a456-426614174000";
    const QString link = "vless://" + uuid + "@[2001:db8::1]:443?encryption=none&type=grpc&security=tls&serviceName=proxy#Example";
    qputenv("XDG_DATA_HOME", temporary.path().toUtf8());
    ProfileStore store(temporary.filePath("profiles"));
    QString error;
    const QString saved = store.createConfig("Example", link, &error);
    require(!saved.isEmpty());
    QFile savedFile(saved);
    require(savedFile.open(QIODevice::ReadOnly));
    const QString uriProfile = QString::fromUtf8(savedFile.readAll());
    savedFile.close();
    check(uriProfile, {"VLESS = " + link}, core, temporary.filePath("uri.conf"));
    ProfileEditorDialog vlessTabs(uriProfile, {});
    require(vlessTabs.findChild<QTabWidget *>()->isTabVisible(1));
    check("[Profile]\nVersion = 1\n[AccessKey]\nKey = " + link + "\n",
        {"Key = " + link}, core, temporary.filePath("legacy.conf"));
    require(ProfileStore::connectionInfo("[AccessKey]\nKey = " + link).protocol == "VLESS");
    require(ProfileStore::connectionInfo("[AccessKey]\nKey = ssconf://example.com").protocol == QStringLiteral("Shadowsocks ⟳"));
    require(ProfileStore::connectionInfo("[AccessKey]\nFile = key.txt").connectable);
    require(ProfileStore::connectionInfo("[AccessKey]\nFile = key.txt").protocol.isEmpty());
    require(!ProfileStore::connectionInfo("[AccessKey]\nKey = unknown://example.com").connectable);
    const QString hysteriaCore = qEnvironmentVariable("BIMERON_HYSTERIA2_CORE");
    const QString hysteriaLink = "hysteria2://p%40ss@[2001:db8::1]:443?sni=server.example#Example";
    const QString hysteriaPath = store.createConfig("Example", hysteriaLink, &error);
    require(!hysteriaPath.isEmpty());
    QFile hysteriaFile(hysteriaPath);
    require(hysteriaFile.open(QIODevice::ReadOnly));
    const QString hysteriaProfile = QString::fromUtf8(hysteriaFile.readAll());
    check(hysteriaProfile, {"Hysteria2 = " + hysteriaLink}, hysteriaCore, temporary.filePath("hysteria2.conf"));
    require(ProfileStore::connectionInfo(hysteriaProfile).protocol == "Hysteria2");
    require(ProfileStore::connectionInfo(hysteriaProfile).connectable);
    ProfileEditorDialog hysteriaEditor(hysteriaProfile, QJsonObject{{"vpn", true}, {"proxy", true}});
    require(hysteriaEditor.configText().contains("Hysteria2 = " + hysteriaLink));
    require(!hysteriaEditor.findChild<QComboBox *>("keySource")->isEnabled());
    const QString alias = "hy2://secret@server.example:443#Alias";
    const QString aliasPath = store.createConfig("Alias", alias, &error);
    require(!aliasPath.isEmpty());
    QFile aliasFile(aliasPath);
    require(aliasFile.open(QIODevice::ReadOnly));
    require(QString::fromUtf8(aliasFile.readAll()).contains("Hysteria2 = " + alias));
    check("[Profile]\nVersion = 1\n[AccessKey]\nKey = " + alias + "\n",
        {"Key = " + alias}, hysteriaCore, temporary.filePath("hy2-key.conf"));
    require(ProfileStore::connectionInfo("[AccessKey]\nKey = " + alias).protocol == "Hysteria2");
    require(ProfileStore::connectionInfo("[AccessKey]\nHysteria2 = " + alias).connectable);
    require(!ProfileStore::connectionInfo("[AccessKey]\nHysteria2 = hy2://@server.example:443").connectable);
    require(!ProfileStore::connectionInfo("[AccessKey]\nHysteria2 = hy2://secret@server.example").connectable);
    require(!ProfileStore::connectionInfo("[AccessKey]\nHysteria2 = hy2://secret@server.example:443/path").connectable);
    const QString queryLink = "hysteria2://[2001:db8::1]:443?auth=p%40ss&sni=server.example&insecure=0&alpn=h3#Example";
    const QString queryPath = store.createConfig("Query", queryLink, &error);
    require(!queryPath.isEmpty());
    QFile queryFile(queryPath);
    require(queryFile.open(QIODevice::ReadOnly));
    const QString queryProfile = QString::fromUtf8(queryFile.readAll());
    check(queryProfile, {"Hysteria2 = " + queryLink}, hysteriaCore, temporary.filePath("query-hy2.conf"));
    require(ProfileStore::connectionInfo(queryProfile).connectable);
    ProfileEditorDialog queryEditor(queryProfile, QJsonObject{{"vpn", true}});
    require(queryEditor.configText().contains("Hysteria2 = " + queryLink));
    for (const QString &fp : {"firefox", "firefox-116", "firefox-116b", "firefox-116c", "chrome", "chrome-115", "chrome-146"}) {
        const QString fingerprintLink = queryLink.section('#', 0, 0) + "&fp=" + fp + "#Example";
        require(ProfileStore::connectionInfo("[AccessKey]\nHysteria2 = " + fingerprintLink).connectable);
        check("[Profile]\nVersion = 1\n[AccessKey]\nHysteria2 = " + fingerprintLink + "\n",
            {"Hysteria2 = " + fingerprintLink}, hysteriaCore, temporary.filePath("fingerprint-" + fp + ".conf"));
    }
    require(!ProfileStore::connectionInfo("[AccessKey]\nHysteria2 = hy2://server.example:443?auth=secret&fp=firefox-117").connectable);
    require(!ProfileStore::connectionInfo("[AccessKey]\nHysteria2 = hy2://secret@server.example:443?auth=other").connectable);
    require(!safeCoreDiagnostic("error: hy2://secret@server.example:443").contains("secret"));
    require(!safeCoreDiagnostic("Hysteria2 = hysteria2://secret@server.example:443").contains("secret"));
    rejectWithCore("hy2://secret@server.example:443?obfs=salamander", hysteriaCore, temporary.filePath("unsupported-hy2.conf"));
    rejectWithCore("hy2://secret@server.example:443?sni=one.example&sni=two.example", hysteriaCore, temporary.filePath("duplicate-hy2.conf"));
    rejectWithCore("hy2://server.example:443?auth=secret&fp=firefox-117", hysteriaCore, temporary.filePath("fingerprint-hy2.conf"));
    require(ProfileStore::connectionInfo("[Interface]\nMode = proxy\n").interfaceMode == "proxy");
    require(ProfileStore::connectionInfo("[Interface]\nMode = vpn\n").interfaceMode == "vpn");
    ProfileEditorDialog uriEditor(uriProfile, QJsonObject{{"vpn", true}, {"proxy", true}});
    require(!uriEditor.findChild<QComboBox *>("keySource")->isEnabled());
    require(uriEditor.findChild<QLineEdit *>("vlessId")->text() == uuid);
    require(!uriEditor.findChild<QLineEdit *>("vlessId")->isEnabled());
    require(uriEditor.findChild<QLineEdit *>("vlessEndpoint")->text().contains("443"));
    require(uriEditor.findChild<QComboBox *>("vlessNetwork")->currentText() == "grpc");
    require(uriEditor.findChild<QComboBox *>("vlessSecurity")->currentText() == "tls");
    require(uriEditor.findChild<QLineEdit *>("vlessServiceName")->text() == "proxy");
    ProfileEditorDialog grpcEditor(QString(uriProfile).replace("serviceName=proxy", "serviceName=proxy&mode=multi"), QJsonObject{{"vpn", true}});
    require(grpcEditor.findChild<QCheckBox *>("vlessMultiMode")->isChecked());
    const QString wsLink = "vless://" + uuid + "@server.example:443?encryption=none&type=ws&security=tls&path=%2Fchat%3Fed%3D2048";
    ProfileEditorDialog wsEditor("[AccessKey]\nVLESS = " + wsLink + "\n", QJsonObject{{"vpn", true}});
    require(wsEditor.findChild<QLineEdit *>("vlessPath")->text() == "/chat");
    require(wsEditor.findChild<QSpinBox *>("vlessEarlyData")->value() == 2048);
    ProfileEditorDialog invalidEditor("[Profile]\nName = Invalid\n[AccessKey]\nKey = unknown://example.com\n", QJsonObject{{"vpn", true}});
    require(!invalidEditor.findChild<QTabWidget *>()->isTabEnabled(0));
    const QString extendedLink = "vless://" + uuid + "@server.example:443?encryption=none&type=xhttp&security=tls"
        "&mode=stream-up&alpn=h3&extra=%7B%22headers%22%3A%7B%22User-Agent%22%3A%22Example%22%7D%7D";
    check("[Profile]\nVersion = 1\n[AccessKey]\nVLESS = " + extendedLink + "\n",
        {"VLESS = " + extendedLink}, core, temporary.filePath("extended-link.conf"));
    for (const auto &scheme : {"ss://", "ssconf://", "socks5://"}) {
        const QString path = store.createConfig("Existing", QString::fromLatin1(scheme) + "example", &error);
        require(!path.isEmpty());
        QFile::remove(path);
    }

    const QString base = "[Profile]\nVersion = 1\nName = Example\n[AccessKey]\nType = vless\nServer = server.example\nPort = 443\nID = " + uuid + "\nEncryption = none\n";
    const QString tail = "[Interface]\nMode = proxy\n";
    const struct { const char *settings; const char *field; } cases[] = {
        {"Network = tcp\nSecurity = tls\nServerName = server.example\n", "Network = tcp"},
        {"Network = tcp\nSecurity = reality\nFlow = xtls-rprx-vision\nServerName = server.example\nFingerprint = firefox\nPublicKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\nShortID = abcd\n", "Flow = xtls-rprx-vision"},
        {"Network = tcp\nSecurity = reality\nServerName = server.example\nFingerprint = firefox\nPublicKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n", "Security = reality"},
        {"Network = ws\nSecurity = tls\nWSPath = /chat\nWSHost = front.example\nWSEarlyData = 2048\nALPN = http/1.1\n", "WSEarlyData = 2048"},
        {"Network = grpc\nSecurity = tls\nGRPCServiceName = proxy\nALPN = h2\n", "GRPCServiceName = proxy"},
        {"Network = grpc\nSecurity = tls\nGRPCServiceName = proxy\nGRPCMultiMode = false\n", "GRPCMultiMode = false"},
        {"Network = grpc\nSecurity = reality\nGRPCServiceName = /proxy/Custom\nGRPCAuthority = front.example\nGRPCMultiMode = true\nServerName = server.example\nFingerprint = firefox\nPublicKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\nALPN = h2\n", "GRPCMultiMode = true"},
        {"Network = xhttp\nSecurity = tls\nXHTTPMode = packet-up\nXHTTPPath = /stream\nALPN = h2\n", "XHTTPMode = packet-up"},
        {"Network = xhttp\nSecurity = tls\nXHTTPMode = stream-up\nALPN = h3\n", "XHTTPMode = stream-up"},
        {"Network = xhttp\nSecurity = tls\nXHTTPMode = auto\n", "XHTTPMode = auto"},
        {"Network = xhttp\nSecurity = reality\nXHTTPMode = stream-one\nServerName = server.example\nFingerprint = firefox\nPublicKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n", "XHTTPMode = stream-one"},
        {"Network = xhttp\nSecurity = reality\nXHTTPMode = stream-up\nServerName = server.example\nFingerprint = firefox\nPublicKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n", "XHTTPMode = stream-up"},
        {"Network = xhttp\nSecurity = reality\nXHTTPMode = auto\nServerName = server.example\nFingerprint = firefox\nPublicKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n", "XHTTPMode = auto"},
    };
    int index = 0;
    for (const auto &item : cases)
        check(base + item.settings + tail, {"Type = vless", item.field, "ID = " + uuid}, core, temporary.filePath(QString::number(index++) + ".conf"));

    const QString extended = base + "Network = xhttp\nSecurity = tls\nXHTTPMode = packet-up\n"
        "XHTTPExtra = {\"headers\":{\"User-Agent\":\"Example\"}}\n" + tail;
    ProfileEditorDialog editor(extended, QJsonObject{{"vpn", true}, {"proxy", true}, {"ip_split", true}, {"domain_split", true}});
    QTabWidget *tabs = editor.findChild<QTabWidget *>();
    QPlainTextEdit *raw = editor.findChild<QPlainTextEdit *>("rawConfig");
    require(tabs && raw);
    tabs->setCurrentIndex(tabs->count() - 1);
    require(raw->toPlainText() == extended && editor.configText() == extended);
    editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    require(editor.result() == QDialog::Accepted && editor.configText() == extended);
    raw->appendPlainText("# retained");
    require(editor.configText().contains("XHTTPExtra =") && editor.configText().contains("# retained"));
    check(extended, {"XHTTPExtra ="}, core, temporary.filePath("extended.conf"));
    const QString separate = base + "Network = xhttp\nSecurity = tls\nXHTTPMode = packet-up\n"
        "XHTTPHeaders = {\"User-Agent\":\"Example\"}\n" + tail;
    check(separate, {"XHTTPHeaders ="}, core, temporary.filePath("separate.conf"));
    return 0;
}
