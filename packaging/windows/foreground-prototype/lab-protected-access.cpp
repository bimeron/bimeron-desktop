// Offline-only access observation. No file or installer authority is granted.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msi.h>

extern "C" __declspec(dllexport) DWORD __stdcall LabEffectiveTokenSource() {
    HANDLE token = nullptr;
    DWORD source = 1;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
        if (GetLastError() != ERROR_NO_TOKEN) return 0;
        source = 2;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return 0;
    }
    TOKEN_STATISTICS statistics{};
    DWORD size = 0;
    bool ok = GetTokenInformation(token, TokenStatistics, &statistics,
                                  sizeof(statistics), &size) != FALSE;
    CloseHandle(token);
    return ok ? source : 0;
}
// Unknown client correlation and unimplemented protected storage always block.
extern "C" __declspec(dllexport) UINT __stdcall LabProtectedBegin(MSIHANDLE) {
    return ERROR_INSTALL_FAILURE;
}
extern "C" __declspec(dllexport) UINT __stdcall LabProtectedAcknowledge(MSIHANDLE) {
    (void)LabEffectiveTokenSource();
    return ERROR_INSTALL_FAILURE;
}
