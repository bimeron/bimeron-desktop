#pragma once

#include <QListWidget>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QPointer>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QMenu>
#include "NativeIKEv2SessionTest.h"

// Compile the application translator itself; never invoke its renamed entry point.
#define main bimeronApplicationEntryForTranslationTest
#include "../src/main.cpp"
#undef main

// Uses only temporary profiles/settings, a private pipe/socket and this test's
// validate subcommand. Never reaches a system endpoint or privileged helper.
struct ConnectionManagementTest {
    static int preview(QApplication &app, const QString &destination, const QString &language) {
        class Translator final : public QTranslator {
        public:
            QJsonObject strings;
            QString translate(const char *, const char *source, const char *, int) const override {
                return strings.value(QString::fromUtf8(source)).toString();
            }
            bool isEmpty() const override { return strings.isEmpty(); }
        } translator;
        if (language == "ru") {
            QFile translations(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json"));
            require(translations.open(QIODevice::ReadOnly), "preview translations");
            translator.strings = QJsonDocument::fromJson(translations.readAll()).object();
            app.installTranslator(&translator);
        }
        MainWindow window;
        if (app.arguments().contains("--ssh-preview")) {
            window.state_ = "connecting";
            window.handleSSHAuthentication({{"id", "preview"}, {"kind", "keyboard_interactive"},
                {"prompts", QJsonArray{"Account password", "One-time authentication code"}}, {"echo", QJsonArray{false, false}}});
            if (app.arguments().contains("--ssh-eight")) {
                window.clearSSHAuthentication();
                QJsonArray prompts, echo;
                for (int i = 0; i < 8; ++i) {
                    prompts.append(QString("Untrusted server prompt %1: ").arg(i + 1) + QString(220, 'x'));
                    echo.append(false);
                }
                window.handleSSHAuthentication({{"id", "eight"}, {"kind", "keyboard_interactive"},
                    {"prompts", prompts}, {"echo", echo}});
            }
            QCoreApplication::processEvents();
            require(window.sshDialog_ && window.sshDialog_->grab().save(destination), "SSH preview image");
            return 0;
        }
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        QTemporaryDir directory;
        const QString path = directory.filePath("preview.conf");
        QFile profile(path);
        require(profile.open(QIODevice::WriteOnly), "preview profile");
        if (app.arguments().contains("--native-preview")) {
            profile.write("[Profile]\nName = Native\n[IKEv2]\nWindowsProfile = Fixture\nServerName = vpn.invalid\n[Interface]\nMode = vpn\n");
            profile.close();
            window.profiles_.append({"native-preview", "Native IKEv2", path});
            window.profileList_->addItem("Native IKEv2");
            window.profileList_->setCurrentRow(0);
            window.handleStatus({{"State", "disconnected"}});
            window.updateProfileAppearance();
            window.updateConnectionButton();
            NativeIKEv2SessionTest::Fixture nativePreview;
            if (app.arguments().contains("--native-bound-preview")) {
                nativePreview.discover();
                nativePreview.observe(nativePreview.sessionReply("connected", true));
                window.bindNativeController(&nativePreview.controller);
                nativePreview.controller.cancelExchange();
            }
            window.show();
            QCoreApplication::processEvents();
            require(window.grab().save(destination), "native unavailable preview image");
            return 0;
        }
        profile.write("[Profile]\nName = Example proxy\n[AccessKey]\nSOCKS5 = socks5://localhost:1080\n[Interface]\nMode = proxy\n");
        profile.close();
        window.profiles_.append({"preview", "Example proxy", path});
        window.profileList_->addItem("Example proxy");
        window.profileList_->setCurrentRow(0);
        window.handleStatus({{"State", "connected"}, {"Session", QJsonObject{
            {"id", "preview"}, {"profile_id", "preview"}, {"active", true}, {"mode", "proxy"}}}});
        window.show();
        QCoreApplication::processEvents();
        auto *unknown = new QListWidgetItem("Unknown example\nUnknown protocol", window.profileList_);
        unknown->setData(Qt::UserRole + 1, true);
        unknown->setSizeHint(QSize(0, 50));
        unknown->setBackground(Qt::white);
        window.profileList_->setCurrentItem(unknown);
        QCoreApplication::processEvents();
        require(window.grab().save(destination), "connection preview image");
        return 0;
    }

    static int quitFixture(QApplication &app, bool stop) {
        QLocalServer server;
        require(server.listen("bimeron-exit-" + QUuid::createUuid().toString()), "exit fixture endpoint");
        MainWindow window;
        window.core_.setEndpoint(server.serverName());
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        window.instance_ = "exit";
        int requests = 0;
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (!socket->canReadLine()) return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                require(request.value("command").toString() == "shutdown", "explicit exit policy command");
                ++requests;
                socket->write(QJsonDocument(QJsonObject{{"version", 1}, {"id", request.value("id")},
                    {"instance", "exit"}, {"status", QJsonObject{{"State", "disconnected"}}}}).toJson(QJsonDocument::Compact) + '\n');
            });
        });
        if (window.tray_ && window.tray_->isVisible()) {
            QCloseEvent close;
            window.closeEvent(&close);
            require(!close.isAccepted() && !window.quitRequested_ && requests == 0, "close to tray must leave Core alone");
        }
        QSettings().setValue("app/stopCoreOnExit", stop);
        QTimer::singleShot(0, &window, [&] { window.requestShutdown(); });
        QTimer::singleShot(3000, &window, [] { require(false, "explicit quit must complete"); });
        app.exec();
        require(requests == (stop ? 1 : 0), "exit must honor only the explicit stop policy");
        return 0;
    }

    static void transport() {
        QLocalServer server;
        require(server.listen("bimeron-framing-" + QUuid::createUuid().toString()), "framing fixture");
        CoreClient client;
        client.setEndpoint(server.serverName());
        int replies = 0, failures = 0, received = 0;
        int mode = 0;
        CoreClient::Failure reason = CoreClient::Failure::Transport;
        QObject::connect(&client, &CoreClient::replied, &client, [&](const QString &, const QJsonObject &) { ++replies; });
        QObject::connect(&client, &CoreClient::failed, &client, [&](const QString &, CoreClient::Failure value, const QString &) {
            reason = value; ++failures;
        });
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (!socket->canReadLine()) return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                ++received;
                QByteArray payload;
                if (mode == 1) payload = "not JSON\n";
                else if (mode == 2) payload = QByteArray(256 * 1024 + 1, 'x');
                else payload = QJsonDocument(QJsonObject{{"version", mode == 3 ? 2 : 1},
                    {"id", mode == 4 ? QJsonValue("wrong") : request.value("id")}}).toJson(QJsonDocument::Compact) + '\n';
                if (mode == 0) {
                    socket->write(payload.left(3));
                    QTimer::singleShot(10, socket, [socket, payload] { socket->write(payload.mid(3)); });
                } else socket->write(payload);
            });
        });
        require(client.request("hello"), "first request accepted");
        require(!client.request("status"), "busy client must reject conflicting request explicitly");
        require(waitFor([&] { return replies == 1; }) && received == 1, "fragmented response must be assembled");
        const CoreClient::Failure expected[]{CoreClient::Failure::InvalidResponse, CoreClient::Failure::TooLarge,
            CoreClient::Failure::Incompatible, CoreClient::Failure::InvalidResponse};
        for (mode = 1; mode <= 4; ++mode) {
            const int previous = failures;
            client.request("hello");
            require(waitFor([&] { return failures > previous; }), "invalid response must fail");
            require(reason == expected[mode - 1], "structured failure must identify the cause");
        }
        const int previous = received;
        require(!client.request("start", {{"config", QString(17000, 'x')}}), "oversized request must be rejected locally");
        require(reason == CoreClient::Failure::TooLarge && received == previous, "oversized request must never reach Core");
    }

    static void sshAuthentication() {
        MainWindow window;
        window.state_ = "connecting";
        const auto challenge = [](const QString &id, const QString &kind = "keyboard_interactive") {
            return QJsonObject{{"id", id}, {"kind", kind}, {"prompts", QJsonArray{"<b>OTP</b>", "Password"}},
                {"echo", QJsonArray{true, false}}};
        };
        window.handleSSHAuthentication(challenge("one"));
        require(window.sshDialog_, "SSH dialog must be nonmodal");
        auto inputs = window.sshDialog_->findChildren<QLineEdit *>();
        auto promptViews = window.sshDialog_->findChildren<QPlainTextEdit *>();
        require(promptViews.size() == 2 && promptViews[0]->isReadOnly()
            && promptViews[0]->toPlainText() == "<b>OTP</b>", "server prompt remains literal text");
        require(inputs.size() == 2 && inputs[0]->echoMode() == QLineEdit::Normal
            && inputs[1]->echoMode() == QLineEdit::Password, "SSH echo behavior");
        for (auto *label : window.sshDialog_->findChildren<QLabel *>())
            require(label->textFormat() == Qt::PlainText, "SSH prompts are untrusted plain text");
        inputs[0]->setText("OTP_SENTINEL");
        inputs[1]->setText("PASSWORD_SENTINEL");
        auto *buttons = window.sshDialog_->findChild<QDialogButtonBox *>();
        buttons->button(QDialogButtonBox::Ok)->click();
        require(window.sshReplyReady_ && window.sshAnswers_.size() == 2, "SSH queues one reply");
        require(inputs[0]->text().isEmpty() && inputs[1]->text().isEmpty(), "SSH input cleared on submit");
        window.clearSSHAuthentication();
        require(window.sshAnswers_.isEmpty() && !window.sshReplyReady_, "SSH cancellation discards pending reply");
        window.handleSSHAuthentication(challenge("one"));
        require(!window.sshDialog_, "SSH one-use challenge cannot reopen");
        window.handleSSHAuthentication(challenge("two", "password"));
        require(window.sshDialog_->findChildren<QLineEdit *>()[0]->echoMode() == QLineEdit::Password,
            "password remains masked despite untrusted echo");
        window.sshDialog_->reject();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        window.handleSSHAuthentication(challenge("two", "password"));
        require(!window.sshDialog_, "canceled challenge cannot reopen");
        auto bad = challenge("bad");
        bad.insert("echo", QJsonArray{false});
        window.handleSSHAuthentication(bad);
        require(!window.sshDialog_, "malformed SSH challenge rejected");
        bad = challenge("oversized");
        bad.insert("prompts", QJsonArray{QString(257, 'x'), "second"});
        window.handleSSHAuthentication(bad);
        require(!window.sshDialog_, "oversized SSH prompt rejected");
        window.handleSSHAuthentication(challenge("unicode"));
        auto unicodeInputs = window.sshDialog_->findChildren<QLineEdit *>();
        unicodeInputs[0]->setText(QString(600, QChar(0x044f)));
        window.sshDialog_->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        require(!window.sshReplyReady_ && window.sshDialog_, "SSH answer bound counts UTF-8 bytes");
        require(!window.sshDialog_->findChild<QLabel *>("sshAnswerError")->isHidden(), "SSH explains oversized answers safely");
        int heartbeat = 0;
        QTimer::singleShot(0, &window, [&] { ++heartbeat; });
        require(waitFor([&] { return heartbeat == 1; }), "SSH prompt leaves UI responsive");
        window.clearSSHAuthentication();
        auto many = challenge("many");
        QJsonArray eightPrompts, eightEchoes;
        for (int i = 0; i < 8; ++i) { eightPrompts.append("Prompt"); eightEchoes.append(false); }
        many.insert("prompts", eightPrompts);
        many.insert("echo", eightEchoes);
        window.handleSSHAuthentication(many);
        require(window.sshDialog_ && window.sshAuthExpiry_->interval() == 120000, "eight prompts and two-minute expiry");
        require(window.sshDialog_->findChild<QScrollArea *>(), "many prompts use a bounded scroll area");
        for (auto *input : window.sshDialog_->findChildren<QLineEdit *>()) input->setText(QString(1024, QChar(1)));
        window.sshDialog_->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        require(!window.sshReplyReady_, "serialized JSON request bound includes escaping");
        QMetaObject::invokeMethod(window.sshAuthExpiry_, "timeout", Qt::DirectConnection);
        require(!window.sshDialog_ && window.sshAnswers_.isEmpty(), "expiry closes and clears challenge");
        window.handleSSHAuthentication(many);
        require(!window.sshDialog_, "expired challenge cannot reopen");
        many.insert("id", "nine");
        eightPrompts.append("Ninth"); eightEchoes.append(false);
        many.insert("prompts", eightPrompts); many.insert("echo", eightEchoes);
        window.handleSSHAuthentication(many);
        require(!window.sshDialog_, "ninth SSH prompt rejected");
        window.handleSSHAuthentication(challenge("stopped"));
        window.handleStatus({{"State", "stopping"}});
        require(!window.sshDialog_ && window.sshAnswers_.isEmpty(), "stop closes SSH prompt");
        window.state_ = "connecting";
        QLocalServer server;
        require(server.listen("bimeron-ssh-" + QUuid::createUuid().toString()), "SSH private endpoint");
        window.core_.setEndpoint(server.serverName());
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        window.instance_ = "ssh-fixture";
        window.sessionId_ = "ssh-session";
        window.nextStatus_ = window.nextEvents_ = window.nextSSHAuth_ = window.pollClock_.elapsed() + 60000;
        int sent = 0;
        QString expectedCommand = "auth_reply";
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (!socket->canReadLine()) return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                require(request.value("command") == expectedCommand, "SSH uses existing correlated control transport");
                if (expectedCommand == "auth_reply") {
                    require(request.value("challenge_id") == "wire", "SSH replies to current ID");
                    require(request.value("answers").toArray().size() == 2, "SSH one answer per prompt");
                }
                ++sent;
                socket->write(QJsonDocument(QJsonObject{{"version", 1}, {"id", request.value("id")},
                    {"instance", "ssh-fixture"}, {"status", QJsonObject{{"State", "connecting"},
                        {"Session", QJsonObject{{"id", "ssh-session"}, {"active", true}}}}}}).toJson(QJsonDocument::Compact) + '\n');
            });
        });
        window.handleSSHAuthentication(challenge("wire"));
        auto *wireDialog = window.sshDialog_.data();
        for (auto *input : wireDialog->findChildren<QLineEdit *>()) input->setText("WIRE_SENTINEL");
        wireDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        window.poll();
        require(waitFor([&] { return sent == 1 && !window.core_.busy(); }), "SSH reply completes asynchronously");
        window.poll();
        require(sent == 1 && window.sshAnswers_.isEmpty(), "SSH reply is not retried or cached");
        require(!window.log_->toPlainText().contains("WIRE_SENTINEL"), "SSH answers absent from logs");
        expectedCommand = "auth_pending";
        window.nextSSHAuth_ = -2;
        window.nextStatus_ = window.nextEvents_ = -1;
        window.poll();
        require(waitFor([&] { return sent == 2 && !window.core_.busy(); }), "overdue status/events cannot starve auth polling");
        window.nextStatus_ = window.nextEvents_ = window.nextSSHAuth_ = window.pollClock_.elapsed() + 60000;
        window.handleSSHAuthentication(challenge("session-change"));
        window.handleStatus({{"State", "connecting"}, {"Session", QJsonObject{{"id", "different-session"}}}});
        require(!window.sshDialog_, "session change closes old prompt");
        window.handleSSHAuthentication(challenge("failure"));
        window.handleReply("auth_reply", {{"instance", "ssh-fixture"}, {"error", "invalid_auth_challenge"},
            {"status", QJsonObject{{"State", "connecting"}, {"Session", QJsonObject{{"id", "different-session"}}}}}});
        require(!window.sshDialog_ && window.statusNeeded_ && !window.sshReplyReady_, "invalid challenge resynchronizes without replay");
        window.handleReply("auth_pending", {{"instance", "ssh-fixture"}, {"error", "unknown_command"},
            {"status", QJsonObject{{"State", "connecting"}, {"Session", QJsonObject{{"id", "ssh-session"}}}}}});
        require(!window.sshAuthSupported_, "old Core disables interactive polling");
        window.handleSSHAuthentication(challenge("three", "key_passphrase"));
        window.invalidateConnection();
        require(!window.sshDialog_ && window.sshAnswers_.isEmpty(), "backend invalidation closes SSH prompt");
    }

    static void run() {
        {
            MainWindow window;
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                require(dialog, "Add dialog opened");
                const QSize original = dialog->size();
                const QList<QLineEdit *> fields{dialog->findChild<QLineEdit *>("profileUri"), dialog->findChild<QLineEdit *>("profileName")};
                require(fields.size() == 2, "URI and name fields");
                QPushButton *paste = nullptr;
                for (auto *button : dialog->findChildren<QPushButton *>()) {
                    if (button->text() == "Paste") paste = button;
                }
                require(paste, "Paste control");
                require(fields[1]->isHidden() && fields[0]->echoMode() == QLineEdit::Normal, "Name initially hidden and URI visible");
                const QString previous = QApplication::clipboard()->text();
                QApplication::clipboard()->setText("unsupported clipboard");
                QCoreApplication::processEvents();
                require(!paste->isVisible(), "Unsupported clipboard hides Paste");
                fields[0]->setText("typed text");
                QCoreApplication::processEvents();
                require(dialog->size() == original, "Typing retains geometry");
                QApplication::clipboard()->setText("https://example.invalid:443");
                QCoreApplication::processEvents();
                require(paste->isEnabled() && paste->isVisible(), "Supported clipboard shows Paste");
                paste->click();
                QCoreApplication::processEvents();
                require(dialog->width() == original.width() && dialog->height() > original.height(), "Valid URI reveals name without width change");
                require(fields[1]->isVisible() && fields[1]->y() < fields[0]->y(), "Name appears above URI");
                fields[0]->setText("ssh://alice@example.invalid");
                QCoreApplication::processEvents();
                require(fields[1]->isVisible() && dialog->width() == original.width(), "SSH retains name and dialog width");
                require(dialog->findChildren<QLineEdit *>().size() == 4, "SSH identity and known hosts retained");
                QApplication::clipboard()->setText(previous);
                dialog->reject();
            });
            window.addProfile();
        }

        transport();
        sshAuthentication();
        for (const QString policy : {QStringLiteral("keep"), QStringLiteral("stop")}) {
            QProcess process;
            process.start(QCoreApplication::applicationFilePath(), {"--visual-check", "--connection-quit", policy});
            require(process.waitForFinished(5000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
                    "isolated exit-policy fixture");
        }
        QTemporaryDir directory;
        require(directory.isValid(), "connection fixture directory");
        QLocalServer server;
        require(server.listen("bimeron-connection-" + QUuid::createUuid().toString()), "connection fixture endpoint");
        MainWindow window;
        window.core_.setEndpoint(server.serverName());
        window.validationExecutable_ = QCoreApplication::applicationFilePath();
        const QByteArray config("[Profile]\nName = Test\n[AccessKey]\nSOCKS5 = socks5://localhost:1080\n[Interface]\nMode = proxy\n");
        const auto write = [](const QString &path, const QByteArray &bytes) {
            QFile file(path);
            require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "write connection fixture");
            require(file.write(bytes) == bytes.size(), "complete connection fixture");
        };
        for (const QString name : {QStringLiteral("A"), QStringLiteral("B")}) {
            const QString path = directory.filePath(name + ".conf");
            write(path, config);
            window.profiles_.append({ProfileStore::identity(path), name, path});
            window.profileList_->addItem(name);
        }
        window.profileList_->setCurrentRow(0);
        QSettings().setValue("app/autoConnect", true);
        QSettings().setValue("app/autoConnectProfile", window.profiles_[1].id);
        QString instance = "first";
        QString version = "0.2.0";
        QJsonObject status{{"State", "connected"}, {"Session", QJsonObject{
            {"id", "existing"}, {"active", true}, {"profile_id", window.profiles_[0].id}, {"mode", "proxy"}}}};
        int starts = 0, stops = 0, requests = 0;
        qint64 bytes = 100;
        bool holdStart = false, holdStop = false, dropped = false, invalidCursor = false, failStop = false;
        QPointer<QLocalSocket> stopSocket;
        QJsonObject stopRequest;
        const auto respond = [&](QLocalSocket *socket, const QJsonObject &request, QJsonObject extra = {}) {
            QJsonObject reply{{"version", 1}, {"id", request.value("id")}, {"instance", instance},
                              {"status", status}, {"core", QJsonObject{{"version", version}}},
                              {"capabilities", QJsonObject{{"vpn", true}, {"proxy", true}, {"ipv6", true}}}};
            for (auto it = extra.begin(); it != extra.end(); ++it) reply.insert(it.key(), it.value());
            socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
            socket->flush();
        };
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (!socket->canReadLine()) return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                ++requests;
                const QString command = request.value("command").toString();
                QJsonObject extra;
                if (command == "start") {
                    ++starts;
                    status = {{"State", "connecting"}, {"Session", QJsonObject{
                        {"id", QString::number(starts)}, {"active", true}, {"mode", "proxy"},
                        {"profile_id", ProfileStore::identity(request.value("config").toString())}}}};
                    if (holdStart) return;
                } else if (command == "stop") {
                    ++stops;
                    if (failStop) {
                        status = {{"State", "failed"}, {"LastError", "cleanup_failed_run_recover"}};
                        respond(socket, request, {{"error", "session_stop_failed"}});
                        return;
                    }
                    if (holdStop) {
                        status.insert("State", "stopping");
                        stopSocket = socket;
                        stopRequest = request;
                        return;
                    }
                    auto session = status.value("Session").toObject();
                    session.insert("active", false);
                    status = {{"State", "disconnected"}, {"Session", session}};
                } else if (command == "events") {
                    extra = {{"cursor", 12}, {"dropped", dropped}};
                    if (invalidCursor) extra.insert("error", "invalid_cursor");
                } else if (command == "stats") {
                    extra.insert("stats", QJsonObject{{"bytes_up", bytes}, {"bytes_down", bytes}});
                }
                require(command != "shutdown" && command != "recover", "ordinary controls must not shutdown or recover");
                respond(socket, request, extra);
            });
        });
        const auto synchronized = [&] { return window.helloReady_ && !window.statusNeeded_ && !window.core_.busy(); };
        const auto request = [&](const QString &command) {
            require(!window.core_.busy(), "fixture request serialization");
            require(window.core_.request(command), "fixture request accepted");
            require(waitFor([&] { return !window.core_.busy(); }), "fixture response");
        };
        window.poll();
        require(waitFor(synchronized), "initial hello and status");
        require(window.state_ == "connected" && window.activeProfileId_ == window.profiles_[0].id,
                "startup must adopt authoritative session");
        require(starts == 0 && window.autoConnectAttempted_, "startup adoption must not start again");
        require(window.activeProxyMode(), "runtime session mode must be authoritative");
        require(window.connectButton_->isEnabled(), "active profile must allow disconnect");

        holdStop = true;
        window.profileList_->setCurrentRow(1);
        window.toggleConnection();
        window.toggleConnection();
        require(waitFor([&] { return stops == 1; }), "switch must send one stop");
        require(starts == 0 && window.operation_ == "stop", "switch must wait for cleanup");
        int heartbeats = 0;
        QTimer heartbeat;
        heartbeat.setInterval(5);
        QObject::connect(&heartbeat, &QTimer::timeout, &heartbeat, [&] { ++heartbeats; });
        heartbeat.start();
        QElapsedTimer delay;
        delay.start();
        require(waitFor([&] { return delay.elapsed() > 70; }), "delayed stop UI interval");
        require(heartbeats > 3 && starts == 0, "pending stop must leave UI responsive without starting");
        auto session = status.value("Session").toObject();
        session.insert("active", false);
        status = {{"State", "disconnected"}, {"Session", session}};
        respond(stopSocket, stopRequest);
        require(waitFor([&] { return starts == 1 && window.operation_.isEmpty() && !window.statusNeeded_ && !window.core_.busy(); }),
                "successful cleanup must start target exactly once");
        require(window.state_ == "connecting" && window.activeProfileId_ == window.profiles_[1].id,
                "start acceptance is connecting, not connected");

        status.insert("State", "connected");
        request("status");
        const int requestBaseline = requests;
        QTimer polling;
        polling.setInterval(10);
        QObject::connect(&polling, &QTimer::timeout, &polling, [&] { window.poll(); });
        polling.start();
        delay.restart();
        require(waitFor([&] { return delay.elapsed() > 1100; }), "polling observation interval");
        polling.stop();
        require(waitFor([&] { return !window.core_.busy(); }), "polling drain");
        require(requests - requestBaseline <= 8, "frequent ticks must coalesce background work without a request storm");
        request("stats");
        require(window.statsClock_.isValid(), "stats must establish monotonic baseline");
        bytes = 10;
        request("stats");
        require(!window.trafficLabel_->text().contains("/s"), "counter decrease must reset rate");
        auto changed = status.value("Session").toObject();
        changed.insert("id", "replacement");
        status.insert("Session", changed);
        request("stats");
        require(!window.trafficLabel_->text().contains("/s"), "session change in stats must reset rate");
        dropped = true;
        request("events");
        require(window.statusNeeded_, "dropped history must request authoritative status");
        require(waitFor(synchronized), "dropped history resync");
        dropped = false;
        invalidCursor = true;
        request("events");
        require(window.eventCursor_ == 0 && window.statusNeeded_, "invalid cursor must reset and resync");
        require(waitFor(synchronized), "cursor resync");
        invalidCursor = false;

        // Restart can be observed in stats, not only events.
        window.pendingStartPath_ = window.profiles_[0].path;
        const quint64 generation = window.connectionGeneration_;
        instance = "second";
        request("stats");
        require(window.pendingStartPath_.isEmpty() && window.connectionGeneration_ > generation, "instance change cancels old intentions");
        require(waitFor(synchronized), "restart handshake");
        require(starts == 1, "restart must not restore a profile automatically");

        // Accepted start with a lost reply must be adopted, never retried.
        holdStop = false;
        window.toggleConnection();
        require(waitFor([&] { return window.cleanDisconnected() && window.operation_.isEmpty() && !window.core_.busy(); }), "ordinary stop");
        holdStart = true;
        window.operation_ = "start";
        require(window.core_.request("start", {{"config", window.profiles_[0].path}}, 30), "ambiguous start request");
        require(waitFor([&] { return starts == 2 && !window.helloReady_; }), "ambiguous start timeout");
        require(window.connectionLabel_->text() == "Core unavailable", "IPC loss is not disconnected");
        window.retryAt_ = 0;
        window.poll();
        require(waitFor(synchronized), "ambiguous start status reconciliation");
        require(starts == 2 && window.state_ == "connecting", "accepted timed-out start must not be repeated");
        holdStart = false;

        // A stop timeout must discard switching, including after later cleanup.
        window.profileList_->setCurrentRow(1);
        window.pendingStartPath_ = window.profiles_[1].path;
        window.operation_ = "stop";
        holdStop = true;
        require(window.core_.request("stop", {}, 30), "stop timeout request");
        require(waitFor([&] { return !window.helloReady_; }), "stop timeout");
        require(window.pendingStartPath_.isEmpty(), "stop timeout must cancel target");
        status = {{"State", "disconnected"}};
        window.retryAt_ = 0;
        window.poll();
        require(waitFor(synchronized) && starts == 2, "later cleanup must not resurrect canceled start");

        // Cleanup failure cancels switching and never starts/recover automatically.
        bool cleanupDialog = false;
        QTimer dialogCloser;
        dialogCloser.setInterval(10);
        QObject::connect(&dialogCloser, &QTimer::timeout, &dialogCloser, [&] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                cleanupDialog = true;
                box->accept();
            }
        });
        dialogCloser.start();
        failStop = true;
        window.pendingStartPath_ = window.profiles_[1].path;
        window.operation_ = "stop";
        request("stop");
        require(cleanupDialog && window.pendingStartPath_.isEmpty() && window.cleanupBlocked_ && starts == 2,
                "failed stop must report cleanup and cancel switching");
        dialogCloser.stop();
        failStop = false;
        require(waitFor(synchronized), "failed stop status resync");
        // Cleanup failure remains visible even when the GUI did not initiate it.
        status = {{"State", "failed"}, {"LastError", "cleanup_failed_run_recover"}};
        request("status");
        window.toggleConnection();
        require(window.cleanupBlocked_ && window.operation_.isEmpty() && starts == 2,
                "cleanup failure must block a new session");
        require(window.connectionLabel_->text().contains("failed", Qt::CaseInsensitive), "failed is not disconnected");
        status = {{"State", "connected"}, {"Session", QJsonObject{{"id", "native"}, {"active", true}, {"backend", "ikev2"}}}};
        request("status");
        require(!window.connectButton_->isEnabled(), "native IKEv2 must not expose packet control");

        // Independent idle fixture for asynchronous validation and startup policies.
        status = {{"State", "disconnected"}};
        window.cleanupBlocked_ = false;
        request("status");
        window.profileList_->setCurrentRow(0);
        write(window.profiles_[0].path, config + "# fixture-reject\n");
        window.toggleConnection();
        require(waitFor([&] { return window.operation_.isEmpty(); }), "rejected validation must complete");
        require(starts == 2, "rejected profile must not reach start");
        write(window.profiles_[0].path, config + "# fixture-delay\n");
        window.toggleConnection();
        require(window.operation_ == "validate", "validation must be asynchronous");
        window.toggleConnection();
        const int heartbeatBefore = heartbeats;
        write(window.profiles_[0].path, config);
        require(waitFor([&] { return window.operation_.isEmpty(); }), "changed profile validation must complete");
        require(starts == 2 && heartbeats > heartbeatBefore, "changed profile must cancel start without blocking UI");
        write(window.profiles_[0].path, config + "# fixture-delay\n");
        window.toggleConnection();
        window.invalidateConnection();
        write(window.profiles_[0].path, config);
        window.poll();
        require(waitFor(synchronized), "backend invalidation must resynchronize");
        require(starts == 2 && !window.validationProcess_, "stale CLI must not start after invalidation");

        QSettings().setValue("app/autoConnectProfile", "removed-profile");
        window.autoConnectAttempted_ = false;
        request("status");
        require(starts == 2 && window.autoConnectAttempted_, "missing startup profile must not choose a replacement");
        QSettings().setValue("app/autoConnectProfile", window.profiles_[1].id);
        window.autoConnectAttempted_ = false;
        request("status");
        require(waitFor([&] { return starts == 3 && window.operation_.isEmpty() && !window.statusNeeded_ && !window.core_.busy(); }),
                "opt-in auto-connect must start once after synchronized idle status");
        request("hello");
        require(waitFor(synchronized) && starts == 3, "repeated handshake must not auto-connect again");

        status = {{"State", "disconnected"}};
        version = "0.3.0";
        instance = "updated";
        window.coreUpdateAwaitingResult_ = true;
        window.expectedCoreVersionAfterUpdate_ = version;
        window.reconnectProfileAfterUpdate_ = window.profiles_[0].id;
        window.invalidateConnection();
        window.poll();
        require(waitFor([&] { return starts == 4 && window.operation_.isEmpty() && !window.statusNeeded_ && !window.core_.busy(); }),
                "verified update must restore prior profile once");
        require(!window.coreUpdateAwaitingResult_, "update verification must finish");
        status = {{"State", "disconnected"}};
        window.coreUpdateAwaitingResult_ = true;
        window.expectedCoreVersionAfterUpdate_ = "9.9.9";
        window.reconnectProfileAfterUpdate_ = window.profiles_[0].id;
        window.invalidateConnection();
        window.poll();
        require(waitFor(synchronized) && starts == 4, "mismatched update must not reconnect");

        write(window.profiles_[0].path, "[Profile]\nName = Unsupported\n[AccessKey]\nType = future\n");
        window.coreUpdateAwaitingResult_ = true;
        window.expectedCoreVersionAfterUpdate_ = version;
        window.reconnectProfileAfterUpdate_ = window.profiles_[0].id;
        window.invalidateConnection();
        window.poll();
        require(waitFor(synchronized) && starts == 4 && window.reconnectResultProfileId_.isEmpty(),
                "unconnectable update target must finish with cancellation, not hang reconnecting");

        // Native profiles remain displayable but can never use packet lifecycle paths.
        const QString nativePath = directory.filePath("native.conf");
        write(nativePath, "[Profile]\nName = Native\n[IKEv2]\nWindowsProfile = Fixture\nServerName = vpn.invalid\n[Interface]\nMode = vpn\n");
        const Profile native{ProfileStore::identity(nativePath), "Native", nativePath};
        window.profiles_.append(native);
        window.store_ = ProfileStore(directory.path());
        require(window.store_.save(window.profiles_), "save isolated native selector fixture");
        window.refreshProfiles();
        const int nativeRow = window.profiles_.size() - 1;
        require(window.profileInfo(nativeRow).backend == ProfileConnectionInfo::Backend::NativeIKEv2,
                "native backend discriminator");
        require(window.autoConnectProfile_->findData(native.id) < 0, "native excluded from startup selector");
        require(!window.profileList_->item(nativeRow)->data(Qt::UserRole + 1).toBool(),
                "native unavailable label is not replaced by unknown protocol");
        window.profileList_->setCurrentRow(nativeRow);
        window.updateConnectionButton();
        require(!window.connectButton_->isEnabled() && window.nativeStatusLabel_->text().contains("approval"),
                "native action disabled with approval explanation");
        const auto settingsKeys = QSettings().allKeys();
        const auto startupProfile = QSettings().value("app/autoConnectProfile");
        window.toggleConnection();
        require(!window.startProfile(nativeRow) && window.operation_.isEmpty(), "native never starts packet validation");
        require(QSettings().allKeys() == settingsKeys && QSettings().value("app/autoConnectProfile") == startupProfile,
                "blocked native route does not mutate settings");
        require(waitFor(synchronized), "native dispatch fixture synchronized");
        window.startReadyPath_ = nativePath;
        window.operation_ = "start";
        window.poll();
        require(window.operation_.isEmpty() && starts == 4, "stale dispatch cannot send native daemon start");
        QSettings().setValue("app/autoConnectProfile", native.id);
        QSettings().setValue("app/autoConnect", true);
        window.autoConnectAttempted_ = false;
        window.completeSynchronization();
        require(window.autoConnectAttempted_ && starts == 4 && window.operation_.isEmpty(), "stale native autostart is ignored");
        window.coreUpdateAwaitingResult_ = true;
        window.expectedCoreVersionAfterUpdate_ = version;
        window.reconnectProfileAfterUpdate_ = native.id;
        window.completeSynchronization();
        require(!window.coreUpdateAwaitingResult_ && window.reconnectResultProfileId_.isEmpty() && starts == 4,
                "native never restored by packet updater");
        // Selection must not strand the authoritative packet session. Neither an
        // unsupported packet profile nor native selection is a switch target.
        holdStop = false;
        for (const int selectedRow : {0, nativeRow}) {
            for (const QString activeState : {QStringLiteral("connected"), QStringLiteral("connecting"), QStringLiteral("reconnecting")}) {
                status = {{"State", activeState}, {"Session", QJsonObject{
                    {"id", "packet-selection"}, {"active", true}, {"profile_id", window.profiles_[1].id}, {"mode", "proxy"}}}};
                request("status");
                window.profileList_->setCurrentRow(selectedRow);
                window.updateConnectionButton();
                require(window.connectButton_->isEnabled() && window.connectButton_->text() == "Disconnect",
                        "unavailable selection must allow authoritative packet disconnect");
                const int previousStops = stops;
                window.connectButton_->click();
                require(!window.connectButton_->isEnabled(), "pending packet stop disables duplicate action");
                window.toggleConnection();
                require(waitFor([&] { return stops == previousStops + 1 && synchronized() && window.operation_.isEmpty(); }),
                        "unavailable selection sends exactly one packet stop");
                require(starts == 4 && window.pendingStartPath_.isEmpty() && !window.nativeSession_,
                        "disconnect never starts native or switches backend");
            }
        }
        for (const QString blockedState : {QStringLiteral("stopping"), QStringLiteral("unknown")}) {
            status = {{"State", blockedState}, {"Session", QJsonObject{{"id", "packet-selection"}, {"active", true}}}};
            request("status");
            const int previousStops = stops;
            require(!window.connectButton_->isEnabled(), "ambiguous or stopping state disables disconnect");
            window.toggleConnection();
            require(stops == previousStops && window.operation_.isEmpty(), "blocked state cannot duplicate stop");
        }
        status = {{"State", "disconnected"}};
        window.retryAt_ = 0;
        window.poll();
        require(waitFor(synchronized), "unknown state resynchronizes before polling test");

        // Prove the update guard independently, before the permanent native block.
        require(!window.nativeOwnerBlocked_ && synchronized(), "polling fixture has no native owner block");
        window.nextStatus_ = 0;
        const int liveBefore = requests;
        window.poll();
        require(waitFor([&] { return requests > liveBefore && synchronized(); }), "regular polling is live before update");
        window.nextStatus_ = 0;
        const int suspendedBefore = requests;
        window.coreUpdateCommand_ = true;
        window.poll();
        require(requests == suspendedBefore && !window.core_.busy(), "service update alone suspends polling");
        window.coreUpdateCommand_ = false;
        window.poll();
        require(waitFor([&] { return requests > suspendedBefore && synchronized(); }), "regular polling resumes after update");

        const int beforeUpdateRefresh = requests;
        window.refreshCoreAfterUpdate();
        require(waitFor([&] { return requests > beforeUpdateRefresh && synchronized(); }),
                "unbound update refresh still synchronizes the packet backend");
        if (!window.developmentStatus_) window.developmentPage()->setParent(&window);
        const quint64 beforeDevelopmentChange = window.connectionGeneration_;
        window.development_->select("relative-invalid-core");
        require(window.connectionGeneration_ > beforeDevelopmentChange
                && window.connectionLabel_->text() == "Core unavailable"
                && window.developmentStatus_->text() == window.development_->status()
                && !window.development_->busy() && window.development_->failed(),
                "unbound Development failure still resets packet state and updates controls");
        window.core_.setEndpoint(server.serverName());

        window.connectionFailure("hello", CoreClient::Failure::AccessDenied, "Access to Core was denied");
        require(window.connectionLabel_->text() == "Access to Core was denied", "access denial must be distinct");
        window.connectionFailure("hello", CoreClient::Failure::Incompatible, "incompatible");
        require(window.connectionLabel_->text().contains("incompatible"), "incompatible IPC must be distinct");
        require(!window.nativeController_, "ordinary packet window never constructs a native controller");
        auto nativeFixture = std::make_unique<NativeIKEv2SessionTest::Fixture>();
        nativeFixture->discover();
        nativeFixture->observe(nativeFixture->sessionReply("connected", true));
        bool queuedRefreshDelivered = false, queuedDevelopmentDelivered = false;
        const auto queueLegacyCallbacks = [&] {
            queuedRefreshDelivered = queuedDevelopmentDelivered = false;
            QTimer::singleShot(0, &window, [&] {
                window.refreshCoreAfterUpdate();
                queuedRefreshDelivered = true;
            });
            QTimer::singleShot(0, &window, [&] {
                emit window.development_->changed();
                queuedDevelopmentDelivered = true;
            });
        };
        const auto checkLegacyCallbacks = [&] {
            const QString presentation = window.connectionLabel_->text();
            const quint64 generation = window.connectionGeneration_;
            const int previousRequests = requests;
            const auto previousCalls = nativeFixture ? nativeFixture->calls.size() : 0;
            if (!window.trayMenu_) window.trayMenu_ = new QMenu(&window);
            window.rebuildTrayMenu();
            const QString trayStatus = window.trayStatusAction_->text();
            window.developmentStatus_->setText("stale development status");
            require(waitFor([&] { return queuedRefreshDelivered && queuedDevelopmentDelivered; }),
                    "queued legacy callbacks delivered after native authority transition");
            require(window.nativeOwnerBlocked_ && window.connectionLabel_->text() == presentation
                    && window.nativeStatusLabel_->text() == presentation
                    && window.trayStatusAction_->text() == trayStatus
                    && window.connectionGeneration_ == generation,
                    "queued legacy callbacks preserve native presentation tray and authority");
            require(!window.core_.busy() && requests == previousRequests
                    && (!nativeFixture || nativeFixture->calls.size() == previousCalls),
                    "queued legacy callbacks send no daemon hello or native activation");
            require(window.developmentStatus_->text() == window.development_->status()
                    && window.developmentStopOrphan_->isHidden() == !window.development_->orphanAvailable(),
                    "native authority preserves Development status and cleanup controls");
        };
        queueLegacyCallbacks();
        window.bindNativeController(&nativeFixture->controller);
        checkLegacyCallbacks();
        require(window.nativeOwnerBlocked_ && !window.nativeSession_
                && window.nativeStatusLabel_->text().contains("Active"),
                "initial real controller snapshot binds independently of daemon flag");
        nativeFixture->observe(nativeFixture->sessionReply("stopping", true));
        require(window.nativeStatusLabel_->text().contains("Inactive"), "changed signal propagates stopping snapshot");
        auto retained = nativeFixture->sessionReply("stopping", true);
        auto ns = retained["status"].toObject();
        auto protection = ns["native"].toObject();
        protection["wfp"] = "retained"; protection["dns"] = "modified";
        ns["native"] = protection; retained["status"] = ns;
        nativeFixture->observe(retained);
        require(window.nativeStatusLabel_->text().contains("Retained")
                && window.nativeStatusLabel_->text().contains("Modified"), "retained signal reaches native UI");
        JsonTranslator applicationTranslator;
        require(applicationTranslator.load(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json")),
                "load actual Russian translator");
        QCoreApplication::installTranslator(&applicationTranslator);
        nativeFixture->observe(retained);
        require(!window.nativeStatusLabel_->text().contains("WFP: Retained"), "controller signal uses Russian translation");
        QCoreApplication::removeTranslator(&applicationTranslator);
        nativeFixture->observe(retained);
        const QString retainedText = window.nativeStatusLabel_->text();
        window.handleStatus({{"State", "disconnected"}});
        window.refreshProfiles();
        window.trayMenu_ = new QMenu(&window);
        window.rebuildTrayMenu();
        require(window.nativeStatusLabel_->text() == retainedText
                && window.connectionLabel_->text() == retainedText
                && window.trayStatusAction_->text().contains(retainedText),
                "packet replies profile refresh and tray rebuild preserve native uncertainty");
        const auto nativeCalls = nativeFixture->calls.size();
        QCloseEvent close;
        window.closeEvent(&close);
        window.trayMenu_->actions().last()->trigger();
        require(!window.quitRequested_ && nativeFixture->calls.size() == nativeCalls,
                "close and tray quit never stop or close native owner");
        queueLegacyCallbacks();
        nativeFixture.reset();
        checkLegacyCallbacks();
        require(!window.nativeController_ && window.nativeOwnerBlocked_
                && window.nativeStatusLabel_->text().contains("Unknown"),
                "controller destruction leaves guarded sticky unknown");
        require(!window.runServiceCommand({"service", "update"}, "fixture") && !window.coreUpdatePrerequisite(),
                "native unknown ownership blocks service mutation");
        const int nativeRequests = requests;
        QSettings().setValue("app/autoConnect", true);
        QSettings().setValue("app/autoConnectProfile", window.profiles_.first().id);
        window.autoConnectAttempted_ = false;
        window.completeSynchronization();
        window.restartApplication();
        require(!window.quitRequested_ && !window.canActivateProfile(0), "settings and restart cannot bypass native block");
        window.poll();
        window.toggleConnection();
        require(requests == nativeRequests && starts == 4 && !window.connectButton_->isEnabled(),
                "native ownership blocks polling and packet activation");

        window.core_.cancel();
        QSettings().clear();
    }
};
