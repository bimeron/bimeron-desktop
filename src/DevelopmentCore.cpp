#include "DevelopmentCore.h"
#include "ElevationOwner.h"
#include "TrustedCore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <QHash>
#include <memory>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#include <sddl.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <cerrno>
#endif

namespace {
#ifdef Q_OS_WIN
bool elevatedProcess() {
    HANDLE token = nullptr;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool elevated = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)
        && GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)
        && elevation.TokenIsElevated;
    if (token) CloseHandle(token);
    return elevated;
}

QString processSid(HANDLE process) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return {};
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    QByteArray data(static_cast<qsizetype>(size), '\0');
    QString result;
    if (GetTokenInformation(token, TokenUser, data.data(), size, &size)) {
        LPWSTR sid = nullptr;
        if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(data.data())->User.Sid, &sid)) {
            result = QString::fromWCharArray(sid);
            LocalFree(sid);
        }
    }
    CloseHandle(token);
    return result;
}

void elevate(QObject *owner, const QString &file, const QStringList &arguments,
             std::function<void(HANDLE)> done) {
    auto handle = std::make_shared<HANDLE>(nullptr);
    const HWND window = elevationOwnerWindow();
    auto *thread = QThread::create([file, arguments, handle, window] {
        const auto executable = QDir::toNativeSeparators(file).toStdWString();
        const auto parameters = TrustedCore::windowsCommandLine(arguments).toStdWString();
        const auto directory = QDir::toNativeSeparators(QFileInfo(file).absolutePath()).toStdWString();
        if (elevatedProcess()) {
            std::wstring commandLine = TrustedCore::windowsCommandLine(QStringList{file} + arguments).toStdWString();
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            if (CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                               CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process)) {
                CloseHandle(process.hThread);
                *handle = process.hProcess;
            }
            return;
        }
        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
        info.hwnd = window;
        info.lpVerb = L"runas";
        info.lpFile = executable.c_str();
        info.lpParameters = parameters.c_str();
        info.lpDirectory = directory.c_str();
        info.nShow = SW_HIDE;
        if (ShellExecuteExW(&info)) *handle = info.hProcess;
    });
    QObject::connect(thread, &QThread::finished, owner, [handle, done] { done(*handle); });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
#endif
}

DevelopmentCore::DevelopmentCore(QString helper, QObject *parent)
    : QObject(parent), helper_(std::move(helper)) {
    readJournal();
    timer_.setInterval(200);
    connect(&timer_, &QTimer::timeout, this, &DevelopmentCore::tick);
    connect(&control_, &CoreClient::replied, this, [this](const QString &command, const QJsonObject &response) {
        if (phase_ == Phase::Verifying) {
#ifdef Q_OS_WIN
            orphanAvailable_ = !path_.isEmpty();
#endif
            finish(false, tr("The local daemon still responds; close it before retrying"));
            return;
        }
        if (phase_ == Phase::Starting && command == "hello") {
            if (response.contains("error") || response.value("instance").toString().isEmpty()
                || response.value("core").toObject().value("version").toString().isEmpty()) {
                failure_ = tr("Local core returned an incompatible hello");
                requestedPath_.clear();
                shutdown();
                return;
            }
            phase_ = Phase::Active;
            finish(true, tr("Local core: %1").arg(response.value("core").toObject().value("version").toString()));
        } else if (phase_ == Phase::Stopping && command == "stop") {
            if (response.contains("error") || response.value("status").toObject().value("State").toString() != "disconnected") {
                finish(false, tr("The local connection did not disconnect; retry after checking the daemon"));
                return;
            }
            stopConfirmed_ = true;
            shutdownSent_ = true;
            control_.request("shutdown", {}, 120000);
        } else if (phase_ == Phase::Stopping && command == "shutdown" &&
                   (response.contains("error") || response.value("status").toObject().value("State").toString() != "disconnected")) {
            finish(false, tr("The local daemon did not shut down; retry before using the system service"));
        }
    });
    connect(&control_, &CoreClient::unavailable, this, [this](const QString &) {
        if (phase_ == Phase::Verifying) verifyLock();
        else if (phase_ == Phase::Stopping)
            finish(false, tr("The local daemon did not confirm connection cleanup; retry before using the system service"));
    });
}

DevelopmentCore::~DevelopmentCore() {
#ifdef Q_OS_WIN
    if (processHandle_) CloseHandle(reinterpret_cast<HANDLE>(processHandle_));
#endif
}

void DevelopmentCore::command(const QString &file, const QStringList &arguments, bool elevated, Result result) {
#ifdef Q_OS_WIN
    if (elevated && !elevatedProcess()) {
        elevate(this, file, arguments, [this, result](HANDLE handle) {
            if (!handle) { result(-1, tr("Elevation was canceled or failed")); return; }
            auto *poller = new QTimer(this);
            auto elapsed = std::make_shared<QElapsedTimer>();
            elapsed->start();
            connect(poller, &QTimer::timeout, this, [this, handle, poller, result, elapsed] {
                if (WaitForSingleObject(handle, 0) == WAIT_TIMEOUT) {
                    if (elapsed->elapsed() > 120000 && status_ != tr("Waiting for the service command to finish…")) {
                        status_ = tr("Waiting for the service command to finish…");
                        emit changed();
                    }
                    return;
                }
                DWORD code = 1;
                GetExitCodeProcess(handle, &code);
                CloseHandle(handle);
                poller->stop();
                poller->deleteLater();
                result(static_cast<int>(code), {});
            });
            poller->start(200);
        });
        return;
    }
#endif
    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    auto output = std::make_shared<QByteArray>();
    auto finished = std::make_shared<bool>(false);
    connect(process, &QProcess::readyRead, this, [process, output] {
        const QByteArray bytes = process->readAll();
        if (output->size() < 16384) output->append(bytes.left(16384 - output->size()));
    });
    const auto complete = [process, result, output, finished](int code) {
        if (*finished) return;
        *finished = true;
        output->append(process->readAll().left(16384 - output->size()));
        process->deleteLater();
        result(code, QString::fromUtf8(*output).trimmed());
    };
    connect(process, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(status == QProcess::NormalExit ? code : -1);
    });
    connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete(-1);
    });
#ifdef Q_OS_LINUX
    if (elevated && geteuid() != 0)
        process->start(QStringLiteral("/usr/bin/pkexec"), QStringList{file} + arguments);
    else
#endif
        process->start(file, arguments);
    QTimer::singleShot(elevated ? 120000 : 5000, process, [this, process, elevated] {
        if (process->state() == QProcess::NotRunning) return;
        if (!elevated) process->kill();
        else { status_ = tr("Waiting for the service command to finish…"); emit changed(); }
    });
}

void DevelopmentCore::select(const QString &path) {
    if (busy_) return;
    if (interruptedRecovery_) {
        if (path.isEmpty()) recoverInterrupted();
        else finish(false, tr("Resolve the interrupted core switch before selecting another core"));
        return;
    }
    timer_.stop();
    requestedPath_ = path;
    failure_.clear();
    busy_ = true;
    failed_ = false;
    status_ = tr("Switching core…");
    emit changed();
    if (path.isEmpty()) { shutdown(); return; }
    const QFileInfo file(path);
    if (!file.isAbsolute() || !file.isFile() || !file.isExecutable()) {
        finish(false, tr("Select an existing executable using an absolute path"));
        return;
    }
#ifdef Q_OS_WIN
    if (!QFileInfo::exists(file.dir().filePath("wintun.dll"))) {
        finish(false, tr("Place the matching wintun.dll beside the selected core"));
        return;
    }
#endif
    command(path, {"version"}, false, [this](int code, const QString &output) {
        if (code != 0 || output.isEmpty()) {
            finish(false, tr("Cannot run the selected core; check its architecture and runtime dependencies"));
            return;
        }
        if (local() || restoreService_) shutdown();
        else prepare();
    });
}

void DevelopmentCore::prepare() {
    command(helper_, {"service", "status"}, false, [this](int code, const QString &output) {
        const QString state = output.section(':', 1).trimmed();
        if (code != 0 && !output.contains("not installed", Qt::CaseInsensitive)) {
            finish(false, tr("Cannot determine the system service state"));
        } else if (code != 0 || state == "1") {
            journalOriginalState_ = code != 0 ? "absent" : "stopped";
            if (!writeJournal("launching")) { finish(false, recoveryDiagnostic_); return; }
            launch();
        } else if (state == "4") {
            journalOriginalState_ = "running";
            if (!writeJournal("stopping-service")) { finish(false, recoveryDiagnostic_); return; }
            restoreService_ = true;
            command(helper_, {"service", "stop"}, true, [this](int stopped, const QString &) {
                if (stopped != 0) {
                    failure_ = tr("Could not stop the system service");
                    requestedPath_.clear();
                    restore();
                    return;
                }
                command(helper_, {"service", "status"}, false, [this](int code, const QString &output) {
                    if (code == 0 && output.section(':', 1).trimmed() == "1") launch();
                    else {
                        failure_ = tr("Could not stop the system service");
                        requestedPath_.clear();
                        restore();
                    }
                });
            });
        } else {
            finish(false, tr("Wait for the system service to reach a stable state"));
        }
    });
}

void DevelopmentCore::launch() {
    if (!writeJournal("launching")) { finish(false, recoveryDiagnostic_); return; }
    path_ = requestedPath_;
    const auto started = [this] {
        control_.setEndpoint(endpoint_, processId_);
        phase_ = Phase::Starting;
        deadline_.restart();
        timer_.start();
        tick();
    };
#ifdef Q_OS_WIN
    elevate(this, path_, {"daemon"}, [this, started](HANDLE handle) {
        if (!handle) {
            path_.clear();
            requestedPath_.clear();
            failure_ = tr("Elevation was canceled or failed");
            restore();
            return;
        }
        processHandle_ = reinterpret_cast<quintptr>(handle);
        processId_ = GetProcessId(handle);
        const QString sid = processSid(handle);
        if (!processId_ || sid.isEmpty() || sid != processSid(GetCurrentProcess())) {
            finish(false, tr("The daemon must be elevated as the same Windows user; close it before retrying"));
            return;
        }
        endpoint_ = "Bimeron.Core." + QString::fromLatin1(QCryptographicHash::hash(sid.toUtf8(), QCryptographicHash::Sha256).left(16).toHex());
        started();
    });
#elif defined(Q_OS_LINUX)
    endpoint_ = CoreClient::systemEndpoint();
    process_ = new QProcess(this);
    process_->setStandardOutputFile(QProcess::nullDevice());
    process_->setStandardErrorFile(QProcess::nullDevice());
    connect(process_, &QProcess::started, this, [this, started] {
        processId_ = process_->processId();
        started();
    });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        releaseProcess();
        requestedPath_.clear();
        failure_ = tr("Elevation was canceled or failed");
        restore();
    });
    QStringList arguments{"BIMERON_CONTROL_UID=" + QString::number(getuid()), path_, "daemon"};
    if (geteuid() == 0) process_->start("/usr/bin/env", arguments);
    else process_->start("/usr/bin/pkexec", QStringList{"/usr/bin/env"} + arguments);
#else
    path_.clear();
    finish(false, tr("Local core is not supported on this platform"));
#endif
}

bool DevelopmentCore::processRunning() const {
#ifdef Q_OS_WIN
    return processHandle_ && WaitForSingleObject(reinterpret_cast<HANDLE>(processHandle_), 0) == WAIT_TIMEOUT;
#else
    return process_ && process_->state() != QProcess::NotRunning;
#endif
}

void DevelopmentCore::releaseProcess() {
    control_.setEndpoint();
#ifdef Q_OS_WIN
    if (processHandle_) CloseHandle(reinterpret_cast<HANDLE>(processHandle_));
    processHandle_ = 0;
#endif
    if (process_) { process_->deleteLater(); process_ = nullptr; }
    processId_ = 0;
    path_.clear();
    endpoint_.clear();
    phase_ = Phase::Idle;
    timer_.stop();
}

void DevelopmentCore::stopOrphan(bool force) {
#ifdef Q_OS_WIN
    if (busy_ || !orphanAvailable_ || endpoint_.isEmpty() || path_.isEmpty()) return;
    busy_ = true;
    emit changed();
    auto *socket = new QLocalSocket(this);
    const auto fail = [this, socket] {
        if (socket->property("finished").toBool()) return;
        socket->setProperty("finished", true);
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
        finish(false, tr("Cannot verify the remaining daemon; no shutdown was sent"));
    };
    connect(socket, &QLocalSocket::connected, this, [this, socket, fail, force] {
        ULONG pid = 0;
        if (!GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(socket->socketDescriptor()), &pid)) { fail(); return; }
        HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | (force ? PROCESS_TERMINATE : 0), FALSE, pid);
        wchar_t image[32768];
        DWORD length = 32768;
        if (!handle || QFileInfo(path_).canonicalFilePath().isEmpty()
            || !QueryFullProcessImageNameW(handle, 0, image, &length)
            || QFileInfo(QString::fromWCharArray(image, length)).canonicalFilePath().compare(
                QFileInfo(path_).canonicalFilePath(), Qt::CaseInsensitive) != 0
            || processSid(handle).isEmpty() || processSid(handle) != processSid(GetCurrentProcess())) {
            if (handle) CloseHandle(handle);
            fail(); return;
        }
        if (force) {
            const bool stopped = TerminateProcess(handle, 1);
            CloseHandle(handle);
            if (!stopped) { fail(); return; }
            socket->setProperty("finished", true);
            socket->disconnect(this);
            socket->deleteLater();
            emit forcedExitReady();
            return;
        }
        if (processHandle_) CloseHandle(reinterpret_cast<HANDLE>(processHandle_));
        processHandle_ = reinterpret_cast<quintptr>(handle);
        processId_ = pid;
        socket->setProperty("finished", true);
        socket->disconnect(this);
        socket->deleteLater();
        orphanAvailable_ = false;
        requestedPath_.clear();
        shutdown();
    });
    connect(socket, &QLocalSocket::errorOccurred, this, [fail](QLocalSocket::LocalSocketError) { fail(); });
    socket->connectToServer(endpoint_);
    QTimer::singleShot(3000, socket, [socket, fail] {
        if (socket->state() == QLocalSocket::ConnectingState) fail();
    });
#endif
}

void DevelopmentCore::shutdown() {
    if (path_.isEmpty() && !restoreService_) { finish(true, tr("System service")); return; }
    if (!processRunning()) { verifyExit(); return; }
    if (endpoint_.isEmpty() || processId_ <= 0) {
        finish(false, tr("Cannot identify the local daemon; close it before retrying"));
        return;
    }
    phase_ = Phase::Stopping;
    shutdownSent_ = false;
    stopConfirmed_ = false;
    control_.setEndpoint(endpoint_, processId_);
    deadline_.restart();
    timer_.start();
    tick();
}

void DevelopmentCore::tick() {
    if (phase_ == Phase::Verifying) return;
    if (!processRunning()) {
        verifyExit();
        return;
    }
    if (phase_ == Phase::Starting) {
        if (deadline_.elapsed() > 15000) {
            failure_ = tr("Local core did not become ready");
            requestedPath_.clear();
            shutdown();
        } else if (!control_.busy()) control_.request("hello", {}, 1000);
    } else if (phase_ == Phase::Stopping) {
        if (deadline_.elapsed() > 120000) {
            finish(false, tr("Local core cleanup timed out; retry returning to the system service"));
        } else if (!shutdownSent_ && !control_.busy()) {
            control_.request("stop", {}, 120000);
        }
    }
}

void DevelopmentCore::verifyExit() {
    exitPhase_ = phase_;
    phase_ = Phase::Verifying;
    timer_.stop();
    if (endpoint_.isEmpty()) { verifyLock(); return; }
    control_.setEndpoint(endpoint_);
    control_.request("hello", {}, 1000);
}

void DevelopmentCore::verifyLock() {
    if (phase_ != Phase::Verifying) return;
    const auto checked = [this](bool free) { verifiedLock(free); };
#ifdef Q_OS_WIN
    HANDLE mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, L"Global\\BimeronCoreConnect");
    if (!mutex) { checked(GetLastError() == ERROR_FILE_NOT_FOUND); return; }
    const DWORD result = WaitForSingleObject(mutex, 0);
    const bool free = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    if (free) ReleaseMutex(mutex);
    CloseHandle(mutex);
    checked(free);
#elif defined(Q_OS_LINUX)
    if (!QFileInfo::exists("/run/bimeron/vpn.lock")) { checked(true); return; }
    const QString flock = QStandardPaths::findExecutable("flock");
    const QString trueCommand = QStandardPaths::findExecutable("true");
    if (flock.isEmpty() || trueCommand.isEmpty()) { checked(false); return; }
    command(flock, {"-n", "/run/bimeron/vpn.lock", trueCommand}, true,
            [checked](int code, const QString &) { checked(code == 0); });
#else
    checked(false);
#endif
}

void DevelopmentCore::verifiedLock(bool free) {
        if (!free) {
            finish(false, tr("The session lock is still held; close the local daemon and retry"));
            return;
        }
        if (exitPhase_ == Phase::Stopping && !stopConfirmed_) {
            finish(false, tr("The local daemon did not confirm connection cleanup; retry before using the system service"));
            return;
        }
        if (interruptedRecovery_ && !interruptedCleanupConfirmed_) {
            interruptedCleanupNeeded_ = true;
            finish(false, tr("Network cleanup from the interrupted session is unconfirmed. Use Emergency network recovery before restoring the service."));
            return;
        }
        const Phase previous = exitPhase_;
        releaseProcess();
        if (previous == Phase::Active) {
            finish(false, tr("Local core exited; return to the system service to restore it"));
        } else {
            if (previous == Phase::Starting) {
                failure_ = tr("Local core exited before it became ready");
                requestedPath_.clear();
            }
            restore();
        }
}

void DevelopmentCore::restore() {
    if (!restoreService_) { completeRestore(); return; }
    command(helper_, {"service", "status"}, false, [this](int code, const QString &output) {
        if (code != 0) { finish(false, tr("Could not restore the system service; retry")); return; }
        const QString state = output.section(':', 1).trimmed();
        if (state == "4") { completeRestore(); return; }
        if (state != "1") { confirmServiceStarted(40); return; }
        command(helper_, {"service", "start"}, true, [this](int, const QString &) {
            confirmServiceStarted(40);
        });
    });
}

void DevelopmentCore::confirmServiceStarted(int attempts) {
    command(helper_, {"service", "status"}, false, [this, attempts](int code, const QString &output) {
        if (code == 0 && output.section(':', 1).trimmed() == "4") {
            completeRestore();
        } else if (code == 0 && attempts > 0 && output.section(':', 1).trimmed() != "1") {
            QTimer::singleShot(250, this, [this, attempts] { confirmServiceStarted(attempts - 1); });
        } else {
            finish(false, tr("Could not restore the system service; retry"));
        }
    });
}

void DevelopmentCore::completeRestore() {
    if (!clearJournal()) { finish(false, recoveryDiagnostic_); return; }
    restoreService_ = false;
    if (!requestedPath_.isEmpty()) prepare();
    else finish(failure_.isEmpty(), failure_.isEmpty() ? tr("System service") : failure_);
}

bool DevelopmentCore::brokerRestoredSystem() {
    if (busy_) return false;
#ifdef Q_OS_WIN
    // WAIT_FAILED is not proof of exit. Retain the handle and recovery record.
    if (processHandle_ && WaitForSingleObject(reinterpret_cast<HANDLE>(processHandle_), 0) != WAIT_OBJECT_0)
        return false;
#else
    if (process_ && process_->state() != QProcess::NotRunning) return false;
#endif
    if (!clearJournal()) return false;
    if (process_) process_->disconnect(this);
    releaseProcess();
    requestedPath_.clear();
    failure_.clear();
    restoreService_ = false;
    orphanAvailable_ = false;
    shutdownSent_ = stopConfirmed_ = false;
    interruptedCleanupNeeded_ = false;
    exitPhase_ = Phase::Idle;
    failed_ = false;
    status_ = tr("System service");
    emit changed();
    return true;
}

QString DevelopmentCore::recoveryJournalPath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath("core-recovery.json");
}

void DevelopmentCore::readJournal() {
    QFile file(recoveryJournalPath());
    if (!file.exists()) return;
    interruptedRecovery_ = journalActive_ = failed_ = true;
    recoveryDiagnostic_ = tr("An interrupted core switch was found. Close any remaining local daemon, then retry recovery.");
    status_ = recoveryDiagnostic_;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4096) return;
    const QJsonObject record = QJsonDocument::fromJson(file.readAll()).object();
    const QString original = record.value("originalServiceState").toString();
    const QString transition = record.value("transition").toString();
    const QString stage = record.value("stage").toString();
    if (record.value("version").toInt() != 1 || QUuid(transition).isNull()
        || (stage != "launching" && stage != "stopping-service" && stage != "cleanup-confirmed")
        || (original != "running" && original != "stopped" && original != "absent")) {
        recoveryDiagnostic_ = tr("The core recovery record is invalid. Close any local daemon and retry inspection; no saved executable will be launched.");
        status_ = recoveryDiagnostic_;
        return;
    }
    journalOriginalState_ = original;
    journalTransition_ = transition;
    interruptedCleanupConfirmed_ = stage == "cleanup-confirmed";
}

bool DevelopmentCore::writeJournal(const QString &stage) {
    if (journalTransition_.isEmpty()) journalTransition_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString path = recoveryJournalPath();
    QSaveFile file(path);
    const QByteArray bytes = QJsonDocument(QJsonObject{{"version", 1},
        {"transition", journalTransition_}, {"stage", stage},
        {"originalServiceState", journalOriginalState_}}).toJson(QJsonDocument::Compact);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        recoveryDiagnostic_ = tr("Cannot save core recovery information; the switch was stopped before further changes");
        return false;
    }
    journalActive_ = true;
    return true;
}

bool DevelopmentCore::clearJournal() {
    if (journalActive_ && QFile::exists(recoveryJournalPath()) && !QFile::remove(recoveryJournalPath())) {
        recoveryDiagnostic_ = tr("Core restoration completed, but its recovery record could not be removed; retry");
        return false;
    }
    journalActive_ = interruptedRecovery_ = interruptedCleanupConfirmed_ = false;
    journalOriginalState_.clear();
    journalTransition_.clear();
    recoveryDiagnostic_.clear();
    return true;
}

void DevelopmentCore::recoverInterrupted() {
    if (!interruptedRecovery_ || busy_) return;
    busy_ = true;
    failed_ = false;
    requestedPath_.clear();
    failure_.clear();
    status_ = tr("Checking interrupted core switch…");
    emit changed();
    queryLinuxService([this](qint64 pid) {
        if (pid > 0) reconcileLinuxService(pid);
        else inspectInterruptedEndpoint();
    });
}

void DevelopmentCore::inspectInterruptedEndpoint() {
    // Persisted data is an intention, never authority to address or adopt a process.
    // Windows has a separate per-user local endpoint; Linux shares the service one.
#ifdef Q_OS_WIN
    const QString sid = processSid(GetCurrentProcess());
    if (sid.isEmpty()) { finish(false, tr("Cannot verify the current user's local core endpoint")); return; }
    endpoint_ = "Bimeron.Core." + QString::fromLatin1(QCryptographicHash::hash(sid.toUtf8(), QCryptographicHash::Sha256).left(16).toHex());
#else
    endpoint_ = CoreClient::systemEndpoint();
#endif
    restoreService_ = journalOriginalState_ == "running";
    // A failed or inaccessible endpoint is not proof of absence. Probe the socket
    // directly and only proceed for the OS's explicit not-found/refused result.
    auto *socket = new QLocalSocket(this);
    const auto done = [this, socket](bool absent) {
        if (socket->property("completed").toBool()) return;
        socket->setProperty("completed", true);
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
        if (!absent) { finish(false, tr("A local core may still be running or inaccessible. Close it manually before retrying recovery.")); return; }
        endpoint_.clear();
        phase_ = Phase::Verifying;
        exitPhase_ = Phase::Idle;
        verifyLock();
    };
    connect(socket, &QLocalSocket::connected, this, [done] { done(false); });
    connect(socket, &QLocalSocket::errorOccurred, this, [done](QLocalSocket::LocalSocketError error) {
        done(error == QLocalSocket::ServerNotFoundError || error == QLocalSocket::ConnectionRefusedError);
    });
    socket->connectToServer(endpoint_);
    QTimer::singleShot(3000, socket, [done] { done(false); });
}

void DevelopmentCore::queryLinuxService(std::function<void(qint64)> result) {
#ifdef Q_OS_LINUX
    // Query the service manager live; never read a PID from the recovery record.
    command(QStringLiteral("/usr/bin/systemctl"), {"show", "--no-pager", "--property=LoadState",
        "--property=ActiveState", "--property=MainPID", "bimeron-core.service"}, false,
        [result](int code, const QString &output) {
            QHash<QString, QString> fields;
            for (const QString &line : output.split('\n')) {
                const qsizetype equals = line.indexOf('=');
                if (equals > 0) fields.insert(line.left(equals), line.mid(equals + 1).trimmed());
            }
            bool valid = false;
            const qint64 pid = fields.value("MainPID").toLongLong(&valid);
            result(code == 0 && fields.value("LoadState") == "loaded" && fields.value("ActiveState") == "active"
                && valid && pid > 0 ? pid : 0);
        });
#else
    result(0);
#endif
}

void DevelopmentCore::reconcileLinuxService(qint64 pid, const QString &endpoint,
    std::function<void(std::function<void(qint64)>)> query) {
    auto *probe = new CoreClient(this);
    probe->setEndpoint(endpoint, pid);
    auto instance = std::make_shared<QString>();
    const auto fail = [this, probe] {
        probe->disconnect(this);
        probe->deleteLater();
        finish(false, tr("The running service could not be verified. Recovery remains blocked; no service was restarted."));
    };
    connect(probe, &CoreClient::unavailable, this, [fail](const QString &) { fail(); });
    connect(probe, &CoreClient::replied, this, [this, probe, instance, pid, fail, query](const QString &command, const QJsonObject &response) {
        if (response.contains("error") || response.value("instance").toString().isEmpty()) { fail(); return; }
        if (command == "hello") {
            if (response.value("core").toObject().value("version").toString().isEmpty()) { fail(); return; }
            *instance = response.value("instance").toString();
            probe->request("status", {}, 5000);
            return;
        }
        const QJsonObject status = response.value("status").toObject();
        const QString state = status.value("State").toString();
        if (command != "status" || response.value("instance").toString() != *instance
            || (state != "connected" && state != "disconnected")
            || !status.value("LastError").toString().isEmpty()) { fail(); return; }
        probe->disconnect(this);
        probe->deleteLater();
        const auto verified = [this, pid](qint64 currentPid) {
            if (currentPid != pid) {
                finish(false, tr("The service changed during recovery inspection. Retry without restarting it."));
                return;
            }
            // The actual live system service is already healthy; preserve its VPN.
            if (!clearJournal()) { finish(false, recoveryDiagnostic_); return; }
            restoreService_ = false;
            finish(true, tr("System service"));
        };
        if (query) query(verified);
        else queryLinuxService(verified);
    });
    probe->request("hello", {}, 5000);
}

void DevelopmentCore::recoverInterruptedNetwork() {
    if (!interruptedRecovery_ || busy_) return;
    busy_ = true;
    status_ = tr("Emergency network recovery");
    emit changed();
    command(helper_, {"recover"}, true, [this](int code, const QString &) {
        if (code != 0) { finish(false, tr("Emergency network recovery failed; the interrupted session remains blocked")); return; }
        // The Core recover contract acquires the VPN lock and exits successfully
        // only after routes/DNS/protection cleanup. Persist before any service start.
        if (journalOriginalState_.isEmpty()) journalOriginalState_ = "stopped";
        if (!writeJournal("cleanup-confirmed")) { finish(false, recoveryDiagnostic_); return; }
        interruptedCleanupConfirmed_ = true;
        interruptedCleanupNeeded_ = false;
        finish(false, tr("Network cleanup completed. Retry interrupted-session recovery to restore the service."));
    });
}

void DevelopmentCore::finish(bool success, const QString &message) {
    busy_ = false;
    failed_ = !success;
    status_ = message;
    if (!success) { phase_ = Phase::Failed; timer_.stop(); }
    emit changed();
    emit completed(success);
}


void DevelopmentCore::inspectSystemStart(std::function<void(bool, QString)> done) {
    inspectSystemStart([done](StartInspection result) { done(result.allowed(), result.diagnostic); });
}

void DevelopmentCore::inspectSystemStart(std::function<void(StartInspection)> done) {
    inspectSystemStartAtEndpoint({}, std::move(done));
}

void DevelopmentCore::inspectSystemStartAtEndpoint(const QString &requestedEndpoint, std::function<void(StartInspection)> done) {
    // A private socket owns the deadline; destroying this object cancels callbacks.
    auto *socket = new QLocalSocket(this);
    const auto blocked = [this] {
        return local() || busy_ || failed_ || interruptedRecovery_ || journalActive_
            || restoreService_ || orphanAvailable_ || phase_ != Phase::Idle;
    };
    const auto finishProbe = [this, socket, done, blocked](StartBlock reason) {
        if (socket->property("completed").toBool()) return;
        socket->setProperty("completed", true);
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
        if (reason == StartBlock::None && blocked()) reason = StartBlock::LifecycleConflict;
        const QHash<StartBlock, QString> evidence{
            {StartBlock::LifecycleConflict, tr("A developer transition or unresolved recovery record prevents service start.")},
            {StartBlock::EndpointOccupied, tr("The connection endpoint is occupied; its owner has not been verified.")},
            {StartBlock::EndpointAccessDenied, tr("Permission was denied while inspecting the connection endpoint.")},
            {StartBlock::EndpointUnknown, tr("The connection endpoint could not be inspected.")},
            {StartBlock::LockOccupied, tr("The session lock is held; its owner has not been verified.")},
            {StartBlock::LockAccessDenied, tr("Permission was denied while inspecting the session lock.")},
            {StartBlock::LockUnknown, tr("The session lock could not be inspected.")},
            {StartBlock::CleanupUnconfirmed, tr("The previous session abandoned its lock; network cleanup is unconfirmed.")},
            {StartBlock::TimedOut, tr("Connection endpoint inspection timed out.")}};
        done({reason, evidence.value(reason)});
    };
    QTimer::singleShot(0, socket, [this, socket, blocked, finishProbe, requestedEndpoint] {
        if (blocked()) { finishProbe(StartBlock::LifecycleConflict); return; }
        QString endpoint;
#ifdef Q_OS_WIN
        const QString sid = processSid(GetCurrentProcess());
        if (sid.isEmpty()) { finishProbe(StartBlock::EndpointAccessDenied); return; }
        endpoint = "Bimeron.Core." + QString::fromLatin1(QCryptographicHash::hash(
            sid.toUtf8(), QCryptographicHash::Sha256).left(16).toHex());
#elif defined(Q_OS_LINUX)
        endpoint = CoreClient::systemEndpoint();
#else
        finishProbe(StartBlock::EndpointUnknown); return;
#endif
        connect(socket, &QLocalSocket::connected, this, [finishProbe] { finishProbe(StartBlock::EndpointOccupied); });
        connect(socket, &QLocalSocket::errorOccurred, this, [finishProbe](QLocalSocket::LocalSocketError error) {
            if (error != QLocalSocket::ServerNotFoundError && error != QLocalSocket::ConnectionRefusedError) {
                finishProbe(error == QLocalSocket::SocketAccessError ? StartBlock::EndpointAccessDenied : StartBlock::EndpointUnknown); return;
            }
            StartBlock reason = StartBlock::LockUnknown;
#ifdef Q_OS_WIN
            HANDLE mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, L"Global\\BimeronCoreConnect");
            if (!mutex) reason = GetLastError() == ERROR_FILE_NOT_FOUND ? StartBlock::None
                : GetLastError() == ERROR_ACCESS_DENIED ? StartBlock::LockAccessDenied : StartBlock::LockUnknown;
            else {
                const DWORD result = WaitForSingleObject(mutex, 0);
                reason = result == WAIT_OBJECT_0 ? StartBlock::None
                    : result == WAIT_ABANDONED ? StartBlock::CleanupUnconfirmed
                    : result == WAIT_TIMEOUT ? StartBlock::LockOccupied : StartBlock::LockUnknown;
                if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) ReleaseMutex(mutex);
                CloseHandle(mutex);
            }
#elif defined(Q_OS_LINUX)
            // Never create a lock or elevate. Missing lock is safe; permission
            // errors and every other unknown observation fail closed.
            const int fd = open("/run/bimeron/vpn.lock", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
            if (fd < 0) reason = errno == ENOENT ? StartBlock::None
                : (errno == EACCES || errno == EPERM) ? StartBlock::LockAccessDenied : StartBlock::LockUnknown;
            else {
                const bool free = flock(fd, LOCK_EX | LOCK_NB) == 0;
                reason = free ? StartBlock::None : (errno == EWOULDBLOCK || errno == EAGAIN)
                    ? StartBlock::LockOccupied : (errno == EACCES || errno == EPERM)
                    ? StartBlock::LockAccessDenied : StartBlock::LockUnknown;
                if (free) flock(fd, LOCK_UN);
                close(fd);
            }
#endif
            finishProbe(reason);
        });
        socket->connectToServer(requestedEndpoint.isEmpty() ? endpoint : requestedEndpoint);
        QTimer::singleShot(3000, socket, [finishProbe] { finishProbe(StartBlock::TimedOut); });
    });
}
