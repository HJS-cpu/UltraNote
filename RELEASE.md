## UltraNote v1.8.0 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.8.0

A stability, correctness, and hardening release — the result of a full code audit.

**Alarms fixed:**
- Recurring alarms (Daily, Every-N-Days, Weekly, Monthly, Quarterly, Yearly) now actually fire — previously only one-shot alarms worked
- Stacked alarm popups no longer overlap when an earlier popup is dismissed

**Note-list fixes:**
- "Always on Top" via shortcut now toggles **all** selected notes, not just the first
- Hover preview keeps tracking the right note after re-sorting or refreshing the list

**Robustness & security hardening:**
- Imported `.unote` files are validated (format/version check, clamped geometry/font sizes, capped attachments) and malformed JSON can no longer crash or misbehave
- Localized strings can no longer crash the About dialog, export/import messages, or the print footer
- UTF-8 language names display correctly in the language picker

**Performance:**
- Settings are cached instead of re-read from disk on every keystroke, list refresh, and preview tick
- Faster note search and less GDI churn while scrolling the list and editing notes

**Leaks closed:** settings dialog and About dialog no longer leak GDI/icon handles on repeated open/close.

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
