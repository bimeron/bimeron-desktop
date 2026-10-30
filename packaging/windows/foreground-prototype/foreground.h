#pragma once
#include <windows.h>
#include <ntsecapi.h>
#include <string>
#include <memory>
#ifdef FOREGROUND_EXPORTS
#define FG_API __declspec(dllexport)
#else
#define FG_API __declspec(dllimport)
#endif

namespace foreground {
FG_API bool safeMutexDescriptor(PSECURITY_DESCRIPTOR sd);
FG_API bool markerAbsent(const wchar_t* marker);
// Contract admission: borrowed mutex handle; caller must retain it on this thread.
// On success caller owns mutex; on every failure caller does not own mutex.
FG_API bool admit(HANDLE mutex, const wchar_t* marker);

// Mechanics experiment only: accepts an already-admitted fixture mutex, NOT an
// installer authorization API. No policy/path acceptance or MSI routing exists.
class Worker {
public:
    FG_API Worker(HANDLE mutex, std::wstring journal, std::string rollbackMetadata);
    FG_API ~Worker();
    FG_API bool begin();
    FG_API bool settleFixture(const struct SettlementFixture& evidence);
    FG_API DWORD ownerThread() const;
private:
    struct State;
    std::unique_ptr<State> state;
};
}

namespace foreground {
// Offline observations only. No value of these types authorizes enrollment.
struct TokenFixture {
    bool observed = false, interactive = false, elevated = false;
    bool enabledAdmin = false, restricted = true, impersonationAdequate = false;
    DWORD integrity = 0, session = 0;
    std::wstring sid;
    LUID logon{};
};
enum class ThreadTokenResult { Present, NoToken, Error };
FG_API TokenFixture effectiveTokenFixture(ThreadTokenResult threadResult,
                                        const TokenFixture& thread, const TokenFixture& process);
FG_API bool eligibleContextFixture(const TokenFixture& effective, const TokenFixture& initiating);
FG_API bool safeJournalDescriptor(PSECURITY_DESCRIPTOR descriptor);
FG_API bool journalAccessFixture(PSECURITY_DESCRIPTOR descriptor, HANDLE token,
                                bool native64View, bool safeAncestors);
// Reads only the current effective token, never discovers/guesses an MSI client.
FG_API bool readEffectiveToken(TokenFixture& result);
enum class ClientCorrelation { Unknown };
FG_API ClientCorrelation verifyInitiatingClient();
enum class Callback { Missing, Commit, Rollback, PostFinalize };
struct SettlementFixture {
    Callback callback = Callback::Missing;
    TokenFixture effective, initiating;
    unsigned long long pendingGeneration = 0, observedGeneration = 0;
    bool pending = false, inventory = false, outerCompletion = false;
    bool protection = false, quiescent = false, flushed = false, hostAlive = false;
};
FG_API bool settledFixture(const SettlementFixture& evidence);
}

namespace foreground {
enum class TokenSource { Unknown, Thread, Process };
enum class ContextRefusal { None, TokenOpen, TokenQuery, Authority, Principal,
                            LsaQuery, LsaData, LogonType, DescriptorAccess };
// Disconnected observations and model inputs, never authority credentials.
struct CallbackInteractiveContextObservation {
    ContextRefusal refusal = ContextRefusal::TokenQuery;
    DWORD status = 0;
    LONG lsaStatus = 0;
    TokenSource source = TokenSource::Unknown;
    TOKEN_TYPE type = static_cast<TOKEN_TYPE>(0);
    TOKEN_ELEVATION_TYPE elevationType = static_cast<TOKEN_ELEVATION_TYPE>(0);
    SECURITY_IMPERSONATION_LEVEL impersonation = SecurityAnonymous;
    bool queried = false, elevated = false, enabledAdmin = false, adminDenyOnly = false;
    bool restricted = true, appContainer = true, servicePrincipal = false;
    DWORD integrity = 0, session = 0;
    std::wstring sid;
    LUID authenticationId{}, selectedTokenId{};
    bool lsaDataValid = false;
    DWORD lsaSession = 0, logonType = 0;
    std::wstring lsaSid;
    LUID lsaAuthenticationId{};
};
FG_API void captureLogonDataFixture(CallbackInteractiveContextObservation& observation,
                                    const SECURITY_LOGON_SESSION_DATA* data);
FG_API ContextRefusal callbackContextRefusalFixture(
    const CallbackInteractiveContextObservation& observation);
// Optional in-memory registry descriptor check only, never a protected-object open.
FG_API CallbackInteractiveContextObservation observeCallbackInteractiveContext(
    PSECURITY_DESCRIPTOR descriptor = nullptr);
}
