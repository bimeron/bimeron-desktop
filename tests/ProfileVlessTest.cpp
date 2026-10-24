#include "ProfileEditorDialog.h"
#include "ProfileStore.h"

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
