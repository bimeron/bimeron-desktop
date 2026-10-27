#include "DevelopmentCore.h"
#include "TrustedCore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>
#include <memory>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#include <sddl.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
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
    auto *thread = QThread::create([file, arguments, handle] {
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
    timer_.setInterval(200);
    connect(&timer_, &QTimer::timeout, this, &DevelopmentCore::tick);
    connect(&control_, &CoreClient::replied, this, [this](const QString &command, const QJsonObject &response) {
        if (phase_ == Phase::Verifying) {
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
            launch();
        } else if (state == "4") {
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
    const auto checked = [this](bool free) {
        if (!free) {
            finish(false, tr("The session lock is still held; close the local daemon and retry"));
            return;
        }
        if (exitPhase_ == Phase::Stopping && !stopConfirmed_) {
            finish(false, tr("The local daemon did not confirm connection cleanup; retry before using the system service"));
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
    };
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
    restoreService_ = false;
    if (!requestedPath_.isEmpty()) prepare();
    else finish(failure_.isEmpty(), failure_.isEmpty() ? tr("System service") : failure_);
}

void DevelopmentCore::finish(bool success, const QString &message) {
    busy_ = false;
    failed_ = !success;
    status_ = message;
    if (!success) { phase_ = Phase::Failed; timer_.stop(); }
    emit changed();
    emit completed(success);
}
