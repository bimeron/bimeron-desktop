#include "foreground.h"
#include <aclapi.h>
#include <sddl.h>
#include <vector>
#include <msi.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

namespace foreground {
static bool trusted(PSID sid) {
    return sid && (IsWellKnownSid(sid, WinLocalSystemSid) ||
                   IsWellKnownSid(sid, WinBuiltinAdministratorsSid));
}
bool safeMutexDescriptor(PSECURITY_DESCRIPTOR sd) {
    PSID owner = nullptr;
    PACL acl = nullptr;
    BOOL present = FALSE, unused = FALSE;
    if (!sd || !IsValidSecurityDescriptor(sd) ||
        !GetSecurityDescriptorOwner(sd, &owner, &unused) || !trusted(owner) ||
        !GetSecurityDescriptorDacl(sd, &present, &acl, &unused) || !present || !acl ||
        !IsValidAcl(acl)) return false;
    for (DWORD i = 0; i < acl->AceCount; ++i) {
        void* raw = nullptr;
        if (!GetAce(acl, i, &raw)) return false;
        auto header = static_cast<ACE_HEADER*>(raw);
        if (header->AceFlags & INHERIT_ONLY_ACE) continue;
        if (header->AceType == ACCESS_DENIED_ACE_TYPE) continue;
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) return false;
        auto ace = static_cast<ACCESS_ALLOWED_ACE*>(raw);
        if (!trusted(&ace->SidStart) && (ace->Mask & ~(READ_CONTROL | SYNCHRONIZE)))
            return false;
    }
    return true;
}
bool markerAbsent(const wchar_t* marker) {
    HANDLE owner = OpenMutexW(SYNCHRONIZE, FALSE, marker);
    DWORD error = GetLastError();
    if (owner) CloseHandle(owner);
    return !owner && error == ERROR_FILE_NOT_FOUND;
}
bool admit(HANDLE mutex, const wchar_t* marker) {
    PSECURITY_DESCRIPTOR sd = nullptr;
    DWORD error = GetSecurityInfo(mutex, SE_KERNEL_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        nullptr, nullptr, nullptr, nullptr, &sd);
    bool safe = error == ERROR_SUCCESS && safeMutexDescriptor(sd);
    if (sd) LocalFree(sd);
    if (!safe) return false;
    DWORD wait = WaitForSingleObject(mutex, 0);
    if (wait == WAIT_ABANDONED) { ReleaseMutex(mutex); return false; }
    if (wait != WAIT_OBJECT_0) return false;
    if (markerAbsent(marker)) return true;
    ReleaseMutex(mutex);
    return false;
}

struct Worker::State {
    HANDLE mutex;
    std::wstring journal;
    std::string metadata;
    std::thread thread;
    std::mutex lock;
    std::mutex callers;
    std::condition_variable cv;
    enum Command { idle, begin, fixtureAcknowledge, incomplete, quit } command = idle;
    bool done = false, result = false, held = false, started = false;
    DWORD owner = 0;
    HMODULE module = nullptr;

    bool prepare() {
        // Do not retry abandonment; it grants ownership and must be released.
        DWORD wait = WaitForSingleObject(mutex, 0);
        if (wait == WAIT_ABANDONED) { ReleaseMutex(mutex); return false; }
        if (wait != WAIT_OBJECT_0) return false;
        held = true;
        HANDLE file = CreateFileW(journal.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        std::string record = "foreground-prototype-pending-v1\n" + metadata;
        bool ok = WriteFile(file, record.data(), static_cast<DWORD>(record.size()),
                            &written, nullptr) && written == record.size() &&
                  FlushFileBuffers(file);
        CloseHandle(file);
        // Any partial record remains and refuses the next begin.
        return ok;
    }
    void run() {
        std::unique_lock<std::mutex> guard(lock);
        owner = GetCurrentThreadId();
        for (;;) {
            cv.wait(guard, [&] { return command != idle; });
            Command next = command;
            result = false;
            if (next == begin && !started) {
                started = true;
                result = prepare();
                if (!result && held) { ReleaseMutex(mutex); held = false; }
            } else if ((next == fixtureAcknowledge) && held) {
                // Bounded offline PostFinalize fixture only. No protected publication
                // or actual rollback occurs. Failed deletion retains pending.
                result = DeleteFileW(journal.c_str()) != FALSE;
                if (result) { ReleaseMutex(mutex); held = false; }
            }
            if (next == quit && held) {
                // Host teardown is NOT successful settlement. Preserve journal.
                ReleaseMutex(mutex);
                held = false;
            }
            command = idle;
            done = true;
            cv.notify_all();
            if (next == quit) break;
        }
        // Process-lifetime module pin protects even the worker/destructor tail.
        // It does not keep an MSI server process alive.
    }
    bool send(Command next) {
        std::lock_guard<std::mutex> serialize(callers);
        std::unique_lock<std::mutex> guard(lock);
        cv.wait(guard, [&] { return command == idle; });
        done = false;
        command = next;
        cv.notify_all();
        cv.wait(guard, [&] { return done; });
        return result;
    }
};
Worker::Worker(HANDLE mutex, std::wstring journal, std::string metadata)
    : state(std::make_unique<State>()) {
    state->mutex = mutex;
    state->journal = std::move(journal);
    state->metadata = std::move(metadata);
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&safeMutexDescriptor), &state->module))
        throw std::runtime_error("cannot pin prototype DLL");
    state->thread = std::thread([this] { state->run(); });
}
Worker::~Worker() {
    state->send(State::quit);
    state->thread.join();
}
bool Worker::begin() { return state->send(State::begin); }
bool Worker::settleFixture(const SettlementFixture& evidence) {
    return state->send(settledFixture(evidence) ? State::fixtureAcknowledge : State::incomplete);
}
DWORD Worker::ownerThread() const {
    std::lock_guard<std::mutex> guard(state->lock);
    return state->owner;
}
}

// Deliberate hard gate: no accepted final Core artifacts, secure cross-process
// protocol, or proven MSI transaction lifetime. Never authorize a real install.
extern "C" __declspec(dllexport) UINT __stdcall ForegroundPrototype(MSIHANDLE) {
    return ERROR_INSTALL_FAILURE;
}

namespace foreground {
TokenFixture effectiveTokenFixture(ThreadTokenResult result,
                                  const TokenFixture& thread, const TokenFixture& process) {
    if (result == ThreadTokenResult::Present) return thread;
    if (result == ThreadTokenResult::NoToken) return process;
    return {};
}
bool eligibleContextFixture(const TokenFixture& e, const TokenFixture& i) {
    auto full = [](const TokenFixture& t) {
        return t.observed && t.interactive && t.elevated && t.enabledAdmin &&
            !t.restricted && t.impersonationAdequate &&
            t.integrity >= SECURITY_MANDATORY_HIGH_RID && t.session != 0 &&
            !t.sid.empty() && (t.logon.LowPart || t.logon.HighPart);
    };
    return full(e) && full(i) && e.sid == i.sid && e.session == i.session &&
        e.logon.LowPart == i.logon.LowPart && e.logon.HighPart == i.logon.HighPart;
}
bool settledFixture(const SettlementFixture& e) {
    return e.callback == Callback::PostFinalize && e.pending &&
        e.pendingGeneration != 0 && e.pendingGeneration == e.observedGeneration &&
        e.inventory && e.outerCompletion && e.protection && e.quiescent &&
        e.flushed && e.hostAlive && eligibleContextFixture(e.effective, e.initiating);
}
ClientCorrelation verifyInitiatingClient() { return ClientCorrelation::Unknown; }

bool safeJournalDescriptor(PSECURITY_DESCRIPTOR sd) {
    PSID owner = nullptr; PACL acl = nullptr; BOOL present = FALSE, ignored = FALSE;
    SECURITY_DESCRIPTOR_CONTROL control{}; DWORD revision = 0;
    if (!sd || !IsValidSecurityDescriptor(sd) ||
        !GetSecurityDescriptorOwner(sd, &owner, &ignored) || !trusted(owner) ||
        !GetSecurityDescriptorControl(sd, &control, &revision) ||
        !(control & SE_DACL_PROTECTED) ||
        !GetSecurityDescriptorDacl(sd, &present, &acl, &ignored) || !present ||
        !acl || !IsValidAcl(acl)) return false;
    GENERIC_MAPPING mapping{KEY_READ, KEY_WRITE, KEY_EXECUTE, KEY_ALL_ACCESS};
    constexpr DWORD mutation = KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK |
        DELETE | WRITE_DAC | WRITE_OWNER;
    for (DWORD n = 0; n < acl->AceCount; ++n) {
        void* raw = nullptr;
        if (!GetAce(acl, n, &raw)) return false;
        auto h = static_cast<ACE_HEADER*>(raw);
        if (h->AceFlags & INHERIT_ONLY_ACE) continue;
        if (h->AceType == ACCESS_DENIED_ACE_TYPE) continue;
        if (h->AceType != ACCESS_ALLOWED_ACE_TYPE) return false;
        auto a = static_cast<ACCESS_ALLOWED_ACE*>(raw);
        DWORD rights = a->Mask; MapGenericMask(&rights, &mapping);
        if (!trusted(&a->SidStart) && (rights & mutation)) return false;
    }
    return true;
}
bool journalAccessFixture(PSECURITY_DESCRIPTOR sd, HANDLE token, bool native64, bool ancestors) {
    if (!native64 || !ancestors || !safeJournalDescriptor(sd)) return false;
    GENERIC_MAPPING mapping{KEY_READ, KEY_WRITE, KEY_EXECUTE, KEY_ALL_ACCESS};
    PRIVILEGE_SET privileges{}; DWORD size = sizeof(privileges), granted = 0;
    BOOL access = FALSE;
    // AccessCheck requires an impersonation token supplied by the offline fixture.
    return AccessCheck(sd, token, KEY_SET_VALUE, &mapping, &privileges, &size,
                       &granted, &access) && access;
}
bool readEffectiveToken(TokenFixture& result) {
    result = {};
    HANDLE token = nullptr;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
        if (GetLastError() != ERROR_NO_TOKEN ||
            !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    }
    auto read = [&](TOKEN_INFORMATION_CLASS kind, void* buffer, DWORD size) {
        DWORD required = 0;
        return GetTokenInformation(token, kind, buffer, size, &required) != FALSE;
    };
    auto variable = [&](TOKEN_INFORMATION_CLASS kind, std::vector<BYTE>& bytes) {
        DWORD size = 0;
        if (GetTokenInformation(token, kind, nullptr, 0, &size) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER || size > 65536) return false;
        bytes.resize(size); return read(kind, bytes.data(), size);
    };
    TOKEN_ELEVATION elevation{}; TOKEN_STATISTICS stats{}; DWORD session = 0;
    DWORD appContainer = 0, restrictions = 0;
    std::vector<BYTE> user, groups, integrity;
    bool ok = read(TokenElevation, &elevation, sizeof(elevation)) &&
        read(TokenStatistics, &stats, sizeof(stats)) &&
        read(TokenSessionId, &session, sizeof(session)) &&
        read(TokenIsAppContainer, &appContainer, sizeof(appContainer)) &&
        read(TokenHasRestrictions, &restrictions, sizeof(restrictions)) &&
        variable(TokenUser, user) && variable(TokenGroups, groups) &&
        variable(TokenIntegrityLevel, integrity);
    if (ok) {
        auto u = reinterpret_cast<TOKEN_USER*>(user.data());
        LPWSTR sid = nullptr;
        ok = ConvertSidToStringSidW(u->User.Sid, &sid) != FALSE;
        if (ok) { result.sid = sid; LocalFree(sid); }
        auto g = reinterpret_cast<TOKEN_GROUPS*>(groups.data());
        for (DWORD n = 0; n < g->GroupCount; ++n) {
            auto& a = g->Groups[n];
            bool enabled = (a.Attributes & SE_GROUP_ENABLED) &&
                !(a.Attributes & SE_GROUP_USE_FOR_DENY_ONLY);
            if (enabled && IsWellKnownSid(a.Sid, WinBuiltinAdministratorsSid)) result.enabledAdmin = true;
            if (enabled && IsWellKnownSid(a.Sid, WinInteractiveSid)) result.interactive = true;
        }
        auto label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(integrity.data())->Label.Sid;
        if (!IsValidSid(label) || !*GetSidSubAuthorityCount(label)) ok = false;
        else result.integrity = *GetSidSubAuthority(label, *GetSidSubAuthorityCount(label) - 1);
        result.elevated = elevation.TokenIsElevated != 0;
        result.restricted = restrictions != 0 || appContainer != 0 || IsTokenRestricted(token) != FALSE;
        result.session = session; result.logon = stats.AuthenticationId;
        result.impersonationAdequate = stats.TokenType == TokenPrimary ||
            stats.ImpersonationLevel >= SecurityImpersonation;
        result.observed = ok;
    }
    CloseHandle(token);
    if (!ok) result = {};
    return ok;
}
}
extern "C" __declspec(dllexport) UINT __stdcall ForegroundPostFinalize(MSIHANDLE) {
    return ERROR_INSTALL_FAILURE;
}
