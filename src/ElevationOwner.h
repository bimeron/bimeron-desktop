#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <QGuiApplication>
#include <QWindow>
#include <qt_windows.h>

// Windows shows the UAC consent prompt in the foreground only when it is owned
// by the foreground window; otherwise it only flashes on the taskbar. Call on
// the GUI thread; the returned handle may be passed to ShellExecuteEx anywhere.
inline HWND elevationOwnerWindow() {
    QWindow *window = QGuiApplication::focusWindow();
    return window ? reinterpret_cast<HWND>(window->winId()) : nullptr;
}
#endif
