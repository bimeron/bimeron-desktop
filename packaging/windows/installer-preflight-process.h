#pragma once

#include "installer-preflight-digest.h"

#include <tlhelp32.h>

namespace InstallerPreflight {
inline bool desktopIsClosed() {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    const InstallerPreflight::FileHandle processes(snapshot);
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(processes.get(), &entry)) return false;
    do {
        // Include tray-only instances and other sessions. A same-name process
        // whose identity cannot be proven is ambiguous, so refuse the upgrade.
        if (_wcsicmp(entry.szExeFile, L"bimeron-desktop.exe") == 0) return false;
    } while (Process32NextW(processes.get(), &entry));
    return GetLastError() == ERROR_NO_MORE_FILES;
}
}
