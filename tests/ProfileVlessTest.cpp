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
#include <QTimer>

#include <cstdlib>

namespace {
void require(bool condition) { if (!condition) std::abort(); }

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
    require(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0);
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

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid());
    const QString core = qEnvironmentVariable("BIMERON_VLESS_CORE");
    const QString uuid = "123e4567-e89b-12d3-a456-426614174000";
    const QString link = "vless://" + uuid + "@[2001:db8::1]:443?encryption=none&type=grpc&security=tls&serviceName=proxy#Example";
    qputenv("XDG_DATA_HOME", temporary.path().toUtf8());
    ProfileStore store;
    QString error;
    const QString saved = store.createConfig("Example", link, &error);
    require(!saved.isEmpty());
    QFile savedFile(saved);
    require(savedFile.open(QIODevice::ReadOnly));
    const QString uriProfile = QString::fromUtf8(savedFile.readAll());
    savedFile.close();
    check(uriProfile, {"VLESS = " + link}, core, temporary.filePath("uri.conf"));
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
    require(invalidEditor.findChild<QComboBox *>("keySource")->isEnabled());
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
    QPlainTextEdit *raw = editor.findChild<QPlainTextEdit *>();
    require(tabs && raw);
    tabs->setCurrentIndex(tabs->count() - 1);
    require(raw->toPlainText() == extended && editor.configText() == extended);
    QTimer::singleShot(0, [] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->accept();
    });
    editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    require(editor.result() != QDialog::Accepted && editor.configText() == extended);
    raw->appendPlainText("# retained");
    require(editor.configText().contains("XHTTPExtra =") && editor.configText().contains("# retained"));
    check(extended, {"XHTTPExtra ="}, core, temporary.filePath("extended.conf"));
    const QString separate = base + "Network = xhttp\nSecurity = tls\nXHTTPMode = packet-up\n"
        "XHTTPHeaders = {\"User-Agent\":\"Example\"}\n" + tail;
    check(separate, {"XHTTPHeaders ="}, core, temporary.filePath("separate.conf"));
    return 0;
}
