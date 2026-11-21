#pragma once

#include "installer-preflight-digest.h"

#include <QString>
#include <type_traits>
#include <vector>

namespace MsiHandoff {
inline InstallerPreflight::FileHandle startLabProcess(const std::wstring &path, std::wstring command,
                                                       HANDLE *handles, size_t handleCount) {
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    if (!size) return {};
    std::vector<BYTE> buffer(size);
    const auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &size)) return {};
    const std::unique_ptr<std::remove_pointer_t<LPPROC_THREAD_ATTRIBUTE_LIST>, decltype(&DeleteProcThreadAttributeList)>
        attributes(list, DeleteProcThreadAttributeList);
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   handles, sizeof(HANDLE) * handleCount, nullptr, nullptr)) return {};
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = list;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, TRUE,
                        EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &process)) return {};
    CloseHandle(process.hThread);
    return InstallerPreflight::FileHandle(process.hProcess);
}

inline InstallerPreflight::FileHandle inheritedHandle(const QString &value) {
    bool parsed = false;
    const auto raw = value.toULongLong(&parsed);
    if (!parsed || raw == 0 || raw != quintptr(raw)) return {};
    const auto handle = reinterpret_cast<HANDLE>(quintptr(raw));
    DWORD flags = 0;
    if (!GetHandleInformation(handle, &flags) || !(flags & HANDLE_FLAG_INHERIT)) return {};
    return InstallerPreflight::FileHandle(handle);
}
}
