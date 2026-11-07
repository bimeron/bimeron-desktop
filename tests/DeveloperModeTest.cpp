#include "CoreClient.h"
#include "RepairBootstrap.h"
#include "RepairBootstrap_p.h"
#include <QLocalSocket>
#include "DevelopmentCore.h"
#include "MainWindow.h"
#include "QtPlatformPreflight.h"
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QLabel>
#include <QTextEdit>
#include <QMessageBox>
#include <QJsonArray>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocale>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <cstdio>
#include <cstring>
#include <memory>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

static void require(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAILED: %s\n", message);
        fflush(stderr);
        qFatal("%s", message);
    }
}

static bool waitFor(const std::function<bool()> &condition, int timeout = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout) {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    return condition();
}

#include "ConnectionManagementTest.h"
#include "NativeIKEv2TransportTest.h"
#include "NativeIKEv2SessionTest.h"

struct DevelopmentRecoveryTest {
    static void brokerFinalization(DevelopmentCore &backend) {
        const QString journal = DevelopmentCore::recoveryJournalPath();
        QFile original(journal);
        require(original.open(QIODevice::ReadOnly), "broker journal fixture exists");
        const QByteArray record = original.readAll();
        original.close();
        int completed = 0;
        const auto connection = QObject::connect(&backend, &DevelopmentCore::completed,
            &backend, [&](bool) { ++completed; });
        backend.busy_ = true;
        require(!backend.brokerRestoredSystem() && QFile::exists(journal)
            && backend.interruptedRecovery(), "busy finalization preserves recovery gate and journal");
        backend.busy_ = false;
        require(backend.brokerRestoredSystem() && !backend.failed() && !backend.local()
            && !backend.interruptedRecovery() && !QFile::exists(journal),
            "authenticated broker completion finalizes stopped development state");
        require(completed == 0, "broker finalization never triggers legacy restoration or elevation");
        QObject::disconnect(connection);
        require(original.open(QIODevice::WriteOnly) && original.write(record) == record.size(),
            "restore journal fixture for existing reconciliation tests");
    }
    static void typedInspection(const QString &helper) {
        using Block = DevelopmentCore::StartBlock;
        for (const Block reason : {Block::LifecycleConflict, Block::EndpointOccupied,
                Block::EndpointAccessDenied, Block::EndpointUnknown, Block::LockOccupied,
                Block::LockAccessDenied, Block::LockUnknown, Block::CleanupUnconfirmed, Block::TimedOut}) {
            require(!DevelopmentCore::StartInspection{reason, {}}.allowed(),
                    "every unresolved inspection result blocks service start");
        }
        QLocalServer server;
        const QString endpoint = "typed-inspection-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        require(server.listen(endpoint), "private inspection endpoint");
        DevelopmentCore backend(helper);
        bool completed = false;
        backend.inspectSystemStartAtEndpoint(endpoint, [&](DevelopmentCore::StartInspection result) {
            require(result.reason == Block::EndpointOccupied && !result.allowed(),
                    "responding unknown endpoint is conflict, not cleanup evidence");
            require(!result.diagnostic.isEmpty(), "typed conflict retains evidence");
            completed = true;
        });
        require(!completed, "inspection never calls completion synchronously");
        require(waitFor([&] { return completed; }), "private occupied endpoint completes bounded inspection");
        bool lateCallback = false;
        auto *cancelled = new DevelopmentCore(helper);
        cancelled->inspectSystemStartAtEndpoint(endpoint, [&](DevelopmentCore::StartInspection) { lateCallback = true; });
        delete cancelled;
        QCoreApplication::processEvents();
        require(!lateCallback, "destroying inspection owner cancels pending callback");
    }

    static void healthyService(DevelopmentCore &backend, bool changed) {
        QLocalServer server;
        const QString endpoint = "recovery-healthy-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        require(server.listen(endpoint), "healthy recovery service fixture");
        const auto commands = std::make_shared<QStringList>();
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&server, commands] {
            auto *socket = server.nextPendingConnection();
            if (!socket) return;
            socket->setParent(&server);
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, &server, [commands, socket] {
                if (!socket->canReadLine()) return;
                const QJsonObject request = QJsonDocument::fromJson(socket->readLine()).object();
                commands->append(request.value("command").toString());
                const QJsonObject response{{"version", 1}, {"id", request.value("id")},
                    {"instance", "healthy-system-service"}, {"core", QJsonObject{{"version", "test"}}},
                    {"status", QJsonObject{{"State", "connected"}}}};
                socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
                socket->flush();
            });
        });
        const auto query = [changed](std::function<void(qint64)> result) {
            result(QCoreApplication::applicationPid() + (changed ? 1 : 0));
        };
        backend.busy_ = true;
        backend.reconcileLinuxService(QCoreApplication::applicationPid(), endpoint, query);
        require(waitFor([&] { return !backend.busy(); }), "healthy service recovery completes");
        require(*commands == QStringList{"hello", "status"}, "reconciliation sends no stop/start/recover commands");
        require(backend.interruptedRecovery() == changed && backend.failed() == changed,
                "only stable live service identity clears recovery gate");
        require(QFile::exists(DevelopmentCore::recoveryJournalPath()) == changed,
                "changed service keeps journal; healthy stable service clears it");
    }
    static void blockedWithoutCleanup(DevelopmentCore &backend) {
        backend.busy_ = true;
        backend.restoreService_ = true;
        backend.phase_ = DevelopmentCore::Phase::Verifying;
        backend.exitPhase_ = DevelopmentCore::Phase::Idle;
        backend.verifiedLock(true);
        require(backend.failed() && !backend.busy() && backend.interruptedRecovery(),
                "absent daemon and free lock do not prove interrupted cleanup");
        require(QFile::exists(DevelopmentCore::recoveryJournalPath()), "unconfirmed cleanup retains journal");
        require(backend.status().contains("unconfirmed"), "unconfirmed cleanup explains emergency recovery");
    }
    static void peerMismatch(DevelopmentCore &backend) {
        QLocalServer server;
        const QString endpoint = "recovery-peer-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        require(server.listen(endpoint), "recovery peer fixture");
        const auto accepted = std::make_shared<bool>(false);
        const auto received = std::make_shared<QByteArray>();
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&server, accepted, received] {
            *accepted = true;
            auto *socket = server.nextPendingConnection();
            if (!socket) return; // Windows may remove an already disconnected pipe.
            socket->setParent(&server);
            *received += socket->readAll();
            QObject::connect(socket, &QLocalSocket::readyRead, &server, [socket, received] {
                *received += socket->readAll();
            });
        });
        backend.busy_ = true;
        backend.reconcileLinuxService(QCoreApplication::applicationPid() + 1, endpoint);
        require(waitFor([&] { return !backend.busy(); }), "mismatched service peer terminates inspection");
        require(backend.failed() && backend.interruptedRecovery(), "foreign peer cannot clear interrupted journal");
        require(QFile::exists(DevelopmentCore::recoveryJournalPath()), "foreign peer retains journal");
        require(waitFor([&] { return *accepted; }), "inspection reached fixture endpoint");
        require(received->isEmpty(), "no hello or mutation sent to mismatched service peer");
    }
};

static void testRouting() {
    QLocalServer first;
    QLocalServer second;
    require(first.listen("bimeron-test-" + QUuid::createUuid().toString()), "first server");
    require(second.listen("bimeron-test-" + QUuid::createUuid().toString()), "second server");
    int firstRequests = 0;
    int secondRequests = 0;
    const auto serve = [](QLocalServer &server, int &requests) {
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&server, &requests] {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, &requests] {
                if (!socket->canReadLine()) return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                ++requests;
                socket->write(QJsonDocument(QJsonObject{{"version", 1}, {"id", request.value("id")}}).toJson(QJsonDocument::Compact) + '\n');
            });
        });
    };
    serve(first, firstRequests);
    serve(second, secondRequests);
    CoreClient client;
    int replies = 0;
    int errors = 0;
    QObject::connect(&client, &CoreClient::replied, &client, [&] { ++replies; });
    QObject::connect(&client, &CoreClient::unavailable, &client, [&] { ++errors; });
    client.setEndpoint(first.serverName(), QCoreApplication::applicationPid());
    client.request("hello");
    require(waitFor([&] { return replies == 1; }), "matching owner must receive hello");
    client.setEndpoint(second.serverName(), QCoreApplication::applicationPid() + 1000000);
    client.request("hello");
    require(waitFor([&] { return errors == 1; }), "wrong owner must be rejected");
    require(secondRequests == 0, "wrong owner must not receive a command");
    client.setEndpoint(first.serverName());
    client.request("hello", {}, 50);
    client.setEndpoint(second.serverName(), QCoreApplication::applicationPid());
    client.request("hello");
    require(waitFor([&] { return replies == 2; }), "switch must route to new endpoint");
    require(firstRequests >= 1 && firstRequests <= 2 && secondRequests == 1, "new request must reach only the new endpoint");
    QElapsedTimer elapsed;
    elapsed.start();
    waitFor([&] { return elapsed.elapsed() > 80; });
    require(errors == 1, "old timeout must not report an error after switching");
}

int main(int argc, char **argv) {

#ifdef Q_OS_WIN
    if (argc >= 2 && std::strcmp(argv[1], "--bootstrap-pipe-child") == 0) {
        QCoreApplication app(argc, argv);
        QLocalServer server;
        server.setSocketOptions(QLocalServer::UserAccessOption);
        require(server.listen(QString::fromLocal8Bit(argv[2])), "private pipe listen");
        QTimer::singleShot(30000, &app, &QCoreApplication::quit);
        return app.exec();
    }
    if (argc == 2 && std::strcmp(argv[1], "--bootstrap-auth-success") == 0) {
        QCoreApplication app(argc, argv);
        const QString exe = app.applicationFilePath();
        const QString pipe = "bootstrap-auth-" + QUuid::createUuid().toString(QUuid::Id128);
        auto spawn = [&](const QString &name) {
            QString command = '"' + QDir::toNativeSeparators(exe) + "\" --bootstrap-pipe-child " + name;
            STARTUPINFOW startup{}; startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            require(CreateProcessW(nullptr, reinterpret_cast<wchar_t *>(command.data()), nullptr, nullptr,
                FALSE, 0, nullptr, nullptr, &startup, &process), "spawn harmless same-image child");
            CloseHandle(process.hThread);
            return process;
        };
        auto process = spawn(pipe);
        auto unrelated = spawn(pipe + "-other");
        auto *socket = new QLocalSocket;
        require(waitFor([&] {
            if (socket->state() == QLocalSocket::ConnectedState) return true;
            socket->abort(); socket->connectToServer(pipe);
            return socket->waitForConnected(100);
        }, 10000), "connect private pipe");
        ULONG peer = 0;
        require(GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(socket->socketDescriptor()), &peer)
            && peer == process.dwProcessId, "real pipe server PID");
        const auto trust = [&](const QString &path) {
            return QDir::cleanPath(path).compare(QDir::cleanPath(exe), Qt::CaseInsensitive) == 0;
        };
        const auto parent = reinterpret_cast<quintptr>(GetCurrentProcess());
        const quintptr owned = RepairBootstrap::detail::authenticatedChild(parent, peer, exe, trust);
        require(owned != 0, "authenticate live child");
        require(!RepairBootstrap::detail::authenticatedChild(reinterpret_cast<quintptr>(unrelated.hProcess), peer, exe, trust),
            "unrelated parent rejected");
        require(!RepairBootstrap::detail::authenticatedChild(parent, peer, exe, [](const QString &) { return false; }),
            "wrong trusted image rejected");
        QTemporaryDir temporary;
        QFile wrong(temporary.filePath("wrong-image"));
        require(wrong.open(QIODevice::WriteOnly), "wrong payload");
        wrong.write("wrong"); wrong.close();
        require(!RepairBootstrap::detail::authenticatedChild(parent, peer, wrong.fileName(), trust), "wrong image bytes rejected");
        CloseHandle(process.hProcess);
        HANDLE handle = reinterpret_cast<HANDLE>(owned);
        require(GetProcessId(handle) == peer && WaitForSingleObject(handle, 0) == WAIT_TIMEOUT,
            "owned handle survives launching handle close");
        RepairBootstrap::bindOwnedChild(socket, owned);
        HANDLE terminator = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, peer);
        require(terminator && TerminateProcess(terminator, 0) && WaitForSingleObject(terminator, 5000) == WAIT_OBJECT_0,
            "stop harmless child");
        require(!RepairBootstrap::detail::authenticatedChild(parent, peer, exe, trust), "exited child rejected");
        CloseHandle(terminator);
        delete socket;
        DWORD flags = 0;
        require(!GetHandleInformation(handle, &flags) && GetLastError() == ERROR_INVALID_HANDLE,
            "socket destruction closes owned handle");
        TerminateProcess(unrelated.hProcess, 0);
        WaitForSingleObject(unrelated.hProcess, 5000);
        CloseHandle(unrelated.hProcess);
        puts("PASS bootstrap authentication and handle ownership");
        return 0;
    }
#endif
    if (argc == 2 && std::strcmp(argv[1], "--bootstrap-auth-rejection") == 0) {
        QCoreApplication app(argc, argv);
        require(RepairBootstrap::authenticatedChild(0, 0) == 0, "null parent rejected");
#ifdef Q_OS_WIN
        DWORD before = 0, after = 0;
        // Initialize Qt resource/security lookup caches before measuring per-call leaks.
        require(RepairBootstrap::authenticatedChild(reinterpret_cast<quintptr>(GetCurrentProcess()),
            GetCurrentProcessId()) == 0, "initial self-process authentication rejected");
        require(GetProcessHandleCount(GetCurrentProcess(), &before), "read initial handle count");
        for (int i = 0; i < 256; ++i)
            require(RepairBootstrap::authenticatedChild(reinterpret_cast<quintptr>(GetCurrentProcess()),
                GetCurrentProcessId()) == 0, "self cannot authenticate as bootstrap child");
        require(GetProcessHandleCount(GetCurrentProcess(), &after), "read final handle count");
        require(after == before, "rejected authentication leaks no native handles");
#endif
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--repair-probe-fixture") == 0) {
        if (std::strcmp(argv[2], "failed") == 0) { std::fprintf(stderr, "password=secret-probe-fixture"); return 7; }
        if (std::strcmp(argv[2], "missing") == 0) { std::fprintf(stderr, "service not installed"); return 1; }
        if (std::strcmp(argv[2], "stopped") == 0) { std::printf("Service state: 1\n"); return 0; }
        if (std::strcmp(argv[2], "starting") == 0) { std::printf("Service state: 2\n"); return 0; }
        if (std::strcmp(argv[2], "stopping") == 0) { std::printf("Service state: 3\n"); return 0; }
        std::printf("%s", std::strcmp(argv[2], "valid") == 0 ? "Service state: 4\n" : "invalid: 4\n");
        return 0;
    }

    if (argc == 3 && std::strcmp(argv[1], "validate") == 0) {
        QFile file(QString::fromLocal8Bit(argv[2]));
        if (!file.open(QIODevice::ReadOnly)) return 1;
        const QByteArray text = file.readAll();
        if (text.contains("fixture-delay")) QThread::msleep(200);
        return text.contains("fixture-reject") ? 1 : 0;
    }
    // The fixture CLI behaves like a Core without the error catalog
    // (`errors --json`), so the GUI keeps its legacy validation flow.
    if (argc == 3 && std::strcmp(argv[1], "errors") == 0) return 1;
    if (argc == 2 && std::strcmp(argv[1], "version") == 0) {
        printf("integration-fixture\n");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "daemon") == 0) return 1;
    if (argc == 3 && std::strcmp(argv[1], "service") == 0) {
        QFile state("/run/bimeron-desktop-integration-service");
        if (!state.open(QIODevice::ReadOnly)) return 1;
        const QByteArray current = state.readAll();
        state.close();
        if (std::strcmp(argv[2], "status") == 0) {
            if (current == "absent") { fprintf(stderr, "service not installed\n"); return 1; }
            printf("service: %s\n", current.constData());
            return 0;
        }
        if (QFile::exists(QString("/run/bimeron-desktop-integration-fail-") + argv[2])) return 1;
        if (!state.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 1;
        state.write(std::strcmp(argv[2], "start") == 0 ? "4" : "1");
        return 0;
    }
    if (!QtPlatformPreflight::requestedPlatformAvailable()) return 1;
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary settings");
    QCoreApplication::setOrganizationName("BimeronTests");
    QCoreApplication::setApplicationName("DeveloperMode-" + QUuid::createUuid().toString());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    if (app.arguments().contains("--repair-quit-ownership")) {
        ConnectionManagementTest::repairQuitOwnership();
        return 0;
    }
    {
        const QString journal = DevelopmentCore::recoveryJournalPath();
        require(QDir().mkpath(QFileInfo(journal).absolutePath()), "journal directory");
        for (const QByteArray record : {QByteArray("invalid"),
                QJsonDocument(QJsonObject{{"version", 1}, {"transition", QUuid::createUuid().toString()},
                    {"originalServiceState", "running"}, {"stage", "launching"},
                    {"path", "must-not-execute"}, {"pid", 42}}).toJson(QJsonDocument::Compact)}) {
            QFile file(journal);
            require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "write interrupted journal");
            require(file.write(record) == record.size(), "write entire journal");
            file.close();
            DevelopmentCore interrupted(temporary.filePath("must-not-execute"));
            require(interrupted.interruptedRecovery() && interrupted.failed(), "journal blocks normal backend control");
            require(!interrupted.local() && interrupted.processId() == 0 && interrupted.endpoint().isEmpty(),
                    "journal never adopts a path, PID or endpoint");
            require(!interrupted.recoveryDiagnostic().isEmpty(), "journal has an actionable diagnostic");
            bool inspected = false;
            interrupted.inspectSystemStart([&](bool allowed, const QString &message) {
                require(!allowed && !message.isEmpty(), "interrupted journal blocks automatic service start");
                inspected = true;
            });
            require(waitFor([&] { return inspected; }), "startup guard is asynchronous and bounded");
            inspected = false;
            interrupted.inspectSystemStart([&](DevelopmentCore::StartInspection result) {
                require(result.reason == DevelopmentCore::StartBlock::LifecycleConflict,
                        "interrupted journal is lifecycle uncertainty, not inferred network damage");
                inspected = true;
            });
            require(waitFor([&] { return inspected; }), "typed journal inspection completes");
            DevelopmentRecoveryTest::blockedWithoutCleanup(interrupted);
            DevelopmentRecoveryTest::peerMismatch(interrupted);
            interrupted.select(QCoreApplication::applicationFilePath());
            require(interrupted.interruptedRecovery() && !interrupted.busy() && !interrupted.local(),
                    "new local selection cannot bypass interrupted recovery");
            require(QFile::exists(journal), "failed selection retains recovery record");
            DevelopmentRecoveryTest::brokerFinalization(interrupted);
        }
        DevelopmentCore restored(temporary.filePath("must-not-execute"));
        DevelopmentRecoveryTest::healthyService(restored, true);
        DevelopmentRecoveryTest::healthyService(restored, false);
    }
    DevelopmentRecoveryTest::typedInspection(temporary.filePath("must-not-execute"));
    const int recoveryGeometry = app.arguments().indexOf("--recovery-geometry");
    if (recoveryGeometry >= 0)
        return ConnectionManagementTest::recoveryGeometry(app, app.arguments().value(recoveryGeometry + 1));
    const int profileGeometry = app.arguments().indexOf("--profile-geometry");
    if (profileGeometry >= 0)
        return ConnectionManagementTest::profileGeometry(app, app.arguments().value(profileGeometry + 1));
    const int quitFixture = app.arguments().indexOf("--connection-quit");
    if (quitFixture >= 0)
        return ConnectionManagementTest::quitFixture(app, app.arguments().value(quitFixture + 1) == "stop");
    const int preview = app.arguments().indexOf("--connection-preview");
    if (preview >= 0)
        return ConnectionManagementTest::preview(app, app.arguments().value(preview + 1), app.arguments().value(preview + 2));
    const int integration = app.arguments().indexOf("--integration-core");
    if (integration >= 0) {
        require(integration + 1 < app.arguments().size(), "integration core path");
        require(QFile::exists("/run/bimeron-desktop-integration-service"), "integration requires isolated /run sentinel");
        DevelopmentCore backend(QCoreApplication::applicationFilePath());
        for (const QByteArray initial : {QByteArray("4"), QByteArray("1"), QByteArray("absent")}) {
            QFile state("/run/bimeron-desktop-integration-service");
            require(state.open(QIODevice::WriteOnly | QIODevice::Truncate), "service fixture");
            state.write(initial);
            state.close();
            backend.select(app.arguments().at(integration + 1));
            require(waitFor([&] { return !backend.busy(); }, 25000), "local startup timeout");
            if (backend.failed()) qFatal("%s", qPrintable(backend.status()));
            require(backend.local() && backend.processId() > 0, "real daemon must be active");
            CoreClient client;
            bool hello = false;
            QObject::connect(&client, &CoreClient::replied, &client, [&](const QString &, const QJsonObject &response) {
                hello = !response.value("core").toObject().value("version").toString().isEmpty();
            });
            client.setEndpoint(backend.endpoint(), backend.processId());
            client.request("hello");
            require(waitFor([&] { return hello; }), "GUI IPC must reach the real daemon");
            backend.select({});
            require(waitFor([&] { return !backend.busy(); }, 25000), "local cleanup timeout");
            if (backend.failed()) qFatal("%s", qPrintable(backend.status()));
            require(!backend.needsCleanup() && !backend.local(), "daemon must exit and restoration must finish");
            require(state.open(QIODevice::ReadOnly), "restored service fixture");
            require(state.readAll() == initial, "prior service state must be restored");
        }
        const auto setState = [](const QByteArray &value) {
            QFile state("/run/bimeron-desktop-integration-service");
            require(state.open(QIODevice::WriteOnly | QIODevice::Truncate), "service state write");
            state.write(value);
        };
        setState("4");
        backend.select(QCoreApplication::applicationFilePath());
        require(waitFor([&] { return !backend.busy(); }, 25000), "failed startup rollback timeout");
        require(backend.failed() && !backend.needsCleanup(), "failed startup must restore service without adopting another daemon");
        backend.select(app.arguments().at(integration + 1));
        require(waitFor([&] { return !backend.busy(); }, 25000) && !backend.failed(), "restart after failed startup");
        backend.select(app.arguments().at(integration + 1));
        require(waitFor([&] { return !backend.busy(); }, 25000) && !backend.failed(), "local-to-local transition");
#ifdef Q_OS_LINUX
        const int lock = open("/run/bimeron/vpn.lock", O_CREAT | O_RDWR, 0600);
        require(lock >= 0 && flock(lock, LOCK_EX | LOCK_NB) == 0, "hold VPN session lock");
        backend.select({});
        require(waitFor([&] { return !backend.busy(); }, 25000), "held-lock cleanup timeout");
        require(backend.failed() && backend.needsCleanup(), "held lock must block system restoration");
        QFile stopped("/run/bimeron-desktop-integration-service");
        require(stopped.open(QIODevice::ReadOnly) && stopped.readAll() == "1", "service must remain stopped while lock is held");
        require(flock(lock, LOCK_UN) == 0, "release VPN session lock");
        close(lock);
        backend.select({});
        require(waitFor([&] { return !backend.busy(); }, 25000) && !backend.failed(), "held-lock restoration retry");
        require(!backend.needsCleanup(), "retry must finish restoration");
        backend.select(app.arguments().at(integration + 1));
        require(waitFor([&] { return !backend.busy(); }, 25000) && !backend.failed(), "restart after held-lock retry");
#endif
        QFile fault("/run/bimeron-desktop-integration-fail-start");
        require(fault.open(QIODevice::WriteOnly), "restoration fault");
        fault.close();
        backend.select({});
        require(waitFor([&] { return !backend.busy(); }, 25000), "failed restoration timeout");
        require(backend.failed() && backend.needsCleanup() && !backend.local(), "failed restoration must retain retry state");
        setState("4");
        backend.select({});
        require(waitFor([&] { return !backend.busy(); }, 25000), "restoration retry timeout");
        require(!backend.failed() && !backend.needsCleanup(), "already-running service must complete restoration without another start");
        require(fault.remove(), "clear restoration fault");
        return 0;
    }
    if (app.arguments().contains("--development")) {
        MainWindow window;
        require(window.findChild<QStackedWidget *>()->count() == 5, "restart must expose development tab");
        require(window.findChild<QStackedWidget *>()->currentIndex() == 4, "restart must select development tab");
        const QString screenshot = qEnvironmentVariable("BIMERON_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            window.show();
            QCoreApplication::processEvents();
            require(window.grab().save(screenshot), "development screenshot");
        }
        auto *pathField = window.findChild<QLineEdit *>("developmentPath");
        require(pathField && pathField->text().isEmpty(), "first unlock must have no remembered path");
        auto *modified = window.findChild<QLabel *>("developmentModified");
        auto *backendChoice = window.findChild<QComboBox *>("developmentBackend");
        require(modified && backendChoice && modified->isHidden(), "system selection must hide local file time");
        backendChoice->setCurrentIndex(1);
        pathField->setText(QCoreApplication::applicationFilePath());
        require(!modified->isHidden() && modified->text().contains(QLocale().toString(
            QFileInfo(pathField->text()).lastModified().toLocalTime(), QLocale::ShortFormat)),
            "local file modification time must be displayed");
        QMetaObject::invokeMethod(pathField, "editingFinished");
        pathField->setText(temporary.filePath("missing-core"));
        require(modified->text().contains("unavailable"), "missing file must not show stale modification time");
        QMetaObject::invokeMethod(pathField, "editingFinished");
        MainWindow reopened;
        require(reopened.findChild<QLineEdit *>("developmentPath")->text() == QCoreApplication::applicationFilePath(),
                "later unlock must restore the last valid path");
        require(reopened.findChild<QComboBox *>("developmentBackend")->currentIndex() == 0,
                "remembered path must not select local core");
        require(reopened.findChild<QLabel *>("developmentModified")->isHidden(),
                "remembered path must not expose local file time while System is selected");
        require(!reopened.findChild<DevelopmentCore *>()->local(), "remembered path must not launch a daemon");
        QFile receipt(qEnvironmentVariable("BIMERON_TEST_RESTART_RECEIPT"));
        require(receipt.open(QIODevice::WriteOnly), "restart receipt");
        receipt.write("development");
        return 0;
    }
    testRouting();
    NativeIKEv2TransportTest::run();
    NativeIKEv2SessionTest::run();
    ConnectionManagementTest::profileGeometry(app, temporary.filePath("profile-geometry.png"));
    ConnectionManagementTest::run();
    ConnectionManagementTest::sharedGeoDataPreparation();
    ConnectionManagementTest::settingsGeoDataChecking();
    DevelopmentCore backend(temporary.filePath("missing-helper"));
    bool completed = false;
    QObject::connect(&backend, &DevelopmentCore::completed, &backend, [&](bool success) {
        require(!success, "invalid executable must fail");
        completed = true;
    });
    backend.select("relative-core");
    require(completed && !backend.busy() && !backend.needsCleanup(), "invalid path cannot acquire a daemon or service");
    const QString receipt = temporary.filePath("restart");
    qputenv("BIMERON_TEST_RESTART_RECEIPT", receipt.toUtf8());
    MainWindow window;
    auto *client = window.findChild<CoreClient *>();
    require(client, "window IPC client");
    const QString isolatedEndpoint = "bimeron-redaction-" + QUuid::createUuid().toString();
    client->setEndpoint(isolatedEndpoint);
    client->replied("hello", QJsonObject{{"instance", "redaction"}, {"status", QJsonObject{{"State", "disconnected"}}}});
    const QJsonObject secretEvent{{"Code", "future://SENTINEL"}, {"Message", "Password = SENTINEL"}};
    const QJsonObject freeTextEvent{{"Message", "SENTINEL"}};
    client->replied("events", QJsonObject{{"instance", "redaction"}, {"status", QJsonObject{{"State", "disconnected"}}}, {"events", QJsonArray{
        QJsonObject{{"event", secretEvent}}, QJsonObject{{"event", freeTextEvent}}}}});
    client->setEndpoint(isolatedEndpoint);
    client->replied("status", QJsonObject{{"instance", "redaction"}, {"status", QJsonObject{{"State", "failed"}, {"LastError", "{\"answers\":[\"SENTINEL\"]}"}}}});
    client->setEndpoint(isolatedEndpoint);
    bool diagnosticShown = false;
    QTimer::singleShot(0, [&] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box, "Core error dialog");
        diagnosticShown = box->text().contains("SENTINEL") && box->textFormat() == Qt::PlainText;
        box->accept();
    });
    client->replied("start", QJsonObject{{"instance", "redaction"}, {"status", QJsonObject{{"State", "failed"}}}, {"error", "SENTINEL"}});
    require(diagnosticShown, "unknown IPC diagnostic is shown as plain text");
    const auto *log = window.findChild<QTextEdit *>();
    require(log && log->toPlainText().contains("SENTINEL"), "IPC diagnostic details remain in Journal and its export text");
    client->setEndpoint(isolatedEndpoint);
#ifdef Q_OS_WIN
    window.setProperty("developmentRestartWithoutElevation", true);
#endif
    auto *pages = window.findChild<QStackedWidget *>();
    require(pages && pages->count() == 4, "ordinary launch must hide development");
    auto *label = window.findChild<QLabel *>("desktopVersion");
    require(label && label->cursor().shape() == Qt::ArrowCursor && label->toolTip().isEmpty(), "activation must have no affordance");
    const auto release = [label](Qt::MouseButton button) {
        QMouseEvent event(QEvent::MouseButtonRelease, QPointF(1, 1), QPointF(1, 1), button, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(label, &event);
    };
    release(Qt::RightButton);
    for (int click = 0; click < 4; ++click) release(Qt::LeftButton);
    require(pages->count() == 4 && !QFile::exists(receipt), "first four clicks must be silent");
    QTimer::singleShot(0, &app, [&] { release(Qt::LeftButton); });
    QTimer::singleShot(5000, &app, [] { qFatal("unlock did not restart"); });
    app.exec();
    require(waitFor([&] { return QFile::exists(receipt); }, 5000), "fifth click must launch development session");
    return 0;
}
