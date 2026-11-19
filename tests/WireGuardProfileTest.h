#pragma once

// WireGuard/AmneziaWG regressions for profile-vless-test. Included after the
// test's require() helper. A fake `bimeron wireguard` bridge stands in for
// Core: it implements only what these fixtures need and is not a reference
// parser. Set BIMERON_WIREGUARD_CORE to a Core with the bridge to repeat the
// round trips against the real implementation.

#include "WireGuardCore.h"
#include "WireGuardForm.h"

#include <QToolButton>

namespace wireguardfixture {

const QString privateKey = QStringLiteral("PRIVATE_SENTINEL_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=");

QStringList splitLines(const QString &text) {
    QStringList lines;
    qsizetype start = 0;
    for (qsizetype i = 0; i < text.size(); ++i)
        if (text[i] == '\n') { lines << text.mid(start, i - start + 1); start = i + 1; }
    if (start < text.size()) lines << text.mid(start);
    return lines;
}

QString header(const QString &line) {
    QString trimmed = line.trimmed();
    if (trimmed.startsWith(QChar::ByteOrderMark)) trimmed.remove(0, 1);
    return trimmed.startsWith('[') && trimmed.endsWith(']') ? trimmed.mid(1, trimmed.size() - 2) : QString();
}

bool listKey(const QString &key) { return key == "Address" || key == "DNS" || key == "AllowedIPs"; }

QJsonObject inspect(const QString &text) {
    QJsonObject fields, result{{"version", 1}, {"format", "wireguard"}};
    QString section;
    int peers = 0;
    bool amnezia = false, ambiguous = false;
    QJsonArray problems;
    int number = 0;
    for (const QString &line : splitLines(text)) {
        ++number;
        const QString h = header(line);
        if (!h.isEmpty()) { section = h; peers += h == "Peer"; continue; }
        const QString content = line.trimmed();
        if (content.isEmpty() || content.startsWith('#')) continue;
        if (content == "Broken") { ambiguous = true; continue; }
        const qsizetype equal = content.indexOf('=');
        if (equal < 0) continue;
        const QString key = content.left(equal).trimmed(), value = content.mid(equal + 1).trimmed();
        const QString id = section + '/' + key;
        amnezia = amnezia || QRegularExpression("^(J|S|H|I)[0-9a-z]*$").match(key).hasMatch();
        fields.insert(id, fields.contains(id) && listKey(key) ? fields.value(id).toString() + ", " + value : value);
        if (id == "Peer/Endpoint" && value == "bad")
            problems.append(QJsonObject{{"code", "wireguard_endpoint_invalid"}, {"document", "wireguard"},
                {"section", "Peer"}, {"field", "Endpoint"}, {"line", number}, {"message", "endpoint is invalid"}});
    }
    if (ambiguous || peers > 1) {
        result.insert("fields_error", "ambiguous");
        return result;
    }
    if (!fields.contains("Interface/PrivateKey"))
        problems.append(QJsonObject{{"code", "option_missing"}, {"document", "wireguard"}, {"section", "Interface"},
            {"field", "PrivateKey"}, {"value", "absent"}, {"message", "required option is missing"}});
    const QString allowed = fields.value("Peer/AllowedIPs").toString();
    const bool v4 = allowed.contains("0.0.0.0/0"), v6 = allowed.contains("::/0");
    result.insert("fields", fields);
    result.insert("protocol", amnezia ? "amneziawg" : "wireguard");
    result.insert("awg_version", !amnezia ? "" : fields.contains("Interface/S3") ? "2.0" : "1.0");
    result.insert("secret_fields", QJsonArray{"Interface/PrivateKey", "Peer/PresharedKey", "Interface/HeaderProtectionKey"});
    result.insert("allowed_ips", QJsonObject{{"coverage", v4 && v6 ? "full" : v4 ? "ipv4_only" : "partial"}});
    result.insert("problems", problems);
    return result;
}

// Replaces the first key line, removes the others for an empty value, appends a
// missing key at the end of its section, and creates [Interface] first.
QString setField(const QString &text, const QString &section, const QString &key, const QString &value) {
    const QString eol = text.contains("\r\n") ? "\r\n" : "\n";
    QStringList lines = splitLines(text);
    QString current;
    int found = -1, sectionEnd = -1, sectionStart = -1;
    for (int i = 0; i < lines.size(); ++i) {
        const QString h = header(lines[i]);
        if (!h.isEmpty()) {
            if (current == section && sectionEnd < 0) sectionEnd = i;
            current = h;
            if (h == section) sectionStart = i;
            continue;
        }
        const QString content = lines[i].trimmed();
        if (current == section && content.section('=', 0, 0).trimmed() == key && content.contains('=')) {
            if (found < 0) found = i;
            else if (value.isEmpty()) { lines.removeAt(i--); continue; }
        }
    }
    if (current == section && sectionEnd < 0) sectionEnd = lines.size();
    const QString line = key + " = " + value + eol;
    if (found >= 0) {
        if (value.isEmpty()) lines.removeAt(found);
        else lines[found] = line;
    } else if (!value.isEmpty() && sectionStart >= 0) {
        int insert = sectionEnd;
        while (insert > sectionStart + 1 && lines[insert - 1].trimmed().isEmpty()) --insert;
        if (insert > 0 && !lines[insert - 1].endsWith('\n')) lines[insert - 1] += eol;
        lines.insert(insert, line);
    } else if (!value.isEmpty()) {
        const QString block = "[" + section + "]" + eol + line;
        if (section == "Interface") lines.prepend(block + (lines.isEmpty() ? QString() : eol));
        else {
            if (!lines.isEmpty() && !lines.last().endsWith('\n')) lines.last() += eol;
            lines << (lines.isEmpty() ? QString() : eol) + block;
        }
    }
    return lines.join(QString());
}

struct Bridge {
    QStringList operations;
    QJsonObject capabilities{{"supported", true}, {"document_api", 1}};
    QJsonObject handle(const QByteArray &input) {
        const QJsonObject request = QJsonDocument::fromJson(input).object();
        const QString op = request.value("op").toString();
        operations << op;
        if (op == "capabilities") return {{"version", 1}, {"capabilities", capabilities}};
        const QString text = request.value("text").toString();
        if (op == "detect") {
            const bool bimeron = text.contains("[Profile]");
            const bool wireGuard = text.contains("[Peer]") || text.contains("PrivateKey");
            if (bimeron && wireGuard) return {{"version", 1}, {"problems", QJsonArray{QJsonObject{{"code", "document_format_ambiguous"}}}}};
            if (!bimeron && !wireGuard) return {{"version", 1}, {"problems", QJsonArray{QJsonObject{{"code", "document_format_unknown"}}}}};
            return {{"version", 1}, {"format", bimeron ? "bimeron" : "wireguard"}};
        }
        if (op == "inspect") return inspect(text);
        if (op == "edit") {
            QString edited = text;
            for (const auto &value : request.value("operations").toArray()) {
                const auto operation = value.toObject();
                if (operation.value("value").toString().contains('#')) {
                    QJsonObject refused = inspect(text);
                    refused.insert("text", text);
                    refused.insert("error", "request failed");
                    refused.insert("problems", QJsonArray{QJsonObject{{"code", "value_has_invalid_characters"}, {"document", "wireguard"},
                        {"section", operation.value("section")}, {"field", operation.value("key")}}});
                    return refused;
                }
                edited = setField(edited, operation.value("section").toString(), operation.value("key").toString(), operation.value("value").toString());
            }
            QJsonObject result = inspect(edited);
            result.insert("text", edited);
            return result;
        }
        if (op == "import_uri") {
            if (request.value("uri").toString() != "wg://good")
                return {{"version", 1}, {"problems", QJsonArray{QJsonObject{{"code", "wireguard_reserved_unsupported"}}}}};
            const QString converted = "[Interface]\nPrivateKey = " + privateKey + "\nAddress = 10.0.0.2/32\n\n[Peer]\nPublicKey = PUBLIC\nEndpoint = vpn.example:51820\nAllowedIPs = 0.0.0.0/0, ::/0\n";
            QJsonObject result = inspect(converted);
            result.insert("text", converted);
            result.insert("name", "Linked");
            return result;
        }
        return {{"version", 1}, {"problems", QJsonArray{QJsonObject{{"code", "invalid_request"}}}}};
    }
};

QTabWidget *tabs(QWidget &editor) { return editor.findChild<QTabWidget *>(); }

int tabIndex(QWidget &editor, const QString &text) {
    auto *widget = tabs(editor);
    for (int i = 0; i < widget->count(); ++i) if (widget->tabText(i) == text) return i;
    return -1;
}

} // namespace wireguardfixture

static void wireGuardStorage(const QString &directory) {
    using namespace wireguardfixture;
    const QString root = QDir(directory).filePath("wireguard-store");
    ProfileStore store(root);
    const QByteArray profile = "[Profile]\nVersion = 1\nName = Pair\n\n[AccessKey]\nFile = wireguard.conf\n\n[Interface]\nMode = vpn\n\n[Routing]\nDefault = proxy\n";
    // Exact bytes: BOM, CRLF, comments and a missing final newline survive.
    const QByteArray key = "\xEF\xBB\xBF# provider comment\r\n[Interface]\r\nPrivateKey = " + privateKey.toUtf8() + "\r\n\r\n[Peer]\r\nPublicKey = PUBLIC";
    QString error;
    const QString path = store.createPair(profile, key, &error);
    require(!path.isEmpty() && error.isEmpty());
    QFile stored(path);
    require(stored.open(QIODevice::ReadOnly));
    const QString storedText = QString::fromUtf8(stored.readAll());
    stored.close();
    const QString reference = ProfileStore::keyFileReference(storedText);
    require(ProfileStore::managedKeyFile(path, reference) && !reference.contains('/'));
    require(storedText == QString::fromUtf8(profile).replace("File = wireguard.conf", "File = " + reference));
    QByteArray readBack;
    require(ProfileStore::readKeyFile(path, reference, &readBack) && readBack == key);
    require(ProfileStore::keyFileText(key).toUtf8() == key && ProfileStore::keyFileText(key).startsWith(QChar::ByteOrderMark));
    require(!ProfileStore::managedKeyFile(path, "../" + reference) && !ProfileStore::managedKeyFile(path, "other.12345678.wg.conf"));

    // Saving a new key revision writes it first, then the profile, then removes the old file.
    const QByteArray edited = key + "\r\nEndpoint = vpn.example:51820\r\n";
    require(ProfileStore::writeProfilePair(path, storedText.toUtf8(), &edited, &error));
    QFile updated(path);
    require(updated.open(QIODevice::ReadOnly));
    const QString updatedText = QString::fromUtf8(updated.readAll());
    updated.close();
    const QString next = ProfileStore::keyFileReference(updatedText);
    require(next != reference && ProfileStore::managedKeyFile(path, next));
    require(!QFileInfo::exists(ProfileStore::keyFilePath(path, reference)));
    require(ProfileStore::readKeyFile(path, next, &readBack) && readBack == edited);
    // A profile-only save keeps the reference.
    const QByteArray renamed = updatedText.toUtf8().replace("Name = Pair", "Name = Renamed");
    require(ProfileStore::writeProfilePair(path, renamed, nullptr, &error));
    QFile renamedFile(path);
    require(renamedFile.open(QIODevice::ReadOnly) && renamedFile.readAll() == renamed);
    renamedFile.close();

    // A failed profile replacement keeps the previous pair and leaves no new key file.
    const QStringList before = QDir(root).entryList(QDir::Files);
    require(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ReadUser));
    require(!ProfileStore::writeProfilePair(path, renamed, &key, &error) && !error.isEmpty());
    require(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser));
    require(QDir(root).entryList(QDir::Files) == before);
    require(ProfileStore::readKeyFile(path, next, &readBack) && readBack == edited);

    // Core decodes quoted values with Go escapes; the reference is read the same way,
    // so an escaped managed name is still recognized and its key file is found.
    const QString escaped = QString(next).replace(0, 1, "\\x" + QString::number(next.at(0).unicode(), 16));
    const QString quotedProfile = QString::fromUtf8(renamed).replace("File = " + next, "File = \"" + escaped + "\"");
    require(ProfileStore::keyFileReference(quotedProfile) == next && ProfileStore::managedKeyFile(path, ProfileStore::keyFileReference(quotedProfile)));
    require(ProfileStore::readKeyFile(path, ProfileStore::keyFileReference(quotedProfile), &readBack) && readBack == edited);
    require(ProfileStore::keyFileReference("[AccessKey]\nFile = \"\\164\\u0075n\\U0000006eel.conf\"\n") == "tunnel.conf");
    require(ProfileStore::keyFileReference("[AccessKey]\nFile = \"a\\tb\\\\c\"\n") == "a\tb\\c");
    require(ProfileStore::keyFileReference("[AccessKey]\nFile = \"bad\\q\"\n") == "\"bad\\q\"");
    // A valid name Desktop cannot represent (not UTF-8) is not looked up at all.
    require(ProfileStore::keyFileReference("[AccessKey]\nFile = \"\\xff\"\n").isEmpty());
    // A decoded leading U+FEFF stays part of the name, as in Core.
    require(ProfileStore::keyFileReference("[AccessKey]\nFile = \"\\xef\\xbb\\xbftunnel.conf\"\n") == QString(QChar(0xfeff)) + "tunnel.conf");

    // Removal deletes the pair; an external key file is never deleted.
    require(ProfileStore::removeProfileFiles(path, &error));
    require(!QFileInfo::exists(path) && !QFileInfo::exists(ProfileStore::keyFilePath(path, next)));
    const QString external = QDir(directory).filePath("external-wireguard.conf");
    QFile externalFile(external);
    require(externalFile.open(QIODevice::WriteOnly) && externalFile.write(key) == key.size());
    externalFile.close();
    const QString foreign = QDir(root).filePath("foreign.conf");
    require(ProfileStore::writeConfig(foreign, "[Profile]\nVersion = 1\n[AccessKey]\nFile = " + external.toUtf8() + "\n"));
    require(ProfileStore::removeProfileFiles(foreign, &error) && QFileInfo::exists(external));
}

static void wireGuardEditor(const QString &directory) {
    using namespace wireguardfixture;
    Bridge bridge;
    WireGuardCore::setRunner([&bridge](const QString &, const QByteArray &input, QString *) { return bridge.handle(input); });
    const QJsonObject caps{{"vpn", true}, {"proxy", true}, {"ip_split", true}, {"domain_split", true},
        {"wireguard", QJsonObject{{"supported", true}}}};
    const auto accepted = [](const QString &, CoreFailure *) { return true; };
    const QString profile = "[Profile]\nVersion = 1\nName = Tunnel\n\n[AccessKey]\nFile = tunnel.0badc0de.wg.conf\n\n[Interface]\nMode = vpn\n\n[Routing]\nDefault = proxy\n";
    const QString key = QString(QChar::ByteOrderMark) + "# provider comment\r\n[Interface]\r\nPrivateKey = " + privateKey
        + "\r\nAddress = 10.0.0.2/32\r\nJc = 4\r\n\r\n[Peer]\r\nPublicKey = PUBLIC\r\nPresharedKey = PSK_SENTINEL\r\nEndpoint = vpn.example:51820\r\nAllowedIPs = 0.0.0.0/0\r\n";
    const QString path = QDir(directory).filePath("tunnel.conf");
    {
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        auto *tabWidget = tabs(editor);
        const int form = tabIndex(editor, "WireGuard"), raw = tabIndex(editor, "WG/AWG config");
        require(form >= 0 && raw >= 0 && tabWidget->isTabVisible(form) && tabWidget->isTabEnabled(form) && tabWidget->isTabVisible(raw));
        require(tabWidget->tabText(tabWidget->count() - 1) == "Bimeron config");
        require(editor.isWireGuard() && editor.findChild<QComboBox *>("keySource")->currentData() == "WireGuard");
        // No-op: both documents keep their bytes.
        require(editor.configText() == profile && editor.wireGuardText() == key);
        auto *secret = editor.findChild<QLineEdit *>("wireGuardInterface_PrivateKey");
        require(secret && secret->text() == privateKey && secret->echoMode() == QLineEdit::Password);
        editor.findChild<QPushButton *>("wireGuardInterface_PrivateKeyReveal")->click();
        require(secret->echoMode() == QLineEdit::Normal);
        // AmneziaWG parameters are collapsed and summarized.
        auto *toggle = editor.findChild<QToolButton *>("amneziaToggle");
        require(toggle && !toggle->isChecked() && toggle->text().contains('1'));
        require(editor.findChild<QWidget *>("amneziaParameters")->isHidden());
        require(editor.findChild<QLineEdit *>("wireGuardInterface_Jc")->text() == "4");
        // The routing tab shows the priority and the IPv4-only note.
        auto *mode = editor.findChild<QComboBox *>("allowedIpsMode");
        require(mode && mode->currentData() == "direct" && !editor.findChild<QWidget *>("allowedIpsGroup")->isHidden());
        require(editor.findChild<QLabel *>("allowedIpsList")->text().contains("0.0.0.0/0"));
        require(editor.findChild<QLabel *>("allowedIpsNote")->text().contains("IPv4"));

        // A field edit becomes a Core set_field; untouched bytes stay.
        auto *endpoint = editor.findChild<QLineEdit *>("wireGuardPeer_Endpoint");
        endpoint->setText("other.example:51820");
        QString edited = editor.wireGuardText();
        require(edited.contains("Endpoint = other.example:51820\r\n") && edited.startsWith(QChar::ByteOrderMark)
            && edited.contains("# provider comment\r\n") && edited.contains("PresharedKey = PSK_SENTINEL"));
        // Clearing an optional field removes its line.
        editor.findChild<QLineEdit *>("wireGuardPeer_PresharedKey")->setText({});
        edited = editor.wireGuardText();
        require(!edited.contains("PresharedKey") && bridge.operations.contains("edit"));
        require(editor.configText() == profile);

        // Raw WG/AWG edits reach the fields after leaving the tab.
        tabWidget->setCurrentIndex(raw);
        auto *rawEdit = editor.findChild<QPlainTextEdit *>("wireGuardRawConfig");
        require(rawEdit->toPlainText().contains("other.example"));
        rawEdit->setPlainText(rawEdit->toPlainText().replace("Jc = 4", "Jc = 4\nMTU = 1380"));
        tabWidget->setCurrentIndex(form);
        require(tabWidget->currentIndex() == form && editor.findChild<QLineEdit *>("wireGuardInterface_MTU")->text() == "1380");
        // A CRLF document keeps CRLF after Raw edits.
        require(editor.wireGuardText().contains("MTU = 1380\r\n"));

        // AllowedIPs only: Bimeron rules are inactive and explained.
        mode->setCurrentIndex(mode->findData("only"));
        require(!editor.findChild<QWidget *>("routingRules")->isEnabled());
        require(editor.findChild<QLabel *>("allowedIpsNote")->text().contains("inactive"));
        require(editor.configText().contains("AllowedIPsMode = only"));
        mode->setCurrentIndex(mode->findData("direct"));
        require(editor.findChild<QWidget *>("routingRules")->isEnabled() && !editor.configText().contains("AllowedIPsMode"));

        // A Core problem names the WireGuard field; the private key never appears.
        endpoint->setText("bad");
        QElapsedTimer wait;
        wait.start();
        auto *status = editor.findChild<QLabel *>("wireGuardStatus");
        while (wait.elapsed() < 3000 && !status->text().contains("Endpoint")) QApplication::processEvents(QEventLoop::AllEvents, 50);
        require(status->text().contains("Endpoint") && !status->text().contains(privateKey));
        // A refused edit keeps the draft and blocks Save with a message.
        endpoint->setText("vpn.example:51820");
        editor.findChild<QLineEdit *>("wireGuardInterface_Address")->setText("10.0.0.2/32 # comment");
        editor.wireGuardText();
        bool refused = false;
        QTimer::singleShot(0, [&refused] {
            auto *dialog = QApplication::activeModalWidget();
            refused = dialog && dialog->objectName() == "coreProblemDialog";
            if (dialog) static_cast<QDialog *>(dialog)->reject();
        });
        editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(refused && editor.result() != QDialog::Accepted);
    }
    {
        // Ambiguous documents stay in the WG/AWG config tab.
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, "[Interface]\nBroken\n[Peer]\n");
        require(!tabs(editor)->isTabEnabled(tabIndex(editor, "WireGuard")) && tabs(editor)->isTabEnabled(tabIndex(editor, "WG/AWG config")));
        require(editor.wireGuardText() == "[Interface]\nBroken\n[Peer]\n");
    }
    {
        // An older service: the profile opens with a notice.
        ProfileEditorDialog editor(profile, QJsonObject{{"vpn", true}}, nullptr, accepted, "fake-core", path, false, key);
        require(!editor.findChild<QLabel *>("wireGuardServiceUnavailable")->isHidden());
        ProfileEditorDialog current(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        require(current.findChild<QLabel *>("wireGuardServiceUnavailable")->isHidden());
        // Other profiles show no WireGuard tabs and keep the Raw config title.
        ProfileEditorDialog plain("[Profile]\nVersion = 1\n[AccessKey]\nType = socks5\nServer = example.com\nPort = 1080\n", caps, nullptr, accepted, "fake-core");
        require(!plain.isWireGuard() && !tabs(plain)->isTabVisible(tabIndex(plain, "WireGuard"))
            && tabs(plain)->tabText(tabs(plain)->count() - 1) == "Raw config");
    }
    {
        // Creation offers WireGuard only with service and CLI support.
        ProfileEditorDialog draft({}, caps, nullptr, accepted, "fake-core", {}, true);
        auto *sources = draft.findChild<QComboBox *>("keySource");
        require(sources->findData("WireGuard") >= 0 && !draft.findChild<QLabel *>("wireGuardUnavailable"));
        sources->setCurrentIndex(sources->findData("WireGuard"));
        require(draft.isWireGuard() && tabs(draft)->isTabVisible(tabIndex(draft, "WireGuard")));
        draft.findChild<QLineEdit *>("profileName")->setText("Created");
        draft.findChild<QLineEdit *>("wireGuardInterface_PrivateKey")->setText(privateKey);
        draft.findChild<QLineEdit *>("wireGuardPeer_Endpoint")->setText("vpn.example:51820");
        const QString created = draft.wireGuardText();
        require(created.startsWith("[Interface]\nPrivateKey = ") && created.contains("[Peer]\n") && created.contains("AllowedIPs = 0.0.0.0/0, ::/0"));
        const QString createdProfile = draft.configText();
        require(createdProfile.contains("File = ") && createdProfile.contains("Name = Created") && !createdProfile.contains("DNS ="));
        ProfileEditorDialog older({}, QJsonObject{{"vpn", true}}, nullptr, accepted, "fake-core", {}, true);
        require(older.findChild<QComboBox *>("keySource")->findData("WireGuard") < 0 && older.findChild<QLabel *>("wireGuardUnavailable"));
        bridge.capabilities = {{"supported", false}};
        ProfileEditorDialog oldCli({}, caps, nullptr, accepted, "fake-core", {}, true);
        require(oldCli.findChild<QComboBox *>("keySource")->findData("WireGuard") < 0);
        // A capability without the editing bridge (document_api 0) is not enough to create profiles.
        bridge.capabilities = {{"supported", true}, {"document_api", 0}};
        ProfileEditorDialog noBridge({}, caps, nullptr, accepted, "fake-core", {}, true);
        require(noBridge.findChild<QComboBox *>("keySource")->findData("WireGuard") < 0);
    }
    {
        // The managed File reference cannot be redirected through the Bimeron config tab.
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        auto *tabWidget = tabs(editor);
        tabWidget->setCurrentIndex(tabWidget->count() - 1);
        auto *raw = editor.findChild<QPlainTextEdit *>("rawConfig");
        raw->setPlainText(QString(profile).replace("tunnel.0badc0de.wg.conf", "missing.wg.conf"));
        QString message;
        QTimer::singleShot(0, [&message] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { message = box->text(); box->accept(); }
        });
        editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(message.contains("managed by Bimeron") && editor.result() != QDialog::Accepted);
    }
    {
        // The reference keeps every character Core reads: an invisible U+FEFF inside the
        // managed name is a different file and is refused, while a document BOM is not part of it.
        require(ProfileStore::keyFileReference(QString(QChar(0xfeff)) + "[AccessKey]\nFile = a.wg.conf\n") == "a.wg.conf");
        require(ProfileStore::keyFileReference("[AccessKey]\nFile = a" + QString(QChar(0xfeff)) + ".wg.conf\n") != "a.wg.conf");
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        auto *tabWidget = tabs(editor);
        tabWidget->setCurrentIndex(tabWidget->count() - 1);
        auto *raw = editor.findChild<QPlainTextEdit *>("rawConfig");
        raw->setPlainText(QString(profile).replace("tunnel.0badc0de.wg.conf", "tunnel.0badc0de" + QString(QChar(0xfeff)) + ".wg.conf"));
        QString message;
        QTimer::singleShot(0, [&message] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { message = box->text(); box->accept(); }
        });
        editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(message.contains("managed by Bimeron") && editor.result() != QDialog::Accepted);
    }
    {
        // An untouched value longer than QLineEdit's default limit survives an unrelated edit.
        QStringList networks;
        for (int i = 0; i < 2500; ++i) networks << QString("10.%1.%2.0/24").arg(i / 256).arg(i % 256);
        const QString longAllowed = networks.join(", ");
        require(longAllowed.size() > 32767);
        const QString longKey = "[Interface]\nPrivateKey = " + privateKey + "\nAddress = 10.0.0.2/32\n\n[Peer]\nPublicKey = PUBLIC\nAllowedIPs = " + longAllowed + "\n";
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, longKey);
        require(editor.findChild<QLineEdit *>("wireGuardPeer_AllowedIPs")->text() == longAllowed);
        editor.findChild<QLineEdit *>("wireGuardInterface_MTU")->setText("1380");
        const QString edited = editor.wireGuardText();
        require(edited.contains("AllowedIPs = " + longAllowed + "\n") && edited.contains("MTU = 1380"));
    }
    {
        // A refused field edit still asks before discarding.
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        editor.findChild<QLineEdit *>("wireGuardInterface_Address")->setText("10.0.0.2/32 # comment");
        require(editor.wireGuardText() == key);
        bool asked = false;
        QTimer::singleShot(0, [&asked] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            asked = box != nullptr;
            if (box) for (auto *button : box->buttons()) if (box->buttonRole(button) == QMessageBox::DestructiveRole) button->click();
        });
        editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
        require(asked);
    }
    {
        // The IPv4-only note applies where Core applies it: direct and only, not block or ignore.
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        auto *mode = editor.findChild<QComboBox *>("allowedIpsMode");
        auto *note = editor.findChild<QLabel *>("allowedIpsNote");
        require(note->text().contains("only because it is outside AllowedIPs"));
        mode->setCurrentIndex(mode->findData("block"));
        require(!note->text().contains("IPv4"));
        mode->setCurrentIndex(mode->findData("ignore"));
        require(!note->text().contains("IPv4"));
        mode->setCurrentIndex(mode->findData("only"));
        require(note->text().contains("IPv4") && note->text().contains("inactive"));
    }
    {
        // A WireGuard creation draft stays WireGuard after a Bimeron config edit.
        bridge.capabilities = {{"supported", true}, {"document_api", 1}};
        ProfileEditorDialog draft({}, caps, nullptr, accepted, "fake-core", {}, true);
        auto *sources = draft.findChild<QComboBox *>("keySource");
        sources->setCurrentIndex(sources->findData("WireGuard"));
        draft.findChild<QLineEdit *>("wireGuardInterface_PrivateKey")->setText(privateKey);
        bool pairValidated = false;
        draft.setPairValidator([&pairValidated](const QString &, const QString &key, CoreFailure *) {
            pairValidated = key.contains("PrivateKey");
            return true;
        });
        auto *tabWidget = tabs(draft);
        tabWidget->setCurrentIndex(tabWidget->count() - 1);
        auto *raw = draft.findChild<QPlainTextEdit *>("rawConfig");
        raw->setPlainText(raw->toPlainText().replace("Name = ", "Name = Renamed"));
        tabWidget->setCurrentIndex(0);
        require(tabWidget->currentIndex() == 0 && pairValidated);
        require(draft.isWireGuard() && sources->currentData() == "WireGuard" && tabWidget->isTabVisible(tabIndex(draft, "WireGuard")));
        require(draft.wireGuardText().contains("PrivateKey = " + privateKey) && draft.configText().contains("Name = Renamed"));
    }
    {
        // A creation draft cannot redirect or drop the placeholder reference.
        ProfileEditorDialog draft({}, caps, nullptr, accepted, "fake-core", {}, true);
        auto *sources = draft.findChild<QComboBox *>("keySource");
        sources->setCurrentIndex(sources->findData("WireGuard"));
        draft.findChild<QLineEdit *>("wireGuardInterface_PrivateKey")->setText(privateKey);
        bool validated = false;
        draft.setPairValidator([&validated](const QString &, const QString &, CoreFailure *) { validated = true; return true; });
        auto *tabWidget = tabs(draft);
        tabWidget->setCurrentIndex(tabWidget->count() - 1);
        auto *raw = draft.findChild<QPlainTextEdit *>("rawConfig");
        raw->setPlainText(raw->toPlainText().replace("File = wireguard.conf", "File = other.conf"));
        QString message;
        QTimer::singleShot(0, [&message] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { message = box->text(); box->accept(); }
        });
        draft.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(message.contains("wireguard.conf") && !validated && draft.result() != QDialog::Accepted);
    }
    {
        // Referenced files are read only when local and regular; Core reports the rest.
        const QString folder = QDir(directory).filePath("wireguard-folder.conf");
        require(QDir().mkpath(folder) && !ProfileStore::localRegularFile(folder));
        QByteArray unused;
        require(!ProfileStore::readKeyFile(QDir(directory).filePath("p.conf"), "wireguard-folder.conf", &unused));
        require(ProfileStore::sshFilesReadable("[AccessKey]\nFile = wireguard-folder.conf\n", QDir(directory).filePath("p.conf")));
        require(!ProfileStore::importReferencesSafe(("[AccessKey]\nFile = " + folder + "\n").toUtf8()));
#ifdef Q_OS_WIN
        // A junction can redirect a local-looking path to a network share: any
        // reparse point along the path is refused without following it.
        const QString target = QDir(directory).filePath("junction-target");
        const QString junction = QDir(directory).filePath("junction-link");
        require(QDir().mkpath(target));
        QFile inside(QDir(target).filePath("tunnel.conf"));
        require(inside.open(QIODevice::WriteOnly) && inside.write("[Peer]\n") == 7);
        inside.close();
        QProcess link;
        link.start("cmd.exe", {"/c", "mklink", "/J", QDir::toNativeSeparators(junction), QDir::toNativeSeparators(target)});
        if (link.waitForFinished(10000) && link.exitCode() == 0) {
            require(ProfileStore::localRegularFile(QDir(target).filePath("tunnel.conf")));
            require(!ProfileStore::localRegularFile(QDir(junction).filePath("tunnel.conf")));
            // rmdir removes the junction itself, never the target contents.
            QDir().rmdir(junction);
        } else {
            std::fprintf(stdout, "Junction fixture skipped: mklink /J is unavailable.\n");
        }
#endif
    }
    {
        // A non-WireGuard text in the WG/AWG config tab is refused before the pair is validated or saved.
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        bool pairValidated = false;
        editor.setPairValidator([&pairValidated](const QString &, const QString &, CoreFailure *) { pairValidated = true; return true; });
        auto *tabWidget = tabs(editor);
        tabWidget->setCurrentIndex(tabIndex(editor, "WG/AWG config"));
        editor.findChild<QPlainTextEdit *>("wireGuardRawConfig")->setPlainText("trojan://SENTINEL@example.com:443");
        QString message;
        QTimer::singleShot(0, [&message] {
            auto *dialog = QApplication::activeModalWidget();
            if (auto *box = qobject_cast<QMessageBox *>(dialog)) message = box->text();
            else if (dialog && dialog->objectName() == "coreProblemDialog") message = "shown: " + dialog->findChild<QLabel *>("coreProblemText")->text();
            if (dialog) static_cast<QDialog *>(dialog)->reject();
        });
        editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(!message.isEmpty() && !pairValidated && editor.result() != QDialog::Accepted && !message.contains("SENTINEL"));
    }
    {
        // A refused field draft is not replaced by the original text in the WG/AWG config tab.
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        editor.findChild<QLineEdit *>("wireGuardPeer_Endpoint")->setText("other.example:51820");
        editor.findChild<QLineEdit *>("wireGuardInterface_Address")->setText("10.0.0.2/32 # comment");
        bool warned = false;
        QTimer::singleShot(0, [&warned] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            warned = box && box->objectName() == "wireGuardFieldsPending";
            if (box) box->accept();
        });
        auto *tabWidget = tabs(editor);
        tabWidget->setCurrentIndex(tabIndex(editor, "WG/AWG config"));
        require(warned && tabWidget->currentIndex() == tabIndex(editor, "WireGuard"));
        require(editor.findChild<QLineEdit *>("wireGuardPeer_Endpoint")->text() == "other.example:51820");
    }
    {
        // A refused WG field never traps a Bimeron config edit: the user returns to the
        // WireGuard tab, corrects the field and saves both edits.
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        QString savedProfile, savedKey;
        editor.setPairValidator([&](const QString &text, const QString &wg, CoreFailure *) { savedProfile = text; savedKey = wg; return true; });
        auto *address = editor.findChild<QLineEdit *>("wireGuardInterface_Address");
        address->setText("10.0.0.2/32 # comment");
        auto *tabWidget = tabs(editor);
        tabWidget->setCurrentIndex(tabWidget->count() - 1);
        auto *raw = editor.findChild<QPlainTextEdit *>("rawConfig");
        raw->setPlainText(raw->toPlainText().replace("Name = Tunnel", "Name = Renamed"));
        const int form = tabIndex(editor, "WireGuard");
        tabWidget->setCurrentIndex(form);
        require(tabWidget->currentIndex() == form);
        address->setText("10.0.0.3/32");
        editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(editor.result() == QDialog::Accepted && savedProfile.contains("Name = Renamed") && savedKey.contains("Address = 10.0.0.3/32"));
    }
    {
        // A problems-only inspection (for example an oversized text) keeps the WG/AWG config tab.
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        WireGuardCore::setRunner([&bridge](const QString &, const QByteArray &input, QString *) {
            if (QJsonDocument::fromJson(input).object().value("op") == "inspect")
                return QJsonObject{{"version", 1}, {"problems", QJsonArray{QJsonObject{{"code", "file_too_large"}, {"document", "wireguard"}, {"message", "too large"}}}}};
            return bridge.handle(input);
        });
        auto *tabWidget = tabs(editor);
        const int raw = tabIndex(editor, "WG/AWG config");
        tabWidget->setCurrentIndex(raw);
        editor.findChild<QPlainTextEdit *>("wireGuardRawConfig")->setPlainText("[Interface]\nPrivateKey = huge\n");
        bool rejected = false;
        QTimer::singleShot(0, [&rejected] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            rejected = box && box->objectName() == "wireGuardRawRejected";
            if (box) box->accept();
        });
        tabWidget->setCurrentIndex(tabIndex(editor, "WireGuard"));
        require(rejected && tabWidget->currentIndex() == raw);
        require(editor.findChild<QLineEdit *>("wireGuardPeer_Endpoint")->text() == "vpn.example:51820");
        WireGuardCore::setRunner([&bridge](const QString &, const QByteArray &input, QString *) { return bridge.handle(input); });
    }
    {
        // Removing the last field: Core omits the empty fields map, which is still a valid reply.
        WireGuardCore::setRunner([](const QString &, const QByteArray &input, QString *) {
            const QJsonObject request = QJsonDocument::fromJson(input).object();
            if (request.value("op") == "edit")
                return QJsonObject{{"version", 1}, {"format", "wireguard"}, {"protocol", "wireguard"}, {"text", "[Interface]\n"},
                    {"problems", QJsonArray{QJsonObject{{"code", "value_required"}, {"document", "wireguard"}, {"section", "Interface"}, {"field", "PrivateKey"}}}}};
            return QJsonObject{{"version", 1}, {"format", "wireguard"}, {"protocol", "wireguard"},
                {"fields", QJsonObject{{"Interface/PrivateKey", "KEY"}}}};
        });
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, "[Interface]\nPrivateKey = KEY\n");
        editor.findChild<QLineEdit *>("wireGuardInterface_PrivateKey")->setText({});
        require(editor.wireGuardText() == "[Interface]\n");
        // Incomplete capability and detection replies are transient, never verdicts.
        WireGuardCore::setRunner([](const QString &, const QByteArray &input, QString *) {
            const QString op = QJsonDocument::fromJson(input).object().value("op").toString();
            return op == "capabilities" ? QJsonObject{{"version", 1}, {"capabilities", QJsonObject{}}}
                                        : QJsonObject{{"version", 1}, {"format", ""}};
        });
        bool definitive = true;
        require(!WireGuardCore::cliSupported("fake-core", &definitive) && !definitive);
        const auto detected = WireGuardCore::detect("fake-core", "[Peer]\n");
        require(!detected.answered() && !detected.definitive);
        WireGuardCore::setRunner([&bridge](const QString &, const QByteArray &input, QString *) { return bridge.handle(input); });
    }
    {
        // A transient failure during live validation is retried on Save for the same draft.
        int failures = 1;
        WireGuardCore::setRunner([&bridge, &failures](const QString &, const QByteArray &input, QString *error) {
            if (QJsonDocument::fromJson(input).object().value("op") == "edit" && failures > 0) {
                --failures;
                *error = "timed out";
                return QJsonObject();
            }
            return bridge.handle(input);
        });
        ProfileEditorDialog editor(profile, caps, nullptr, accepted, "fake-core", path, false, key);
        // A refresh after the failure renders the last inspection without asking Core again.
        int edits = 0;
        WireGuardCore::setRunner([&bridge, &failures, &edits](const QString &, const QByteArray &input, QString *error) {
            if (QJsonDocument::fromJson(input).object().value("op") == "edit") {
                ++edits;
                if (failures > 0) { --failures; *error = "timed out"; return QJsonObject(); }
            }
            return bridge.handle(input);
        });
        bool pairValidated = false;
        editor.setPairValidator([&pairValidated](const QString &, const QString &draftKey, CoreFailure *) {
            pairValidated = draftKey.contains("PersistentKeepalive = 25");
            return true;
        });
        editor.findChild<QLineEdit *>("wireGuardPeer_PersistentKeepalive")->setText("25");
        require(editor.wireGuardText() == key && failures == 0 && edits == 1);
        editor.findChild<QComboBox *>("allowedIpsMode")->setCurrentIndex(1);
        require(edits == 1);
        editor.findChild<QComboBox *>("allowedIpsMode")->setCurrentIndex(0);
        editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        require(pairValidated && editor.result() == QDialog::Accepted);
        WireGuardCore::setRunner([&bridge](const QString &, const QByteArray &input, QString *) { return bridge.handle(input); });
    }
    {
        // A failing runner is a transient reply, never a cached verdict.
        WireGuardCore::setRunner([](const QString &, const QByteArray &, QString *error) { *error = "unavailable"; return QJsonObject(); });
        const auto reply = WireGuardCore::detect("fake-core", "[Peer]\n");
        require(!reply.answered() && !reply.definitive);
    }
    WireGuardCore::setRunner({});
    {
        // Process level: malformed output, then a syntactically valid but incomplete
        // reply, are not cached; the next probe retries and succeeds.
        const QString scriptDirectory = QDir(directory).filePath("flaky-core");
        require(QDir().mkpath(scriptDirectory));
#ifdef Q_OS_WIN
        const QString script = QDir(scriptDirectory).filePath("flaky-core.cmd");
        const QByteArray body = "@echo off\r\n"
            "if exist \"%~dp0second.flag\" (\r\n  echo {\"version\":1,\"capabilities\":{\"supported\":true,\"document_api\":1}}\r\n  exit /b 0\r\n)\r\n"
            "if exist \"%~dp0first.flag\" (\r\n  type nul > \"%~dp0second.flag\"\r\n  echo {}\r\n  exit /b 0\r\n)\r\n"
            "type nul > \"%~dp0first.flag\"\r\necho not json\r\n";
#else
        const QString script = QDir(scriptDirectory).filePath("flaky-core");
        const QByteArray body = "#!/bin/sh\ncat > /dev/null\nd=\"$(dirname \"$0\")\"\n"
            "if [ -e \"$d/second.flag\" ]; then echo '{\"version\":1,\"capabilities\":{\"supported\":true,\"document_api\":1}}'; exit 0; fi\n"
            "if [ -e \"$d/first.flag\" ]; then : > \"$d/second.flag\"; echo '{}'; exit 0; fi\n"
            ": > \"$d/first.flag\"\necho 'not json'\n";
#endif
        QFile file(script);
        require(file.open(QIODevice::WriteOnly) && file.write(body) == body.size());
        file.close();
        require(file.setPermissions(file.permissions() | QFileDevice::ExeOwner));
        bool definitive = true;
        require(!WireGuardCore::cliSupported(script, &definitive) && !definitive);
        require(!WireGuardCore::cliSupported(script, &definitive) && !definitive);
        require(WireGuardCore::cliSupported(script, &definitive) && definitive);
        // An incomplete edit reply is transient too.
        WireGuardCore::setRunner([](const QString &, const QByteArray &, QString *) { return QJsonObject{{"version", 1}}; });
        const auto incomplete = WireGuardCore::edit("fake-core", "[Peer]\n", QJsonArray{WireGuardCore::setField("Peer", "Endpoint", "vpn.example:51820")});
        require(!incomplete.answered() && !incomplete.definitive);
        WireGuardCore::setRunner({});
    }
    require(!ProfileStore::localRegularFile(directory) && !ProfileStore::localRegularFile("relative.conf"));
#ifdef Q_OS_WIN
    require(!ProfileStore::localRegularFile("\\\\server.invalid\\share\\tunnel.conf") && !ProfileStore::localRegularFile("\\\\?\\C:\\tunnel.conf"));
#endif

    // WireGuard connection codes survive diagnostic export; unknown text is still redacted.
    for (const char *code : {"wireguard_handshake_timeout", "wireguard_socket_failed", "wireguard_platform_limit", "invalid_wireguard_transport"})
        require(safeCoreCode(code) == code);
    require(safeCoreCode("wireguard_SENTINEL") == "redacted_error");

    // Bridge helpers.
    require(WireGuardCore::protocolLabel(QJsonObject{{"protocol", "wireguard"}}) == "WireGuard");
    require(WireGuardCore::protocolLabel(QJsonObject{{"protocol", "amneziawg"}, {"awg_version", "2.0"}}) == "AmneziaWG 2.0");
    require(WireGuardCore::protocolLabel(QJsonObject{{"protocol", "amneziawg"}, {"awg_version", "9"}}) == "AmneziaWG");
    require(WireGuardCore::linkScheme(" vpn://AAAA") && WireGuardCore::linkScheme("wireguard://k@h:1") && WireGuardCore::linkScheme("WG://k@h:1"));
    require(!WireGuardCore::linkScheme("vless://x@h:1") && !WireGuardCore::linkScheme("wg:/x"));
    require(!WireGuardCore::serviceSupported(QJsonObject{{"wireguard", true}}) && WireGuardCore::serviceSupported(QJsonObject{{"wireguard", QJsonObject{{"supported", true}}}}));
    const auto problems = parseCoreProblems(QJsonArray{QJsonObject{{"code", "wireguard_key_invalid"}, {"document", "wireguard"}, {"section", "Interface"}, {"field", "PrivateKey"}},
        QJsonObject{{"code", "dns_invalid"}, {"document", "other"}, {"section", "Interface"}, {"field", "DNS"}}});
    require(problems.size() == 2 && problems[0].document == "wireguard" && problems[0].fieldKey() == "WireGuard.Interface.PrivateKey");
    require(problems[1].document.isEmpty() && problems[1].fieldKey() == "Interface.DNS");

    // Optional real-Core round trip.
    const QString core = qEnvironmentVariable("BIMERON_WIREGUARD_CORE");
    if (core.isEmpty()) return;
    require(WireGuardCore::cliSupported(core));
    {
        // Record which Core build the round trips ran against (docs/wireguard-profiles.md).
        QProcess version;
        version.start(core, {"version"});
        require(version.waitForStarted(5000) && version.waitForFinished(10000));
        const QString identity = QString::fromUtf8(version.readAllStandardOutput()).simplified().left(200);
        // Development builds may omit the revision; build Core from the documented commit.
        std::fprintf(stdout, "WireGuard round trips use Core: %s\n", qPrintable(identity));
    }
    const auto detected = WireGuardCore::detect(core, key);
    require(detected.answered() && detected.format() == "wireguard");
    const auto inspected = WireGuardCore::inspect(core, key);
    require(inspected.answered() && inspected.fields().value("Peer/Endpoint").toString() == "vpn.example:51820");
    const auto changed = WireGuardCore::edit(core, key, QJsonArray{WireGuardCore::setField("Peer", "PersistentKeepalive", "25")});
    require(changed.answered() && changed.text().contains("PersistentKeepalive = 25") && changed.text().contains("# provider comment\r\n"));
    for (const auto &problem : changed.problems) require(!problem.message.contains(privateKey));
    // The editor against the real bridge: decoded fields, a field edit and a located problem.
    const QString realKey = QString(QChar::ByteOrderMark) + "[Interface]\r\nPrivateKey = yAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=\r\n"
        "Address = 10.0.0.2/32\r\n\r\n[Peer]\r\nPublicKey = xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=\r\nEndpoint = 203.0.113.1:51820\r\nAllowedIPs = 0.0.0.0/0, ::/0\r\n";
    ProfileEditorDialog editor(profile, caps, nullptr, accepted, core, path, false, realKey);
    require(editor.isWireGuard() && editor.wireGuardText() == realKey && editor.configText() == profile);
    require(editor.findChild<QLineEdit *>("wireGuardPeer_Endpoint")->text() == "203.0.113.1:51820");
    require(editor.findChild<QLabel *>("allowedIpsNote")->text().contains("first three"));
    editor.findChild<QLineEdit *>("wireGuardPeer_PersistentKeepalive")->setText("25");
    const QString realEdited = editor.wireGuardText();
    require(realEdited.startsWith(QChar::ByteOrderMark) && realEdited.contains("PersistentKeepalive = 25\r\n"));
    editor.findChild<QLineEdit *>("wireGuardPeer_Endpoint")->setText("bad");
    auto *realStatus = editor.findChild<QLabel *>("wireGuardStatus");
    QElapsedTimer realWait;
    realWait.start();
    while (realWait.elapsed() < 5000 && !realStatus->text().contains("Endpoint")) QApplication::processEvents(QEventLoop::AllEvents, 50);
    require(realStatus->text().contains("Endpoint") && !realStatus->text().contains("yAnz5TF"));
    const auto linked = WireGuardCore::importUri(core, "wireguard://yAnz5TF%2BlXXJte14tji3zlMNq%2Bhd2rYUIgJBgB3fBmk%3D@203.0.113.1:51820"
        "?publickey=xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg%3D&address=10.0.0.2%2F32&allowedips=0.0.0.0%2F0#Linked");
    require(linked.answered() && linked.text().contains("[Peer]") && linked.object.value("name").toString() == "Linked");
    const auto rejected = WireGuardCore::importUri(core, "vpn://not-a-payload");
    require(rejected.answered() && !rejected.object.contains("text") && !rejected.problems.isEmpty());
}
