## UltraNote v1.8.5 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.8.5

The complete remediation of a full code audit — **74 verified findings fixed** across data safety, crashes, alarms, the note list, settings, performance, and HiDPI. No new features, no new dependencies.

**Data safety:**
- Text typed in edit mode is no longer lost on Windows shutdown, sign-out, or tray-exit
- A damaged `notes.json` is backed up to `notes.json.bak` and reported, instead of silently loading only part of your notes and overwriting the rest on the next save
- Saves are flushed all the way to disk before the atomic rename — no zero-byte or partial file on power loss / USB removal
- BOM-prefixed and hand-edited storage files load reliably; stored note IDs are healed so a new note can never collide with an existing one

**Crash & stability fixes:**
- Several use-after-free / dangling-pointer paths closed (note context menus, localized-string lookup)
- Localized strings are never passed as `printf` format strings; the About dialog can't overflow on an over-long translated title
- Out-of-range timestamps no longer trip a debug assertion in the print / date-column code; clipboard copy handles allocation failure cleanly
- Modeless dialogs (alarm, find) tear down cleanly without a stray `DefWindowProc(nullptr)`

**Alarms:**
- "Sound only" alarms work — a sound without a popup window (the popup/sound checkboxes were partly inverted, and "sound only" was silent)
- Quarterly alarms fire in the current quarter instead of skipping a quarter ahead; the default start time near midnight no longer lands in the past; the end date survives Feb 29
- Stacked alarm popups no longer cut off each other's sound; the popup shows its full multi-line note preview

**Tray, windows & language:**
- The tray icon comes back after an Explorer restart or crash (it's the only way to control the app)
- Switching the UI language while the Settings dialog is open no longer makes Settings un-openable until restart
- Note-list position and columns survive closing, tray-toggle, exit, and language change; notes left off-screen are brought back on screen

**Keyboard & dialogs:**
- Full Tab / Esc / Enter navigation in the Settings, alarm, and find dialogs
- Global hotkeys now require Ctrl or Alt, so a bare key can't be captured system-wide; Shift-only and bare-key per-note rebinds fire correctly
- More keys render correctly in shortcut labels (Home/End/arrows); AltGr no longer triggers Alt-only shortcuts while typing

**Performance & leaks:**
- Sorting and populating large note lists is no longer O(n²); a dead network-drive attachment can no longer freeze the UI thread
- Multi-select actions and multi-delete write to disk once instead of once per note; GDI / icon handle leaks closed (note-list icons and fonts)

**HiDPI:**
- The tray hover bubble and the column-drag insertion markers now scale on 125–200 % monitors

**Note list:**
- Hover preview triggers only when the cursor tip is over a note row — no longer while crossing the column header

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
