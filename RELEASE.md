## UltraNote v1.6.0 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.6.0

**Import & Export of notes:**
- New `.unote` export format (JSON) — share notes between machines or back them up
- Export the current selection from the note list, or fall back to all visible notes when nothing is selected
- Import always assigns fresh IDs, so re-importing the same file never overwrites existing notes
- Unknown folders referenced in an import are auto-created; off-screen note positions are clamped back onto a visible monitor

**Print notes:**
- Print one or more notes directly from the note window or the note list (toolbar, file menu, context menu)
- Standard Windows print dialog with margins, title bar, body text, and page numbers
- Always prints black-on-white — note background colors are intentionally ignored for legibility

**New toolbar icons:**
- Dedicated `import`, `export`, and `print` icons replace the previous Shell stock glyphs

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
- Settings dialog (Layout, Keyboard, General, Misc, Note List) — configurable shortcuts shown directly in the context menus
- Global hotkeys (Ctrl+Shift+N, Ctrl+Shift+L) plus per-note shortcuts (Delete, Always-on-Top, Hide)
- English and German localization
- JSON-based portable storage — no registry, no AppData

### System Requirements

- Windows 10 or later
- No external dependencies

### Links

- [Source Code](https://github.com/HJS-cpu/UltraNote)
- [All Releases](https://github.com/HJS-cpu/UltraNote/releases)
