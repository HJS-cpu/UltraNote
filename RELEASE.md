## UltraNote v1.8.6 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.8.6

**New setting — "New notes always on top":**
- A new checkbox in Settings → General (Display group) makes every newly created note default to always-on-top
- Off by default, so existing behaviour is unchanged; the option only affects notes created after it is enabled
- Existing notes keep their individual always-on-top state — the global default never overrides a note you've already pinned or un-pinned

Built on top of the v1.8.5 code-audit remediation (74 verified fixes); no new dependencies.

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

- [Source Code](https://github.com/HJS-Lab/UltraNote)
- [All Releases](https://github.com/HJS-Lab/UltraNote/releases)
