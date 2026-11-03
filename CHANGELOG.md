# Changelog

All notable changes to Omatranslate are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
the package version in `pkgbuild/PKGBUILD`.

## [0.2.0] — 2026-11-02

### Added

- An Omarchy bar widget with **Watching**, **Paused**, and **Stopped** modes,
  plus **Open app** and **Settings**. The widget owns start/stop and remembers
  whether to start watching with the bar.
- A single background service: `open`, `settings`, `show`, and `hide` forward
  to it, so the window is a view onto one process instead of a second copy.
- `omatranslate service --paused`, `hide`, `show`, and `status --json`.
- Translation service choice: Google Translate (default), DeepL, and
  LibreTranslate, with the exact endpoint, what is sent, and where keys live
  disclosed in Settings.
- `omatranslate plugin install|remove` and the bundled agent skill.
- Interface translations for English, German, and Danish; the widget follows
  the same languages.

### Changed

- Highlights never translate automatically. A highlight shows a **Translate**
  chip; only the chip, `Enter`, or `omatranslate translate` sends a request.
- Closing the window hides it and keeps watching and the session alive.
  `stop` quits, unless the session has unexported translations.
- The floating card follows the Omarchy theme and is placed from Hyprland's
  monitor, pointer, and active-window information.

### Fixed

- Quitting after the window was shown could hang; the window is now hidden
  before the process exits.
- A service started from an offscreen command no longer inherits
  `QT_QPA_PLATFORM` and disappears.
- A second service no longer steals the control socket.
- The bar widget no longer registers a duplicate IPC handler.

## [0.1.0] — 2026-10-30

### Added

- Translate a passage highlighted in any PDF reader, with a **Translate** chip
  and a floating card over the page.
- Session history in the app window, with dedupe, undo, and Markdown export to
  a sidecar `<pdf>.omatranslate.md`.
- Command line translation (`translate`, `languages`) that needs no display.
- Live Omarchy theme colours and interface language.
