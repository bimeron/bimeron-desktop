#include "ProfileEditorDialog.h"
#include "ProfileStore.h"
#include "CoreDiagnostic.h"

#include <QApplication>
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

namespace {
void requireAt(bool condition, int line) { if (!condition) qFatal("Profile regression failed at line %d", line); }
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
        require(raw->isHidden());
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
    recovery.findChild<QPushButton *>("useVisual")->click();
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
    rejected.findChild<QPushButton *>("useVisual")->click();
    require(safeError && rejected.configText() == rejectedDraft);
    require(!rejected.findChild<QTabWidget *>()->isTabEnabled(0));
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
    for (bool protection : {false, true}) {
        QString input = base;
        if (protection) input.replace("[Interface]", "KillSwitch = true\n[Interface]");
        input.replace("\n", "\r\n");
        input.prepend(QChar(0xfeff));
        const auto info = ProfileStore::connectionInfo(input);
        require(info.backend == ProfileConnectionInfo::Backend::NativeIKEv2);
        require(info.protocol == "IKEv2" && !info.connectable && info.keyFile.isEmpty());
        ProfileEditorDialog editor(input, caps);
        auto *tabs = editor.findChild<QTabWidget *>();
        require(tabs->isTabEnabled(0));
        require(editor.configText() == input);
        require(editor.findChild<QCheckBox *>("nativeKillSwitch")->isChecked() == protection);
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
        require(editor.findChild<QPlainTextEdit *>("rawConfig")->isHidden());
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
    rejected.findChild<QPlainTextEdit *>("rawConfig")->appendPlainText("# draft");
    const QString draft = rejected.configText();
    QTimer::singleShot(0, [] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box);
        box->accept();
    });
    rejected.findChild<QPushButton *>("useVisual")->click();
    require(rejected.configText() == draft);
    bool validated = false;
    ProfileEditorDialog transition(base, caps, nullptr, [&](const QString &, QString *) { validated = true; return true; });
    const QString packet = "[Profile]\nName = Packet\n[AccessKey]\nSSH = ssh://alice@example.invalid\n[Interface]\nMode = proxy\n";
    transition.findChild<QPlainTextEdit *>("rawConfig")->setPlainText(packet);
    transition.findChild<QPushButton *>("useVisual")->click();
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

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (app.arguments().size() == 3 && app.arguments().at(1) == "--permission-fixture")
        return ProfileStore::writeConfig(app.arguments().at(2), "synthetic permission fixture") ? 0 : 1;
    QTemporaryDir temporary;
    require(temporary.isValid());
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
