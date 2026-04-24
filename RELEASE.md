## UltraNote v1.4.0 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.4.0

**Note list stays put:**
- The note list no longer minimizes — the title-bar minimize button is disabled, and `SC_MINIMIZE` is blocked so the window cannot be sent to the taskbar via the system menu, Win+Down, or a taskbar right-click either

**Smaller binary:**
- All `.ico` resources re-encoded with PNG-compressed frames instead of uncompressed BMP-DIB — lossless, pixel-identical, loader-compatible since Windows Vista
- Embedded resources shrink by ~99 KB; the Release EXE drops from ~700 KB to ~600 KB (−15 %)
- New reusable `tools/ico_to_png.py` converter with built-in pixel-roundtrip verification

### Download

| Package | Description |
|---------|-------------|
| **UltraNote-Portable.zip** | Standalone EXE + language files — no installation needed |

### Features

- Sticky notes with drag, resize, multi-select, and always-on-top
- Per-note alarms with flexible repetition and snooze
- Note list with sorting, search (with highlighting), hover preview, folder management, and column visibility toggles
- In-note find dialog with case-sensitive and whole-word options
- In-place editor context menu with date/time insertion and file-path picker
- File attachments via drag & drop
- Clickable URLs in note text
- Configurable initial text with date/time variables
- Settings dialog (Layout, Keyboard, General, Misc, Note List)
- Global hotkeys (Ctrl+Shift+N, Ctrl+Shift+L)
- English and German localization
- JSON-based portable storage — no registry, no AppData

### System Requirements

- Windows 10 or later
- No external dependencies

### Links

- [Source Code](https://github.com/HJS-cpu/UltraNote)
- [All Releases](https://github.com/HJS-cpu/UltraNote/releases)
