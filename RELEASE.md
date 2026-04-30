## UltraNote v1.5.0 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.5.0

**New per-note "Hide" hotkey:**
- Configurable shortcut (default **Alt+H**) hides the focused note — equivalent to the "Hide" entry in the note context menu
- Also works in the note list: select one or more notes and press Alt+H to hide them all at once

**Hotkey suffixes in context menus:**
- The currently configured shortcut for Delete, Always-on-Top and Hide is shown next to each menu entry (e.g. *Delete\tDel*, *Always on Top\tAlt+O*, *Hide\tAlt+H*)
- Suffixes update live when the user changes a shortcut in *Settings → Keyboard*

**Settings dialog stays in front:**
- When the note list is open, the settings dialog is owned by it — Windows keeps it above its owner in the z-order, so it can no longer disappear behind the note list when it loses focus

**Note list activation fixes:**
- The title bar now lights up correctly the first time the note list is opened after program start (previously it stayed in the inactive style)
- The note list is pre-created during application start, so the very first `Show()` follows the same warm activation path as every later open

**More predictable note opening:**
- Opening a note from the note list always shows it in display mode — press Enter or double-click on the text to enter edit mode
- Alt-modified shortcuts (Alt+H, Alt+O, …) now fire reliably in both note windows and the note-list view; previously `WM_SYSKEYDOWN` was not handled, so Alt-only shortcuts silently did nothing
- The hotkey control in *Settings → Keyboard* explicitly accepts all modifier combinations (`HKM_SETRULES(0, 0)`), preventing the Alt modifier from being stripped on some shells

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
