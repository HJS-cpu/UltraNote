## UltraNote v1.2.0 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.2.0

**Alarm system polish:**
- Alarm access from the note list toolbar and the note context menu
- Scheduler and config-dialog UX refinements (spin-edit alignment, preview formatting, paused-state handling)
- New alarm tray/taskbar icon artwork

**Note list:**
- ATnotes-style column drag indicator: two red triangle arrows above and below the header, joined by a red stripe at the insertion gap — replaces Windows' pale default insertion line
- Toolbar button order polished (delete before alarm)

**In-note search:**
- New "Whole word only" option: matches are accepted only when neither neighbour is a word character (letter, digit, or underscore)

**Tray:**
- Custom hover balloon tooltip replaces the default szTip tooltip — richer layout, instant dismissal on click

**UX:**
- Delete-note confirmation now defaults to Yes (folder-delete stays defaulting to No)

### Download

| Package | Description |
|---------|-------------|
| **UltraNote-Portable.zip** | Standalone EXE + language files — no installation needed |

### Features

- Sticky notes with drag, resize, multi-select, and always-on-top
- Per-note alarms with flexible repetition and snooze
- Note list with sorting, search (with highlighting), hover preview, folder management, and column visibility toggles
- In-note find dialog with case-sensitive and whole-word options
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
