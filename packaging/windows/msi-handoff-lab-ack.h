#pragma once

#include <windows.h>

#include <QString>

namespace MsiHandoff {
inline int signalLabReady(const QString &value) {
    bool parsed = false;
    const auto raw = value.toULongLong(&parsed);
    if (!parsed || raw == 0 || raw != quintptr(raw)) return 1;
    const auto event = reinterpret_cast<HANDLE>(quintptr(raw));
    const BOOL signaled = SetEvent(event);
    CloseHandle(event);
    return signaled ? 0 : 1;
}
}
