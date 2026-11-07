#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include "lab-context-record.h"
#include <iostream>
#include <stdexcept>
#include <string>
static unsigned messages = 0;
static unsigned failLogging = 0;
static MSIHANDLE WINAPI testCreateRecord(UINT) { return failLogging == 1 ? 0 : 1; }
static UINT WINAPI testSetString(MSIHANDLE, UINT, LPCWSTR) { return failLogging == 2 ? ERROR_WRITE_FAULT : ERROR_SUCCESS; }
static int WINAPI testMessage(MSIHANDLE, INSTALLMESSAGE, MSIHANDLE) { ++messages; return failLogging == 3 ? -1 : 0; }
static UINT WINAPI testClose(MSIHANDLE) { return ERROR_SUCCESS; }
#define MsiCreateRecord testCreateRecord
#define MsiRecordSetStringW testSetString
#define MsiProcessMessage testMessage
#define MsiCloseHandle testClose
#define LAB_FAILPOINT 1
#define FOREGROUND_LAB_TRACE_CONTEXT
#include "lab-trace.cpp"
#undef MsiCreateRecord
#undef MsiRecordSetStringW
#undef MsiProcessMessage
#undef MsiCloseHandle
using namespace foreground;
static void check(bool ok) { if (!ok) throw std::runtime_error("context record regression"); }
static unsigned captures = 0;
static CallbackInteractiveContextObservation model() {
    ++captures;
    CallbackInteractiveContextObservation o;
    o.source = foreground::TokenSource::Process; o.type = TokenPrimary;
    o.elevationType = TokenElevationTypeDefault;
    o.queried = o.elevated = o.enabledAdmin = o.lsaDataValid = true;
    o.restricted = o.appContainer = false;
    o.integrity = SECURITY_MANDATORY_HIGH_RID; o.session = o.lsaSession = 1;
    o.sid = o.lsaSid = L"S-1-5-21-1-2-3-1001";
    o.authenticationId.LowPart = o.lsaAuthenticationId.LowPart = 7;
    o.selectedTokenId.HighPart = static_cast<LONG>(0xffffffff); o.selectedTokenId.LowPart = 9;
    o.logonType = 2; o.refusal = ContextRefusal::None;
    return o;
}
static CallbackInteractiveContextObservation throwing() { ++captures; throw std::bad_alloc(); }
int main() {
    try {
        wchar_t record[ContextRecordCapacity]{};
        check(captureContextRecord(record, _countof(record), 4, 1, 0, model));
        check(captures == 1);
        const std::wstring valid(record);
        check(valid.find(L"\"complete\":1,\"eligible\":1") != std::wstring::npos);
        check(valid.find(L"ffffffff00000009") != std::wstring::npos && valid.find(L"S-1-") == std::wstring::npos);
        auto o = model();
        check(formatContextRecord(record, valid.size()+1, 0, 1, 0, o));
        check(!formatContextRecord(record, valid.size(), 0, 1, 0, o) && record[0] == 0);
        wchar_t tinyBuffer[2]{L'x',L'x'};
        check(!formatContextRecord(tinyBuffer, 2, 0, 1, 0, o) && tinyBuffer[0] == 0 && tinyBuffer[1] == L'x');
        check(!formatContextRecord(nullptr, 0, 0, 1, 0, o));
        check(!formatContextRecord(record, _countof(record), 5, 1, 0, o));
        for (unsigned callback = 0; callback < 5; ++callback) {
            check(formatContextRecord(record, _countof(record), callback, MAXDWORD, 0, o));
            check(wcslen(record) < ContextRecordCapacity);
        }
        for (int n = 0; n < 10; ++n) {
            auto bad = o;
            switch (n) {
            case 0: bad.queried = false; break;
            case 1: bad.status = ERROR_ACCESS_DENIED; break;
            case 2: bad.lsaStatus = -1; break;
            case 3: bad.lsaDataValid = false; break;
            case 4: bad.source = static_cast<foreground::TokenSource>(99); break;
            case 5: bad.type = static_cast<TOKEN_TYPE>(99); break;
            case 6: bad.elevationType = static_cast<TOKEN_ELEVATION_TYPE>(99); break;
            case 7: bad.type = TokenImpersonation; bad.impersonation = static_cast<SECURITY_IMPERSONATION_LEVEL>(99); break;
            case 8: bad.refusal = static_cast<ContextRefusal>(99); break;
            case 9: break;
            }
            check(formatContextRecord(record, _countof(record), 0, 1, n == 9 ? ERROR_INVALID_DATA : 0, bad));
            check(std::wstring(record).find(L"\"complete\":0,\"eligible\":0") != std::wstring::npos);
        }
        captures = 0;
        check(captureContextRecord(record, _countof(record), 0, 1, 0, throwing) && captures == 1);
        check(std::wstring(record).find(L"\"complete\":0,\"eligible\":0") != std::wstring::npos);
        for (auto type : {2UL,10UL,3UL,5UL}) {
            auto sample = o; sample.logonType = type;
            check((callbackContextRefusalFixture(sample) == ContextRefusal::None) == (type == 2 || type == 10));
        }
        auto system = o; system.sid = system.lsaSid = L"S-1-5-18";
        check(callbackContextRefusalFixture(system) != ContextRefusal::None);
        check(formatContextRecord(record, _countof(record), 2, 1, 0, system));
        check(std::wstring(record).find(L"\"system\":1") != std::wstring::npos);
        check(std::wstring(record).find(L"\"eligible\":0") != std::wstring::npos);
        for (unsigned failure = 0; failure < 4; ++failure) {
            messages = 0; failLogging = failure;
            check(ObservationPreflight(0) == ERROR_SUCCESS);
            check(ObservationRollback(0) == ERROR_SUCCESS);
            check(ObservationDeferredBegin(0) == ERROR_SUCCESS);
            check(ObservationCommit(0) == ERROR_INSTALL_FAILURE);
            check(ObservationPostFinalize(0) == ERROR_SUCCESS);
            check(messages == (failure == 0 ? 10U : failure == 3 ? 20U : 0U));
        }
        std::cout << "PASS bounded records, unknown enums, incomplete/exception, single capture, SYSTEM/type policy, compiled returns with logging failure\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
