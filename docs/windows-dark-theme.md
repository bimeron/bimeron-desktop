# Appearance theme

Settings > Application places Theme directly above Language. The choices are System (default), Light, and Dark. The selection is stored in `app/theme`; changing it restarts Bimeron after confirmation so every window and custom drawn control uses one consistent palette. Cancel restores the previous selection and setting. System follows the Windows app theme (`AppsUseLightTheme`) at launch; the tray icon still follows the taskbar setting. On Linux, System uses the Qt palette, while explicit Light and Dark use the application palettes.

Acceptance criteria:
- A fresh installation shows System selected; explicit Light and Dark persist across restarts.
- Selecting Dark on Windows 11 produces dark surfaces and readable text even if Windows uses the light app theme.
- Selecting Light produces light surfaces and dark text even if Windows uses the dark app theme.
- Canceling the restart prompt leaves the previous theme active and selected.
- The selector is translated to Russian and appears above Language.
## Layout and disabled states

The window starts at and cannot shrink below 720 by 600 logical client pixels, matching the full Settings layout at 150% scaling. The Settings page has no scroll area or horizontal scrollbar; Theme and Language fields align at the right edge and have the same width. In both themes, disabled buttons use a muted neutral surface, text, and border. The disabled startup profile selector uses a subtle border and muted surface rather than a bright outline. These states must look the same at 100% and high DPI rendering paths.

Acceptance criteria:
- Theme and Language rows never overlap at the minimum window size and 150% scaling.
- At the minimum window size, Settings shows its footer without horizontal scrolling.
- The disabled Connect button is neutral in both themes, including its custom high DPI renderer.
- The disabled auto-connect profile selector has a low-contrast border in both themes.
