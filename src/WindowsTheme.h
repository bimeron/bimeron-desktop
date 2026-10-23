#pragma once

#include <QApplication>
#include <QPalette>
#include <QSettings>

#ifdef Q_OS_WIN
#include <qt_windows.h>

inline bool windowsDarkMode() {
    DWORD light = 1;
    DWORD size = sizeof(light);
    return RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size) == ERROR_SUCCESS && light == 0;
}
#else
inline bool windowsDarkMode() { return false; }
#endif

inline bool appDarkMode() {
    const QString choice = QSettings().value("app/theme", "system").toString();
    if (choice == "dark") return true;
    if (choice == "light") return false;
#ifdef Q_OS_WIN
    return windowsDarkMode();
#else
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
#endif
}

inline void applyThemePalette(QApplication &app) {
    const QString choice = QSettings().value("app/theme", "system").toString();
#ifndef Q_OS_WIN
    if (choice == "system") return;
#endif
    const bool dark = appDarkMode();
    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor(dark ? "#181e27" : "#f8fafc"));
    palette.setColor(QPalette::WindowText, QColor(dark ? "#e8edf5" : "#152238"));
    palette.setColor(QPalette::Base, QColor(dark ? "#202937" : "#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor(dark ? "#263244" : "#f7f9fc"));
    palette.setColor(QPalette::Text, QColor(dark ? "#e8edf5" : "#152238"));
    palette.setColor(QPalette::Button, QColor(dark ? "#263244" : "#ffffff"));
    palette.setColor(QPalette::ButtonText, QColor(dark ? "#e8edf5" : "#152238"));
    palette.setColor(QPalette::Mid, QColor(dark ? "#52647c" : "#ccd8e5"));
    palette.setColor(QPalette::Highlight, QColor(dark ? "#315a88" : "#1677ff"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor(dark ? "#aab8ca" : "#95a2b2"));
    app.setPalette(palette);
}
