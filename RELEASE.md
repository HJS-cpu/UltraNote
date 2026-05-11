## UltraNote v1.7.0 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.7.0

**Run at Windows startup:**
- New "Startup" group in the General settings tab — toggle whether UltraNote should launch automatically when you sign in to Windows
- Stored as a single `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\UltraNote` value; nothing else touches the registry
- Self-healing: if the EXE is moved or renamed while autostart is on, the entry is corrected on the next launch

**Live preview for the initial-text template:**
- The Misc tab now shows a read-only preview right below the initial-text box
- As you type or click "Insert…", strftime variables (`%x`, `%X`, `%#c`, …) are resolved in real time against the current Windows locale
- Robust against partial/invalid format strings — typing `%` or `%q` no longer crashes the dialog

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
- Configurable initial text with date/time variables and live preview
- Import / export of notes (`.unote`) and direct printing via the Windows print dialog
- Settings dialog (Layout, Keyboard, General, Misc, Note List) — configurable shortcuts shown directly in the context menus
- Global hotkeys (Ctrl+Shift+N, Ctrl+Shift+L) plus per-note shortcuts (Delete, Always-on-Top, Hide)
- Optional Windows-startup launch (opt-in, single HKCU\…\Run value)
- English and German localization
- JSON-based portable storage — notes and settings stay in the EXE folder

### System Requirements

- Windows 10 or later
- No external dependencies

### Links

- [Source Code](https://github.com/HJS-cpu/UltraNote)
- [All Releases](https://github.com/HJS-cpu/UltraNote/releases)
