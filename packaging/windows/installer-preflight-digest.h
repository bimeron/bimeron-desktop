#pragma once

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace InstallerPreflight {
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

inline FileHandle openPinnedFile(const std::wstring &path, std::string_view expected, DWORD access = GENERIC_READ) {
    if (expected.size() != 64) return {};
    const HANDLE raw = CreateFileW(path.c_str(), access, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (raw == INVALID_HANDLE_VALUE) return {};
    FileHandle file(raw);
    std::array<wchar_t, 1024> resolved{};
    const DWORD length = GetFinalPathNameByHandleW(file.get(), resolved.data(), static_cast<DWORD>(resolved.size()), 0);
    if (!length || length >= resolved.size() || _wcsicmp(resolved.data(), (L"\\\\?\\" + path).c_str()) != 0)
        return {};
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart <= 0 || size.QuadPart > 64 * 1024 * 1024)
        return {};

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};
    const AlgorithmHandle provider(algorithm);
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(provider.get(), &hash, nullptr, 0, nullptr, 0, 0) != 0) return {};
    const HashHandle reader(hash);
    std::array<BYTE, 16384> buffer{};
    DWORD count = 0;
    do {
        if (!ReadFile(file.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)
            || (count && BCryptHashData(reader.get(), buffer.data(), count, 0) != 0))
            return {};
    } while (count);
    std::array<BYTE, 32> digest{};
    if (BCryptFinishHash(reader.get(), digest.data(), static_cast<ULONG>(digest.size()), 0) != 0) return {};
    constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < digest.size(); ++i) {
        if (expected[i * 2] != hex[digest[i] >> 4] || expected[i * 2 + 1] != hex[digest[i] & 15]) return {};
    }
    return file;
}

inline bool matchesPinnedDigest(const std::wstring &path, std::string_view expected) {
    return bool(openPinnedFile(path, expected));
}

// ProcessImageFileMapping compares the kernel image file object with the
// supplied file handle. Reopening a process image path cannot prove this.
inline bool processMapsFile(HANDLE process, HANDLE file) {
    using Query = LONG (NTAPI *)(HANDLE, ULONG, void *, ULONG, ULONG *);
    const auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                                               "NtQueryInformationProcess"));
    constexpr ULONG processImageFileMapping = 44;
    return query && query(process, processImageFileMapping, &file, sizeof(file), nullptr) == 0;
}

template<class ReadStatus>
bool canRetainRunningCore(const std::wstring &path, std::string_view digest,
                           const SERVICE_STATUS_PROCESS &initial, std::wstring_view uiLevel,
                           ReadStatus readStatus) {
    if (uiLevel != L"2" || initial.dwCurrentState != SERVICE_RUNNING || !initial.dwProcessId) return false;
    FileHandle process(OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, initial.dwProcessId));
    if (!process) return false;
    const auto image = openPinnedFile(path, digest, GENERIC_READ | FILE_EXECUTE);
    if (!image || !processMapsFile(process.get(), image.get())) return false;
    SERVICE_STATUS_PROCESS current{};
    return readStatus(current) && current.dwCurrentState == SERVICE_RUNNING
        && current.dwProcessId == initial.dwProcessId && WaitForSingleObject(process.get(), 0) == WAIT_TIMEOUT;
}

template<class ReadStatus>
bool canRetainBundledCore(const std::wstring &path, std::string_view bundledDigest,
                          const SERVICE_STATUS_PROCESS &initial, std::wstring_view uiLevel,
                          ReadStatus readStatus) {
    if (uiLevel == L"3" || uiLevel == L"4" || uiLevel == L"5")
        return matchesPinnedDigest(path, bundledDigest);
    if (uiLevel != L"2") return false;
    if (initial.dwCurrentState == SERVICE_RUNNING)
        return canRetainRunningCore(path, bundledDigest, initial, uiLevel, readStatus);
    if (initial.dwCurrentState != SERVICE_STOPPED || initial.dwProcessId) return false;
    const auto image = openPinnedFile(path, bundledDigest);
    SERVICE_STATUS_PROCESS current{};
    return image && readStatus(current) && current.dwCurrentState == SERVICE_STOPPED && !current.dwProcessId;
}
}
