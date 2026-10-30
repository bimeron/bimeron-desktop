#pragma once
#include <windows.h>
#include <ntsecapi.h>
#include <string>

#if defined(FOREGROUND_EXPORTS)
#define CONTEXT_API __declspec(dllexport)
#elif defined(FOREGROUND_CONTEXT_IMPORTS)
#define CONTEXT_API __declspec(dllimport)
#else
#define CONTEXT_API
#endif
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

// Internal shared observation code. No descriptor, journal or installer access.
bool openSelectedContextToken(HANDLE& token, TokenSource& source, DWORD& status,
                              DWORD access = TOKEN_QUERY);
void captureSelectedContext(HANDLE token, CallbackInteractiveContextObservation& observation);
struct SelectedContextToken {
    HANDLE value = nullptr;
    SelectedContextToken() = default;
    SelectedContextToken(const SelectedContextToken&) = delete;
    SelectedContextToken& operator=(const SelectedContextToken&) = delete;
    ~SelectedContextToken() { if (value) CloseHandle(value); }
};
CONTEXT_API void captureLogonDataFixture(CallbackInteractiveContextObservation& observation,
                            const SECURITY_LOGON_SESSION_DATA* data);
CONTEXT_API ContextRefusal callbackContextRefusalFixture(const CallbackInteractiveContextObservation& observation);
CallbackInteractiveContextObservation observeSelectedContext();
}
