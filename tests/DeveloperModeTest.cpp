#include "CoreClient.h"
#include "DevelopmentCore.h"
#include "MainWindow.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QLocalServer>
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

static void require(bool condition, const char *message) {
    if (!condition) qFatal("%s", message);
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
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary settings");
    QCoreApplication::setOrganizationName("BimeronTests");
    QCoreApplication::setApplicationName("DeveloperMode-" + QUuid::createUuid().toString());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
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
        QFile fault("/run/bimeron-desktop-integration-fail-start");
        require(fault.open(QIODevice::WriteOnly), "restoration fault");
        fault.close();
        backend.select({});
        require(waitFor([&] { return !backend.busy(); }, 25000), "failed restoration timeout");
        require(backend.failed() && backend.needsCleanup() && !backend.local(), "failed restoration must retain retry state");
        require(fault.remove(), "clear restoration fault");
        backend.select({});
        require(waitFor([&] { return !backend.busy(); }, 25000), "restoration retry timeout");
        require(!backend.failed() && !backend.needsCleanup(), "restoration retry must complete");
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
        QFile receipt(qEnvironmentVariable("BIMERON_TEST_RESTART_RECEIPT"));
        require(receipt.open(QIODevice::WriteOnly), "restart receipt");
        receipt.write("development");
        return 0;
    }
    testRouting();
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
