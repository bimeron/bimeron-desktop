#pragma once

#include <QCryptographicHash>
#include <QDir>
#include <qt_windows.h>
#include <array>
#include <memory>

namespace PinnedRepairImage {
struct Close {
    void operator()(void *handle) const { CloseHandle(handle); }
};
using Handle = std::unique_ptr<void, Close>;

inline Handle open(const QString &path, QByteArrayView expected) {
    const auto native = QDir::toNativeSeparators(path).toStdWString();
    const auto raw = CreateFileW(native.c_str(), GENERIC_READ | FILE_EXECUTE, FILE_SHARE_READ,
                                 nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (raw == INVALID_HANDLE_VALUE) return {};
    Handle file(raw);
    std::array<wchar_t, 32768> resolved{};
    const auto length = GetFinalPathNameByHandleW(raw, resolved.data(), DWORD(resolved.size()), 0);
    if (!length || length >= resolved.size()
        || _wcsicmp(resolved.data(), (LR"(\\?\)" + native).c_str()) != 0) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, 65536> buffer;
    DWORD count = 0;
    do {
        if (!ReadFile(raw, buffer.data(), DWORD(buffer.size()), &count, nullptr)) return {};
        hash.addData(QByteArrayView(buffer.data(), count));
    } while (count);
    if (hash.result().toHex() != expected) return {};
    return file;
}

inline bool verifiesPeer(HANDLE file, HANDLE launched, ULONG peer) {
    if (!file || !launched || !peer || GetProcessId(launched) != peer
        || WaitForSingleObject(launched, 0) != WAIT_TIMEOUT) return false;
    using Query = LONG (NTAPI *)(HANDLE, ULONG, void *, ULONG, ULONG *);
    const auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                                               "NtQueryInformationProcess"));
    constexpr ULONG processImageFileMapping = 44;
    return query && query(launched, processImageFileMapping, &file, sizeof(file), nullptr) == 0;
}
}
