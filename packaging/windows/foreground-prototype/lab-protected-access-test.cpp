#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msi.h>
#include <cstdio>
extern "C" __declspec(dllimport) DWORD __stdcall LabEffectiveTokenSource();
extern "C" __declspec(dllimport) UINT __stdcall LabProtectedBegin(MSIHANDLE);
extern "C" __declspec(dllimport) UINT __stdcall LabProtectedAcknowledge(MSIHANDLE);
int main() {
    DWORD source = LabEffectiveTokenSource();
    if (source != 2 || LabProtectedBegin(0) != ERROR_INSTALL_FAILURE ||
        LabProtectedAcknowledge(0) != ERROR_INSTALL_FAILURE) return 1;
    HANDLE process = nullptr, impersonation = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &process) ||
        !DuplicateToken(process, SecurityImpersonation, &impersonation) ||
        !SetThreadToken(nullptr, impersonation)) return 2;
    bool ok = LabEffectiveTokenSource() == 1;
    SetThreadToken(nullptr, nullptr);
    CloseHandle(impersonation); CloseHandle(process);
    std::puts(ok ? "effective token observation and fail-closed callbacks passed" : "thread token observation failed");
    return ok ? 0 : 3;
}
