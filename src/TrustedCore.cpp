#include "TrustedCore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <aclapi.h>
#include <sddl.h>
#else
#include <sys/stat.h>
#endif

namespace {
#ifdef Q_OS_WIN
bool trustedSid(PSID sid) {
    BYTE systemBuffer[SECURITY_MAX_SID_SIZE], adminBuffer[SECURITY_MAX_SID_SIZE];
    DWORD systemSize = sizeof(systemBuffer), adminSize = sizeof(adminBuffer);
    PSID installer = nullptr;
    const bool known = CreateWellKnownSid(WinLocalSystemSid, nullptr, systemBuffer, &systemSize)
        && CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, adminBuffer, &adminSize)
        && ConvertStringSidToSidW(L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464", &installer);
    const bool result = known && (EqualSid(sid, systemBuffer) || EqualSid(sid, adminBuffer) || EqualSid(sid, installer));
    if (installer) LocalFree(installer);
    return result;
}

bool protectedComponent(const QString &path, bool packageComponent) {
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    const DWORD attributes = GetFileAttributesW(native.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    PSID owner = nullptr;
    PACL acl = nullptr;
    if (GetNamedSecurityInfoW(const_cast<wchar_t *>(native.c_str()), SE_FILE_OBJECT,
                              OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                              &owner, nullptr, &acl, nullptr, &descriptor) != ERROR_SUCCESS) return false;
    bool safe = owner && trustedSid(owner) && acl;
    constexpr DWORD replacementRights = GENERIC_ALL | FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER;
    constexpr DWORD contentRights = GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA |
        FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES;
    const DWORD writeRights = replacementRights | (packageComponent ? contentRights : 0);
    for (DWORD i = 0; safe && i < acl->AceCount; ++i) {
        void *entry = nullptr;
        if (!GetAce(acl, i, &entry)) { safe = false; break; }
        const auto *header = static_cast<ACE_HEADER *>(entry);
        if (header->AceFlags & INHERIT_ONLY_ACE || header->AceType == ACCESS_DENIED_ACE_TYPE) continue;
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) { safe = false; break; }
        const auto *allow = static_cast<ACCESS_ALLOWED_ACE *>(entry);
        if ((allow->Mask & writeRights) && !trustedSid(const_cast<DWORD *>(&allow->SidStart))) safe = false;
    }
    LocalFree(descriptor);
    return safe;
}
#endif
}

bool TrustedCore::protectedExecutable(const QString &path) {
    const QFileInfo file(path);
    if (!file.isAbsolute() || !file.exists() || !file.isFile() || file.isSymLink()) return false;
    QString current = QDir::cleanPath(file.absoluteFilePath());
    const QString packageDirectory = QDir::cleanPath(file.absolutePath());
    while (!current.isEmpty()) {
#ifdef Q_OS_WIN
        if (!protectedComponent(current, current == QDir::cleanPath(file.absoluteFilePath()) ||
                                         current == packageDirectory)) return false;
#else
        struct stat info{};
        const QByteArray encoded = QFile::encodeName(current);
        if (lstat(encoded.constData(), &info) != 0 || info.st_uid != 0 ||
            (info.st_mode & (S_IWGRP | S_IWOTH)) || S_ISLNK(info.st_mode) ||
            (current == file.absoluteFilePath() ? !S_ISREG(info.st_mode) : !S_ISDIR(info.st_mode))) return false;
#endif
        const QString parent = QFileInfo(current).dir().absolutePath();
        if (parent == current) break;
        current = parent;
    }
    return true;
}

bool TrustedCore::supportsSourceUpdate(const QString &path) {
    QProcess process;
    process.start(path, {QStringLiteral("service")});
    if (!process.waitForStarted(5000) || !process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    const QByteArray usage = process.readAllStandardError() + process.readAllStandardOutput();
    return usage.contains("update -source <absolute-path> -version <version> -sha256 <executable-sha256>");
}

QStringList TrustedCore::updateArguments(const QString &source, const QString &version, const QString &sha256) {
    if (!QFileInfo(source).isAbsolute()) return {};
    return {QStringLiteral("service"), QStringLiteral("update"), QStringLiteral("-source"), source,
            QStringLiteral("-version"), version, QStringLiteral("-sha256"), sha256};
}

QString TrustedCore::windowsCommandLine(const QStringList &arguments) {
    QStringList quoted;
    for (const QString &argument : arguments) {
        QString part = QStringLiteral("\"");
        int slashes = 0;
        for (const QChar ch : argument) {
            if (ch == QLatin1Char('\\')) { ++slashes; continue; }
            if (ch == QLatin1Char('"')) {
                part += QString(slashes * 2 + 1, QLatin1Char('\\'));
                part += ch;
            } else {
                part += QString(slashes, QLatin1Char('\\'));
                part += ch;
            }
            slashes = 0;
        }
        part += QString(slashes * 2, QLatin1Char('\\')) + QLatin1Char('"');
        quoted.append(part);
    }
    return quoted.join(QLatin1Char(' '));
}

QString TrustedCore::failureMessage(int exitCode, const QString &details) {
    if (!details.isEmpty()) return details;
    return QStringLiteral("Core update failed (exit code %1). Checking service state. If IPC hello failed, the updater attempted rollback.")
        .arg(exitCode);
}
