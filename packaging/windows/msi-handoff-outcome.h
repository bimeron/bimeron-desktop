#pragma once

#include <optional>

#ifdef _WIN32
#include <windows.h>
#endif

namespace MsiHandoff {
enum class Outcome { Waiting, LaunchNew, LaunchOld, WaitForReboot, Recovery };

#ifdef _WIN32
inline std::optional<unsigned long> observedExit(HANDLE process) {
    DWORD code = 0;
    if (!process || WaitForSingleObject(process, 0) != WAIT_OBJECT_0 || !GetExitCodeProcess(process, &code))
        return std::nullopt;
    return code;
}
#endif

inline Outcome select(std::optional<unsigned long> terminalExit, bool oldVerified, bool newVerified, bool rebootPending) {
    if (!terminalExit) return Outcome::Waiting;
    if (*terminalExit == 3010 || *terminalExit == 1641) return Outcome::WaitForReboot;
    if (rebootPending || oldVerified == newVerified) return Outcome::Recovery;
    if (*terminalExit == 0 && newVerified) return Outcome::LaunchNew;
    if (*terminalExit != 0 && oldVerified) return Outcome::LaunchOld;
    return Outcome::Recovery;
}
}
