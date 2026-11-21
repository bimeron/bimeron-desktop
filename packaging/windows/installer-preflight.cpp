#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <msi.h>
#include <msiquery.h>
#include <shlobj.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
struct CloseService {
    void operator()(SC_HANDLE handle) const { CloseServiceHandle(handle); }
};
using ServiceHandle = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, CloseService>;

struct CloseFile {
    void operator()(HANDLE handle) const { CloseHandle(handle); }
};
using FileHandle = std::unique_ptr<void, CloseFile>;

struct CloseAlgorithm {
    void operator()(BCRYPT_ALG_HANDLE handle) const { BCryptCloseAlgorithmProvider(handle, 0); }
};
using AlgorithmHandle = std::unique_ptr<void, CloseAlgorithm>;

struct CloseHash {
    void operator()(BCRYPT_HASH_HANDLE handle) const { BCryptDestroyHash(handle); }
};
using HashHandle = std::unique_ptr<void, CloseHash>;

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

bool matchesBundledDigest(const std::wstring &path) {
    constexpr std::string_view expected(BIMERON_INSTALLER_CORE_SHA256);
    if (expected.size() != 64) return false;
    const HANDLE raw = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (raw == INVALID_HANDLE_VALUE) return false;
    const FileHandle file(raw);
    std::array<wchar_t, 1024> resolved{};
    const DWORD length = GetFinalPathNameByHandleW(file.get(), resolved.data(), static_cast<DWORD>(resolved.size()), 0);
    if (!length || length >= resolved.size() || _wcsicmp(resolved.data(), (L"\\\\?\\" + path).c_str()) != 0)
        return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart <= 0 || size.QuadPart > 64 * 1024 * 1024)
        return false;

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return false;
    const AlgorithmHandle provider(algorithm);
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(provider.get(), &hash, nullptr, 0, nullptr, 0, 0) != 0) return false;
    const HashHandle reader(hash);
    std::array<BYTE, 16384> buffer{};
    DWORD count = 0;
    do {
        if (!ReadFile(file.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)
            || (count && BCryptHashData(reader.get(), buffer.data(), count, 0) != 0))
            return false;
    } while (count);
    std::array<BYTE, 32> digest{};
    if (BCryptFinishHash(reader.get(), digest.data(), static_cast<ULONG>(digest.size()), 0) != 0) return false;
    constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < digest.size(); ++i) {
        if (expected[i * 2] != hex[digest[i] >> 4] || expected[i * 2 + 1] != hex[digest[i] & 15]) return false;
    }
    return true;
}
}

extern "C" __declspec(dllexport) UINT __stdcall BimeronPreflight(MSIHANDLE session) {
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
                if (!path.empty() && registeredImageMatches(service.get(), path) && matchesBundledDigest(path)) {
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
