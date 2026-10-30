#ifdef FOREGROUND_EXPORTS
#include "foreground.h"
#else
#include "callback-context.h"
#endif
#include <sddl.h>
#include <cstddef>
#include <vector>

namespace foreground {
bool openSelectedContextToken(HANDLE& token, TokenSource& source, DWORD& status, DWORD access) {
    if (OpenThreadToken(GetCurrentThread(), access, TRUE, &token)) {
        source = TokenSource::Thread; return true;
    }
    status = GetLastError();
    if (status != ERROR_NO_TOKEN) return false;
    if (!OpenProcessToken(GetCurrentProcess(), access, &token)) {
        status = GetLastError(); return false;
    }
    source = TokenSource::Process; status = 0; return true;
}
static bool sameLuid(LUID a, LUID b) {
    return a.LowPart == b.LowPart && a.HighPart == b.HighPart;
}
ContextRefusal callbackContextRefusalFixture(const CallbackInteractiveContextObservation& o) {
    if (!o.queried || o.status != 0 || (o.source != TokenSource::Thread && o.source != TokenSource::Process)) return ContextRefusal::TokenQuery;
    if ((o.type != TokenPrimary && o.type != TokenImpersonation) ||
        (o.type == TokenImpersonation && (o.impersonation < SecurityImpersonation ||
                                         o.impersonation > SecurityDelegation)) ||
        (o.elevationType != TokenElevationTypeDefault && o.elevationType != TokenElevationTypeFull) ||
        !o.elevated || !o.enabledAdmin || o.adminDenyOnly || o.restricted || o.appContainer ||
        o.integrity < SECURITY_MANDATORY_HIGH_RID) return ContextRefusal::Authority;
    if (o.servicePrincipal || o.sid.empty() || o.sid == L"S-1-5-18" || o.sid == L"S-1-5-19" ||
        o.sid == L"S-1-5-20" || o.sid.compare(0, 9, L"S-1-5-80-") == 0 || o.sid.compare(0, 9, L"S-1-5-82-") == 0 ||
        !o.session || (!o.authenticationId.LowPart && !o.authenticationId.HighPart))
        return ContextRefusal::Principal;
    if (o.lsaStatus != 0) return ContextRefusal::LsaQuery;
    if (!o.lsaDataValid || o.sid != o.lsaSid || o.session != o.lsaSession ||
        !sameLuid(o.authenticationId, o.lsaAuthenticationId)) return ContextRefusal::LsaData;
    if (o.logonType != 2 && o.logonType != 10) return ContextRefusal::LogonType;
    return ContextRefusal::None;
}
// Validate every pointer-bearing token result against the buffer actually returned.
static bool contained(const std::vector<BYTE>& b, const void* p, size_t n) {
    const auto start = reinterpret_cast<ULONG_PTR>(b.data());
    const auto at = reinterpret_cast<ULONG_PTR>(p);
    return at >= start && at - start <= b.size() && n <= b.size() - (at - start);
}
static bool boundedSid(const std::vector<BYTE>& b, PSID sid) {
    if (!contained(b, sid, 8)) return false;
    const auto count = *GetSidSubAuthorityCount(sid);
    return count <= SID_MAX_SUB_AUTHORITIES &&
        contained(b, sid, GetSidLengthRequired(count)) && IsValidSid(sid);
}
static bool sidText(PSID sid, std::wstring& out) {
    LPWSTR text = nullptr;
    if (!sid || !IsValidSid(sid) || GetLengthSid(sid) > SECURITY_MAX_SID_SIZE ||
        !ConvertSidToStringSidW(sid, &text)) return false;
    out = text; LocalFree(text); return true;
}
void captureLogonDataFixture(CallbackInteractiveContextObservation& o,
                             const SECURITY_LOGON_SESSION_DATA* data) {
    o.lsaDataValid = false; o.lsaSid.clear(); o.lsaSession = 0;
    o.lsaAuthenticationId = {}; o.logonType = 0;
    if (!data || data->Size < offsetof(SECURITY_LOGON_SESSION_DATA, Sid) + sizeof(data->Sid) ||
        data->Size > 65536 || !sidText(data->Sid, o.lsaSid)) return;
    o.lsaDataValid = true; o.lsaAuthenticationId = data->LogonId;
    o.lsaSession = data->Session; o.logonType = data->LogonType;
}
void captureSelectedContext(HANDLE token, CallbackInteractiveContextObservation& o) {
    auto read = [&](TOKEN_INFORMATION_CLASS kind, void* p, DWORD size, DWORD minimum = 0) {
        DWORD returned = 0;
        if (!GetTokenInformation(token, kind, p, size, &returned)) {
            o.status = GetLastError(); return false;
        }
        if (returned > size || returned < (minimum ? minimum : size)) { o.status = ERROR_INVALID_DATA; return false; }
        return true;
    };
    auto variable = [&](TOKEN_INFORMATION_CLASS kind, std::vector<BYTE>& bytes) {
        DWORD size = 0;
        if (GetTokenInformation(token, kind, nullptr, 0, &size) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || !size || size > 65536) {
            o.status = ERROR_INVALID_DATA; return false;
        }
        bytes.resize(size); DWORD returned = 0;
        if (!GetTokenInformation(token, kind, bytes.data(), size, &returned)) {
            o.status = GetLastError(); return false;
        }
        if (returned > size) { o.status = ERROR_INVALID_DATA; return false; }
        bytes.resize(returned); return true;
    };
    TOKEN_ELEVATION elevation{}; TOKEN_STATISTICS stats{};
    DWORD restricted = 0, app = 0;
    std::vector<BYTE> user, groups, integrity;
    if (!read(TokenElevation, &elevation, sizeof(elevation)) ||
        !read(TokenElevationType, &o.elevationType, sizeof(o.elevationType)) ||
        !read(TokenStatistics, &stats, sizeof(stats)) ||
        !read(TokenSessionId, &o.session, sizeof(o.session)) ||
        !read(TokenHasRestrictions, &restricted, sizeof(restricted), 1) ||
        !read(TokenIsAppContainer, &app, sizeof(app)) ||
        !variable(TokenUser, user) || !variable(TokenGroups, groups) ||
        !variable(TokenIntegrityLevel, integrity)) return;
    o.status = ERROR_INVALID_DATA;
    if (user.size() < sizeof(TOKEN_USER) || groups.size() < offsetof(TOKEN_GROUPS, Groups) ||
        integrity.size() < sizeof(TOKEN_MANDATORY_LABEL)) return;
    auto u = reinterpret_cast<TOKEN_USER*>(user.data());
    auto g = reinterpret_cast<TOKEN_GROUPS*>(groups.data());
    auto label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(integrity.data())->Label.Sid;
    if (!boundedSid(user, u->User.Sid) || !sidText(u->User.Sid, o.sid) ||
        !boundedSid(integrity, label) || !*GetSidSubAuthorityCount(label) ||
        g->GroupCount > (groups.size() - offsetof(TOKEN_GROUPS, Groups)) / sizeof(SID_AND_ATTRIBUTES)) return;
    for (DWORD n = 0; n < g->GroupCount; ++n) {
        const auto& a = g->Groups[n];
        if (!boundedSid(groups, a.Sid)) return;
        if (IsWellKnownSid(a.Sid, WinServiceSid)) o.servicePrincipal = true;
        if (IsWellKnownSid(a.Sid, WinBuiltinAdministratorsSid)) {
            o.adminDenyOnly = (a.Attributes & SE_GROUP_USE_FOR_DENY_ONLY) != 0;
            o.enabledAdmin = (a.Attributes & SE_GROUP_ENABLED) != 0 && !o.adminDenyOnly;
        }
    }
    o.integrity = *GetSidSubAuthority(label, *GetSidSubAuthorityCount(label) - 1);
    o.type = stats.TokenType; o.impersonation = stats.ImpersonationLevel;
    o.authenticationId = stats.AuthenticationId; o.selectedTokenId = stats.TokenId;
    o.elevated = elevation.TokenIsElevated != 0;
    o.restricted = restricted != 0 || IsTokenRestricted(token) != FALSE;
    o.appContainer = app != 0; o.queried = true; o.status = 0;
    PSECURITY_LOGON_SESSION_DATA data = nullptr;
    o.lsaStatus = LsaGetLogonSessionData(&stats.AuthenticationId, &data);
    if (o.lsaStatus == 0 && data) {
        // LSA owns the returned allocation and SID. Size bounds the structure;
        // SID validity/maximum size bounds SID decoding, not an invented allocation size.
        captureLogonDataFixture(o, data);
    }
    if (data) LsaFreeReturnBuffer(data);
    o.refusal = callbackContextRefusalFixture(o);
}
CallbackInteractiveContextObservation observeSelectedContext() {
    CallbackInteractiveContextObservation o;
    SelectedContextToken token;
    if (!openSelectedContextToken(token.value, o.source, o.status)) {
        o.refusal = ContextRefusal::TokenOpen; return o;
    }
    captureSelectedContext(token.value, o);
    return o;
}
}
