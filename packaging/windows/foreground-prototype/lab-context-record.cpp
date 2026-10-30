#include "lab-context-record.h"
#include <cstdio>
#include <cwchar>
namespace foreground {
bool formatContextRecord(wchar_t* output, size_t capacity, unsigned callback,
    DWORD processSession, DWORD processStatus,
    const CallbackInteractiveContextObservation& o) noexcept {
    if (!output || !capacity) return false;
    output[0] = 0;
    if (callback > 4) return false;
    const bool enums = (o.source == TokenSource::Thread || o.source == TokenSource::Process) &&
        (o.type == TokenPrimary || o.type == TokenImpersonation) &&
        (o.type != TokenImpersonation || (o.impersonation >= SecurityAnonymous && o.impersonation <= SecurityDelegation)) &&
        o.elevationType >= TokenElevationTypeDefault && o.elevationType <= TokenElevationTypeLimited &&
        o.refusal >= ContextRefusal::None && o.refusal <= ContextRefusal::DescriptorAccess;
    const bool complete = enums && !processStatus && o.queried && !o.status && !o.lsaStatus && o.lsaDataValid;
    const bool eligible = complete && callbackContextRefusalFixture(o) == ContextRefusal::None &&
        o.refusal == ContextRefusal::None;
    wchar_t record[ContextRecordCapacity]{};
    const int n = swprintf_s(record,
        L"BIMERON_LAB_CONTEXT_V1 {\"version\":1,\"callback\":%u,\"complete\":%u,\"eligible\":%u,"
        L"\"processSession\":%lu,\"processStatus\":%lu,\"source\":%u,\"type\":%u,\"impersonation\":%d,"
        L"\"tokenId\":\"%08lx%08lx\",\"authenticationId\":\"%08lx%08lx\",\"tokenSession\":%lu,"
        L"\"elevationType\":%u,\"elevated\":%u,\"integrity\":%lu,\"adminEnabled\":%u,\"adminDenyOnly\":%u,"
        L"\"restricted\":%u,\"appContainer\":%u,\"service\":%u,\"system\":%u,\"status\":%lu,"
        L"\"lsaStatus\":%lu,\"lsaValid\":%u,\"lsaSession\":%lu,\"lsaAuthenticationId\":\"%08lx%08lx\","
        L"\"logonType\":%lu,\"sidMatch\":%u,\"refusal\":%u}",
        callback, static_cast<unsigned>(complete), static_cast<unsigned>(eligible), processSession, processStatus,
        enums ? static_cast<unsigned>(o.source) : 0, enums ? static_cast<unsigned>(o.type) : 0,
        enums && o.type == TokenImpersonation ? static_cast<int>(o.impersonation) : -1,
        static_cast<DWORD>(o.selectedTokenId.HighPart), o.selectedTokenId.LowPart,
        static_cast<DWORD>(o.authenticationId.HighPart), o.authenticationId.LowPart, o.session,
        enums ? static_cast<unsigned>(o.elevationType) : 0, static_cast<unsigned>(o.elevated), o.integrity,
        static_cast<unsigned>(o.enabledAdmin), static_cast<unsigned>(o.adminDenyOnly),
        static_cast<unsigned>(o.restricted), static_cast<unsigned>(o.appContainer),
        static_cast<unsigned>(o.servicePrincipal), static_cast<unsigned>(o.sid == L"S-1-5-18"),
        o.status, static_cast<DWORD>(o.lsaStatus), static_cast<unsigned>(o.lsaDataValid), o.lsaSession,
        static_cast<DWORD>(o.lsaAuthenticationId.HighPart), o.lsaAuthenticationId.LowPart,
        o.logonType, static_cast<unsigned>(o.lsaDataValid && !o.sid.empty() && o.sid == o.lsaSid),
        enums ? static_cast<unsigned>(o.refusal) : static_cast<unsigned>(ContextRefusal::TokenQuery));
    if (n <= 0 || static_cast<size_t>(n) >= capacity) return false;
    wmemcpy(output, record, static_cast<size_t>(n) + 1);
    return true;
}
bool captureContextRecord(wchar_t* output, size_t capacity, unsigned callback,
    DWORD processSession, DWORD processStatus, ContextObserver observer) noexcept {
    try {
        if (observer) return formatContextRecord(output, capacity, callback, processSession, processStatus, observer());
    } catch (...) { }
    CallbackInteractiveContextObservation incomplete;
    incomplete.status = ERROR_NOT_ENOUGH_MEMORY;
    return formatContextRecord(output, capacity, callback, processSession, processStatus, incomplete);
}
}
