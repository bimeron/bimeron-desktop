// LAB TRACE ONLY: observations are never enrollment or initiating-client proof.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <bcrypt.h>
#include <cstdio>
#include <vector>

#ifndef LAB_FAILPOINT
#define LAB_FAILPOINT 0
#endif
namespace {
void incomplete(MSIHANDLE install, const wchar_t* callback, UINT intended, DWORD status) noexcept {
    wchar_t text[384]{};
    if (swprintf_s(text, L"BIMERON_LAB_TRACE_V1 {\"schema\":1,\"callback\":\"%s\",\"evidence\":\"incomplete\",\"status\":%lu,\"initiatingClient\":\"Unknown\",\"intendedReturn\":%u}", callback, status, intended) <= 0) return;
    MSIHANDLE record = MsiCreateRecord(0);
    if (record) {
        if (MsiRecordSetStringW(record, 0, text) == ERROR_SUCCESS)
            MsiProcessMessage(install, INSTALLMESSAGE_INFO, record);
        MsiCloseHandle(record);
    }
}
struct Observation {
    DWORD error = 0;
    void failed(DWORD e) { if (!error) error = e ? e : ERROR_INVALID_DATA; }
    bool query(HANDLE token, TOKEN_INFORMATION_CLASS kind, void* p, DWORD size) {
        DWORD required = 0;
        if (GetTokenInformation(token, kind, p, size, &required)) return true;
        failed(GetLastError()); return false;
    }
    bool variable(HANDLE token, TOKEN_INFORMATION_CLASS kind, std::vector<BYTE>& data) {
        DWORD size = 0;
        if (GetTokenInformation(token, kind, nullptr, 0, &size) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || size > 65536 || !size) {
            failed(ERROR_INVALID_DATA); return false;
        }
        data.resize(size); return query(token, kind, data.data(), size);
    }
};
UINT observe(MSIHANDLE install, const wchar_t* callback, int point) {
    const UINT intended = LAB_FAILPOINT == point && point != 0 ? ERROR_INSTALL_FAILURE : ERROR_SUCCESS;
    Observation o;
    SYSTEMTIME utc{}; GetSystemTime(&utc);
    FILETIME created{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exit, &kernel, &user)) o.failed(GetLastError());
    DWORD processSession = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &processSession)) o.failed(GetLastError());
    HANDLE token = nullptr; DWORD source = 1;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
        DWORD e = GetLastError(); source = 0;
        if (e != ERROR_NO_TOKEN) o.failed(e);
        else { source = 2; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) o.failed(GetLastError()); }
    }
    TOKEN_STATISTICS stats{}; TOKEN_ELEVATION elevation{};
    TOKEN_ELEVATION_TYPE elevationType{}; DWORD session = 0, restricted = 0;
    DWORD integrity = 0, adminEnabled = 0, adminDeny = 0;
    wchar_t sidHash[65] = L"unknown";
    if (token) {
        o.query(token, TokenStatistics, &stats, sizeof(stats));
        o.query(token, TokenElevation, &elevation, sizeof(elevation));
        o.query(token, TokenElevationType, &elevationType, sizeof(elevationType));
        o.query(token, TokenSessionId, &session, sizeof(session));
        o.query(token, TokenHasRestrictions, &restricted, sizeof(restricted));
        std::vector<BYTE> sid, groups, label;
        if (o.variable(token, TokenUser, sid)) {
            PSID s = reinterpret_cast<TOKEN_USER*>(sid.data())->User.Sid;
            if (!IsValidSid(s)) o.failed(ERROR_INVALID_SID);
            else {
                BYTE hash[32]{};
                NTSTATUS status = BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                    static_cast<PUCHAR>(s), GetLengthSid(s), hash, sizeof(hash));
                if (status < 0) o.failed(static_cast<DWORD>(status));
                else for (unsigned i = 0; i < 32; ++i) swprintf_s(sidHash + i*2, 3, L"%02x", hash[i]);
            }
        }
        if (o.variable(token, TokenGroups, groups)) {
            auto g = reinterpret_cast<TOKEN_GROUPS*>(groups.data());
            for (DWORD i = 0; i < g->GroupCount; ++i) {
                auto& a = g->Groups[i];
                if (IsWellKnownSid(a.Sid, WinBuiltinAdministratorsSid)) {
                    adminDeny = !!(a.Attributes & SE_GROUP_USE_FOR_DENY_ONLY);
                    adminEnabled = !!(a.Attributes & SE_GROUP_ENABLED) && !adminDeny;
                }
            }
        }
        if (o.variable(token, TokenIntegrityLevel, label)) {
            PSID s = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(label.data())->Label.Sid;
            if (!IsValidSid(s) || !*GetSidSubAuthorityCount(s)) o.failed(ERROR_INVALID_SID);
            else integrity = *GetSidSubAuthority(s, *GetSidSubAuthorityCount(s)-1);
        }
        CloseHandle(token);
    }
    wchar_t message[1536]{};
    int n = swprintf_s(message, L"BIMERON_LAB_TRACE_V1 {\"schema\":1,\"callback\":\"%s\",\"utc\":\"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\",\"pid\":%lu,\"tid\":%lu,\"processCreated\":\"%08lx%08lx\",\"processSession\":%lu,\"tokenSource\":%lu,\"tokenType\":%u,\"impersonation\":%d,\"elevated\":%lu,\"elevationType\":%u,\"integrityRID\":%lu,\"tokenSession\":%lu,\"adminEnabled\":%lu,\"adminDenyOnly\":%lu,\"restricted\":%lu,\"sidSHA256\":\"%s\",\"logonId\":\"%08lx%08lx\",\"status\":%lu,\"evidence\":\"%s\",\"initiatingClient\":\"Unknown\",\"intendedReturn\":%u}",
        callback, utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond, utc.wMilliseconds,
        GetCurrentProcessId(), GetCurrentThreadId(), created.dwHighDateTime, created.dwLowDateTime,
        processSession, source, stats.TokenType, stats.TokenType == TokenImpersonation ? static_cast<int>(stats.ImpersonationLevel) : -1,
        elevation.TokenIsElevated, elevationType, integrity, session, adminEnabled, adminDeny, restricted,
        sidHash, static_cast<DWORD>(stats.AuthenticationId.HighPart), stats.AuthenticationId.LowPart,
        o.error, o.error ? L"incomplete" : L"observed", intended);
    MSIHANDLE record = MsiCreateRecord(0);
    bool logged = n > 0 && record && MsiRecordSetStringW(record, 0, message) == ERROR_SUCCESS;
    if (logged) logged = MsiProcessMessage(install, INSTALLMESSAGE_INFO, record) == 0;
    if (record) MsiCloseHandle(record);
    if (!logged) incomplete(install, callback, intended, ERROR_WRITE_FAULT);
    return intended;
}
UINT safeObserve(MSIHANDLE h, const wchar_t* name, int point) noexcept {
    try { return observe(h, name, point); }
    catch (...) {
        const UINT intended = LAB_FAILPOINT == point && point ? ERROR_INSTALL_FAILURE : ERROR_SUCCESS;
        incomplete(h, name, intended, ERROR_NOT_ENOUGH_MEMORY);
        return intended;
    }
}
}
#define WIDE_(s) L##s
#define WIDEN(s) WIDE_(s)
#define LAB_CALLBACK(name, point) extern "C" __declspec(dllexport) UINT __stdcall name(MSIHANDLE h) { return safeObserve(h, WIDEN(#name), point); }
LAB_CALLBACK(ObservationPreflight, 0)
LAB_CALLBACK(ObservationRollback, 0)
LAB_CALLBACK(ObservationDeferredBegin, 0)
LAB_CALLBACK(ObservationCommit, 1)
LAB_CALLBACK(ObservationPostFinalize, 2)
