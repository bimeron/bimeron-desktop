#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "installer-preflight-digest.h"
#include <msi.h>
#include <msiquery.h>
#include <shlobj.h>
#include "installer-preflight-process.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace {
struct CloseService {
    void operator()(SC_HANDLE handle) const { CloseServiceHandle(handle); }
};
using ServiceHandle = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, CloseService>;

void log(MSIHANDLE session, INSTALLMESSAGE type, const wchar_t *message) {
    if (const MSIHANDLE record = MsiCreateRecord(0)) {
        if (MsiRecordSetStringW(record, 0, message) == ERROR_SUCCESS)
            MsiProcessMessage(session, type, record);
        MsiCloseHandle(record);
    }
}

std::wstring standardServicePath() {
    PWSTR root = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DONT_VERIFY, nullptr, &root))) return {};
    std::wstring path(root);
    CoTaskMemFree(root);
    return path + L"\\Bimeron Core\\bimeron.exe";
}

bool registeredImageMatches(SC_HANDLE service, const std::wstring &path) {
    DWORD bytes = 0;
    QueryServiceConfigW(service, nullptr, 0, &bytes);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes > 8192 || bytes < sizeof(QUERY_SERVICE_CONFIGW))
        return false;
    std::vector<BYTE> storage(bytes);
    auto *config = reinterpret_cast<QUERY_SERVICE_CONFIGW *>(storage.data());
    if (!QueryServiceConfigW(service, config, bytes, &bytes) || !config->lpBinaryPathName
        || !config->lpServiceStartName || _wcsicmp(config->lpServiceStartName, L"LocalSystem") != 0)
        return false;
    const std::wstring command(config->lpBinaryPathName);
    const std::wstring prefix = L"\"" + path + L"\" service run S-1-";
    if (command.size() <= prefix.size() || _wcsnicmp(command.c_str(), prefix.c_str(), prefix.size()) != 0)
        return false;
    return command.find_first_not_of(L"0123456789-", prefix.size()) == std::wstring::npos;
}


}

extern "C" __declspec(dllexport) UINT __stdcall BimeronPreflight(MSIHANDLE session) {
    if (!InstallerPreflight::desktopIsClosed()) {
        log(session, INSTALLMESSAGE_ERROR,
            L"Bimeron upgrade_deferred: Desktop may still be running. Disconnect the VPN, "
            L"select Quit from the Bimeron tray menu, and run the MSI again.");
        return ERROR_INSTALL_FAILURE;
    }
    const ServiceHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (manager) {
        const ServiceHandle service(OpenServiceW(manager.get(), L"BimeronCore", SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS));
        if (!service && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
            log(session, INSTALLMESSAGE_INFO, L"Bimeron preflight: no installed service");
            return ERROR_SUCCESS;
        }
        if (service) {
            SERVICE_STATUS_PROCESS status{};
            DWORD bytes = 0;
            if (QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO,
                                    reinterpret_cast<BYTE *>(&status), sizeof(status), &bytes)) {
                log(session, INSTALLMESSAGE_INFO, status.dwCurrentState == SERVICE_STOPPED
                    ? L"Bimeron preflight: service stopped; VPN cleanup is unverified"
                    : L"Bimeron preflight: service present; VPN idle state is unverified");
                const std::wstring path = standardServicePath();
                if (!path.empty() && registeredImageMatches(service.get(), path) && InstallerPreflight::matchesPinnedDigest(path, BIMERON_INSTALLER_CORE_SHA256)) {
                    log(session, INSTALLMESSAGE_INFO, L"Bimeron preflight: identical Core; retaining the service");
                    return ERROR_SUCCESS;
                }
            }
        }
    }
    log(session, INSTALLMESSAGE_ERROR,
        L"Bimeron upgrade_deferred: the installed Core could not be verified as identical. "
        L"Use the approved Core maintenance procedure before retrying this Desktop package.");
    return ERROR_INSTALL_FAILURE;
}
