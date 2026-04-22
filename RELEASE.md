## UltraNote v1.3.0 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.3.0

**In-place edit context menu:**
- Right-click inside a note while editing now opens a dedicated editor menu: Paste / Select all / Cut / Copy / Delete, plus a Date/Time submenu with 12 ATnotes-style strftime formats and live previews, and Insert file path via the system file picker
- Separate from the outer note context menu — `ID_NOTE_COPY` copies the whole note, `ID_EDIT_COPY` copies only the selection
- Modal file picker no longer tears down the edit control (new `m_editModalOpen` guard)

**Custom icons throughout the edit menu:**
- Cut, Select all, Date/Time and Insert-path now use dedicated `.ico` artwork instead of Segoe MDL2 Assets / Segoe Fluent Icons glyphs — consistent look across Windows 10 and 11
- Removed the glyph-rendering infrastructure (font probing, cached DIB sections, premultiplied-alpha conversion) — one code path instead of two

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
