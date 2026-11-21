#define WIN32_LEAN_AND_MEAN
#include "installer-preflight-digest.h"
#include "DesktopMsiManifest.h"
#include <msi.h>
#include <msiquery.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <cstdio>

#ifdef BIMERON_MSI_HANDOFF_LAB_EXECUTION
#include "installer-preflight-process.h"
#include "msi-handoff-outcome.h"
#include "msi-handoff-lab-process.h"
#include "TrustedCore.h"

#include <shellapi.h>
#include <QDateTime>
#include <QElapsedTimer>
#include <QLockFile>
#include <QLocalSocket>
#include <QStandardPaths>
#include <vector>
#endif

namespace {
struct Identity {
    QString product;
    QString package;
    QString component;
    QString path;
    QString digest;
};

struct InstallerHandle {
    MSIHANDLE value = 0;
    ~InstallerHandle() { if (value) MsiCloseHandle(value); }
    InstallerHandle() = default;
    InstallerHandle(const InstallerHandle &) = delete;
    InstallerHandle &operator=(const InstallerHandle &) = delete;
};

bool matchesFile(const QString &path, const QString &digest) {
    return QFileInfo(path).isAbsolute() && InstallerPreflight::matchesPinnedDigest(
        QDir::toNativeSeparators(path).toStdWString(), digest.toStdString());
}

Identity identity(const QJsonObject &json) {
    return {json.value("product").toString(), json.value("package").toString(),
            json.value("component").toString(), json.value("path").toString(), json.value("sha256").toString()};
}

bool validIdentity(const Identity &value) {
    static const QRegularExpression guid(QStringLiteral(R"(\A\{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}\z)"));
    static const QRegularExpression digest(QStringLiteral(R"(\A[0-9a-f]{64}\z)"));
    return guid.match(value.product).hasMatch() && guid.match(value.package).hasMatch()
        && guid.match(value.component).hasMatch() && QFileInfo(value.path).isAbsolute()
        && QFileInfo(value.path).fileName().compare("bimeron-desktop.exe", Qt::CaseInsensitive) == 0
        && digest.match(value.digest).hasMatch();
}

QString query(MSIHANDLE database, const wchar_t *sql) {
    InstallerHandle view, record;
    if (MsiDatabaseOpenViewW(database, sql, &view.value) != ERROR_SUCCESS
        || MsiViewExecute(view.value, 0) != ERROR_SUCCESS
        || MsiViewFetch(view.value, &record.value) != ERROR_SUCCESS) return {};
    InstallerHandle duplicate;
    if (MsiViewFetch(view.value, &duplicate.value) != ERROR_NO_MORE_ITEMS) return {};
    wchar_t result[1024]{};
    DWORD length = std::size(result);
    if (MsiRecordGetStringW(record.value, 1, result, &length) != ERROR_SUCCESS) return {};
    return QString::fromWCharArray(result, length);
}

QString summary(MSIHANDLE database, UINT property) {
    InstallerHandle info;
    if (MsiGetSummaryInformationW(database, nullptr, 0, &info.value) != ERROR_SUCCESS) return {};
    UINT type = 0;
    wchar_t result[1024]{};
    DWORD length = std::size(result);
    if (MsiSummaryInfoGetPropertyW(info.value, property, &type, nullptr, nullptr, result, &length) != ERROR_SUCCESS)
        return {};
    return QString::fromWCharArray(result, length);
}

QByteArray resource(const QString &name) {
    QFile file(":/handoff/" + name);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

InstallerPreflight::FileHandle verifiedOffer(const QJsonObject &receipt, const Identity &next) {
    const QString path = receipt.value("msi_path").toString();
    const QString digest = receipt.value("msi_sha256").toString();
    const QString architecture = receipt.value("architecture").toString();
#if defined(_M_ARM64)
    if (architecture != "arm64") return {};
    const QString platform = "Arm64";
#elif defined(_M_X64)
    if (architecture != "amd64") return {};
    const QString platform = "x64";
#else
    return {};
#endif
    QList<QByteArray> keys;
    DesktopMsiManifest::Artifact signedArtifact;
    if (!CoreUpdate::parseTrustedKeys(resource("public-key").trimmed(), &keys)
        || !DesktopMsiManifest::verify(resource("manifest"), resource("signature"), keys,
                                      receipt.value("release_tag").toString(),
                                      receipt.value("installed_version").toString(),
                                      architecture, &signedArtifact, nullptr)
        || signedArtifact.name != QFileInfo(path).fileName() || signedArtifact.size != QFileInfo(path).size()
        || signedArtifact.msiSha256 != digest || signedArtifact.guiSha256 != next.digest
        || signedArtifact.productCode != next.product || signedArtifact.packageCode != next.package
        || signedArtifact.guiComponentCode != next.component)
        return {};

    if (!QFileInfo(path).isAbsolute()) return {};
    auto verified = InstallerPreflight::openPinnedFile(QDir::toNativeSeparators(path).toStdWString(), digest.toStdString());
    InstallerHandle database;
    if (!verified || MsiOpenDatabaseW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, &database.value) != ERROR_SUCCESS)
        return {};
    if (query(database.value, L"SELECT `Value` FROM `Property` WHERE `Property` = 'ProductCode'") != next.product
        || query(database.value, L"SELECT `Value` FROM `Property` WHERE `Property` = 'ProductVersion'") != receipt.value("release_tag").toString().mid(1)
        || summary(database.value, 9) != next.package
        || summary(database.value, 7).section(';', 0, 0).compare(platform, Qt::CaseInsensitive) != 0
        || query(database.value, L"SELECT `ComponentId` FROM `Component` WHERE `KeyPath` = 'CM_FP_bimeron_desktop.exe'") != next.component)
        return {};
    return verified;
}

bool installedMatches(const Identity &expected) {
    const auto product = expected.product.toStdWString();
    const auto component = expected.component.toStdWString();
    wchar_t package[64]{};
    DWORD packageLength = std::size(package);
    if (MsiGetProductInfoExW(product.c_str(), nullptr, MSIINSTALLCONTEXT_MACHINE, L"PackageCode",
                            package, &packageLength) != ERROR_SUCCESS ||
        QString::fromWCharArray(package, packageLength) != expected.package) return false;
    wchar_t path[1024]{};
    DWORD pathLength = std::size(path);
    if (MsiGetComponentPathExW(product.c_str(), component.c_str(), nullptr, MSIINSTALLCONTEXT_MACHINE,
                              path, &pathLength) != INSTALLSTATE_LOCAL) return false;
    const auto registeredPath = QString::fromWCharArray(path, pathLength);
    return QDir::toNativeSeparators(registeredPath).compare(
        QDir::toNativeSeparators(expected.path), Qt::CaseInsensitive) == 0
        && matchesFile(registeredPath, expected.digest);
}
#ifdef BIMERON_MSI_HANDOFF_LAB_EXECUTION
bool unelevatedInteractiveSession() {
    DWORD session = 0;
    HANDLE raw = nullptr;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) || !session
        || !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw)) return false;
    const InstallerPreflight::FileHandle token(raw);
    TOKEN_ELEVATION elevation{};
    DWORD bytes = 0;
    return GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &bytes)
        && !elevation.TokenIsElevated;
}

bool rebootPending() {
    for (const auto *path : {
            LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending)",
            LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired)"}) {
        HKEY key = nullptr;
        const auto result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &key);
        if (result == ERROR_SUCCESS) RegCloseKey(key);
        if (result != ERROR_FILE_NOT_FOUND && result != ERROR_PATH_NOT_FOUND) return true;
    }
    DWORD bytes = 0;
    const auto result = RegGetValueW(HKEY_LOCAL_MACHINE, LR"(SYSTEM\CurrentControlSet\Control\Session Manager)",
                                     L"PendingFileRenameOperations", RRF_RT_REG_MULTI_SZ,
                                     nullptr, nullptr, &bytes);
    return result != ERROR_FILE_NOT_FOUND && result != ERROR_PATH_NOT_FOUND;
}

InstallerPreflight::FileHandle startInstaller(const QString &path) {
    wchar_t systemDirectory[MAX_PATH]{};
    const auto length = GetSystemDirectoryW(systemDirectory, std::size(systemDirectory));
    if (!length || length >= std::size(systemDirectory) || path.contains('"')) return {};
    const std::wstring executable = std::wstring(systemDirectory) + LR"(\msiexec.exe)";
    const auto arguments = (QStringLiteral("/i \"%1\" /qb! /norestart").arg(QDir::toNativeSeparators(path))).toStdWString();
    SHELLEXECUTEINFOW call{};
    call.cbSize = sizeof(call);
    call.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    call.lpVerb = L"runas";
    call.lpFile = executable.c_str();
    call.lpParameters = arguments.c_str();
    call.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&call) || !call.hProcess) return {};
    return InstallerPreflight::FileHandle(call.hProcess);
}

bool launchDesktop(const Identity &gui, const QString &mode) {
    if (!TrustedCore::protectedExecutable(gui.path) || !installedMatches(gui) || !InstallerPreflight::desktopIsClosed())
        return false;
    const auto path = QDir::toNativeSeparators(gui.path).toStdWString();
    const auto image = InstallerPreflight::openPinnedFile(path, gui.digest.toStdString());
    if (!image) return false;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const InstallerPreflight::FileHandle ready(CreateEventW(&security, FALSE, FALSE, nullptr));
    if (!ready) return false;
    HANDLE inherited = ready.get();
    const auto command = QStringLiteral("\"%1\" --restart --msi-handoff-lab-%2 %3")
        .arg(QString::fromStdWString(path), mode, QString::number(quintptr(inherited))).toStdWString();
    const auto child = MsiHandoff::startLabProcess(path, command, &inherited, 1);
    DWORD ownerSession = 0, childSession = 0;
    if (!child || !ProcessIdToSessionId(GetCurrentProcessId(), &ownerSession)
        || !ProcessIdToSessionId(GetProcessId(child.get()), &childSession) || childSession != ownerSession) return false;
    const HANDLE waitFor[]{ready.get(), child.get()};
    return WaitForMultipleObjects(std::size(waitFor), waitFor, FALSE, 10000) == WAIT_OBJECT_0
        && WaitForSingleObject(child.get(), 0) == WAIT_TIMEOUT;
}

std::vector<BYTE> tokenUser(HANDLE process) {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &raw)) return {};
    const InstallerPreflight::FileHandle token(raw);
    DWORD bytes = 0;
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &bytes);
    if (bytes < sizeof(TOKEN_USER) || bytes > 4096) return {};
    std::vector<BYTE> info(bytes);
    if (!GetTokenInformation(token.get(), TokenUser, info.data(), bytes, &bytes)) return {};
    return info;
}

bool authenticatedOrigin(HANDLE process, const Identity &expected) {
    DWORD session = 0, ownSession = 0;
    if (!process || WaitForSingleObject(process, 0) != WAIT_TIMEOUT
        || !ProcessIdToSessionId(GetProcessId(process), &session)
        || !ProcessIdToSessionId(GetCurrentProcessId(), &ownSession) || session != ownSession)
        return false;
    const auto peer = tokenUser(process);
    const auto own = tokenUser(GetCurrentProcess());
    if (peer.empty() || own.empty()
        || !EqualSid(reinterpret_cast<const TOKEN_USER *>(peer.data())->User.Sid,
                     reinterpret_cast<const TOKEN_USER *>(own.data())->User.Sid)) return false;
    wchar_t path[1024]{};
    DWORD length = std::size(path);
    if (!QueryFullProcessImageNameW(process, 0, path, &length)) return false;
    const QString image = QDir::fromNativeSeparators(QString::fromWCharArray(path, length));
    return image.compare(QDir::fromNativeSeparators(expected.path), Qt::CaseInsensitive) == 0
        && TrustedCore::protectedExecutable(image) && matchesFile(image, expected.digest);
}

int labRecoveryRequired() {
    std::puts("lab_recovery_required");
    MessageBoxW(nullptr, L"The Desktop update outcome or restart could not be verified. "
                          L"Check the installer outcome before restarting Desktop.",
                L"Bimeron Desktop update lab", MB_OK | MB_ICONWARNING);
    return 6;
}

int runLab(const QJsonObject &receipt, const Identity &old, const Identity &next, const QString &mode,
           const QString &originValue, const QString &channel) {
    // Execution fixtures must be installed by the lab operator under a
    // protected path outside the product directory. The default verifier
    // accepts workspace fixtures for read-only inventory only.
    const QString self = QDir::cleanPath(QCoreApplication::applicationFilePath());
    const QString root = QFileInfo(old.path).absolutePath() + '/';
    const QString targetRoot = QFileInfo(next.path).absolutePath() + '/';
    const QString msi = QDir::cleanPath(QDir::fromNativeSeparators(receipt.value("msi_path").toString()));
    if (receipt.value("allow_execution") != QJsonValue(true) || !unelevatedInteractiveSession()
        || self.startsWith(root, Qt::CaseInsensitive) || self.startsWith(targetRoot, Qt::CaseInsensitive)
        || msi.startsWith(root, Qt::CaseInsensitive) || msi.startsWith(targetRoot, Qt::CaseInsensitive)
        || !TrustedCore::protectedExecutable(self) || !TrustedCore::protectedExecutable(msi)) return 5;
    QLockFile operation(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                        .filePath("bimeron-msi-handoff-lab.lock"));
    operation.setStaleLockTime(0);
    const auto origin = MsiHandoff::inheritedHandle(originValue);
    if (!operation.tryLock(0) || !authenticatedOrigin(origin.get(), old)
        || !installedMatches(old) || installedMatches(next) || rebootPending()) return 5;
    static const QRegularExpression pipeName(QStringLiteral(R"(\ABimeronDesktopHandoffLab-[0-9a-f-]{36}\z)"));
    if (!pipeName.match(channel).hasMatch()) return 5;
    QLocalSocket socket;
    socket.connectToServer(channel);
    ULONG issuer = 0;
    if (!socket.waitForConnected(3000)
        || !GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(socket.socketDescriptor()), &issuer)
        || issuer != GetProcessId(origin.get()) || socket.write("R", 1) != 1
        || (socket.bytesToWrite() && !socket.waitForBytesWritten(1000))
        || (socket.bytesAvailable() == 0 && !socket.waitForReadyRead(10000))
        || socket.read(1) != "C") return 5;
    socket.disconnectFromServer();
    if (WaitForSingleObject(origin.get(), 15000) != WAIT_OBJECT_0 || !InstallerPreflight::desktopIsClosed()
        || !installedMatches(old) || installedMatches(next) || rebootPending()) {
        return labRecoveryRequired();
    }
    QElapsedTimer elapsed;
    elapsed.start();
    const auto startedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toLatin1();
    const auto installer = startInstaller(msi);
    if (!installer || WaitForSingleObject(installer.get(), INFINITE) != WAIT_OBJECT_0) {
        return labRecoveryRequired();
    }
    const auto result = MsiHandoff::observedExit(installer.get());
    const bool oldVerified = installedMatches(old);
    const bool newVerified = installedMatches(next);
    const bool pending = rebootPending();
    if (result) {
        const auto terminalUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toLatin1();
        std::printf("lab_msi_terminal exit=%lu started_utc=%s observed_utc=%s elapsed_ms=%lld old_verified=%d new_verified=%d reboot_pending=%d\n",
                    *result, startedUtc.constData(), terminalUtc.constData(), static_cast<long long>(elapsed.elapsed()),
                    int(oldVerified), int(newVerified), int(pending));
    }
    const auto outcome = MsiHandoff::select(result, oldVerified, newVerified, pending);
    if (outcome == MsiHandoff::Outcome::WaitForReboot) {
        std::puts("lab_wait_reboot");
        return 7;
    }
    const bool launchNew = outcome == MsiHandoff::Outcome::LaunchNew;
    if ((launchNew || outcome == MsiHandoff::Outcome::LaunchOld)
        && launchDesktop(launchNew ? next : old, mode)) {
        std::puts(launchNew ? "lab_new_gui_acknowledged" : "lab_old_gui_acknowledged");
        return 0;
    }
    return labRecoveryRequired();
}
#endif
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
#ifdef BIMERON_MSI_HANDOFF_LAB_EXECUTION
    const QString mode = arguments.size() == 4 && arguments[1] == "--from-desktop-window" ? "window"
        : arguments.size() == 4 && arguments[1] == "--from-desktop-tray" ? "tray" : QString{};
    if (arguments.size() != 1 && mode.isEmpty()) return 2;
#else
    if (arguments.size() != 1) return 2;
#endif
    QFile file(":/handoff/receipt.json");
    if (!file.open(QIODevice::ReadOnly)) return 3;
    const auto receipt = QJsonDocument::fromJson(file.readAll()).object();
    const auto old = identity(receipt.value("old_gui").toObject());
    const auto next = identity(receipt.value("new_gui").toObject());
    const auto verifiedMsi = validIdentity(old) && validIdentity(next) && old.product != next.product
        && old.package != next.package ? verifiedOffer(receipt, next) : InstallerPreflight::FileHandle{};
    if (!verifiedMsi) {
        std::puts("lab_offer_refused");
        return 4;
    }
#ifdef BIMERON_MSI_HANDOFF_LAB_EXECUTION
    if (!mode.isEmpty()) return runLab(receipt, old, next, mode, arguments[2], arguments[3]);
#endif
    std::puts("lab_offer_verified");
    std::puts(installedMatches(old) ? "old_registered_image_verified" : "old_registered_image_unverified");
    std::puts(installedMatches(next) ? "new_registered_image_verified" : "new_registered_image_unverified");
    return 0;
}
