## UltraNote v1.1.0 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.1.0

**Alarm system (new):**
- Per-note alarms with 8 repetition types: Once, Daily, Every N days, Weekly (with weekday selection), Monthly on day, Monthly on Nth weekday, Quarterly, Yearly
- 3 end conditions: Never, After N occurrences, On specific date
- Topmost popup notification with sound loop; Close / Snooze / Open Note actions
- Configurable snooze minutes and paused state
- Three new note list columns: Next alarm, Interval, Status (● active / ○ paused or expired)
- Header context menu to toggle column visibility
- Alarm config dialog with live preview of the next fire time

**Find in note (new):**
- Per-note Find dialog (Ctrl+F equivalent via context menu) with case-sensitive toggle
- Lazy edit mode: opening the dialog does not dirty the view
- Pre-scroll to target line before selecting the match — minimizes viewport flash
- Matches stay visible while the find dialog has focus (ES_NOHIDESEL)

**Note list improvements:**
- Fixed "Unfiled" folder filter (notes with no folder assigned) with red-slashed folder icon
- Current folder is check-marked in the note context menu (single-select only)
- Robust note list foreground restore — handles minimized state and Windows foreground lock via AttachThreadInput
- Rename dialog now uses Segoe UI 9pt (Win 10/11 system font) instead of MS Sans Serif

**Editor bugfixes:**
- WYSIWYG wordwrap drift between edit and view modes eliminated — EM_SETRECTNP forces edit rectangle to match view-mode DrawText
- Cursor position jumps to end when entering edit mode on long notes (EM_SCROLLCARET after EM_SETSEL)

### Download

| Package | Description |
|---------|-------------|
| **UltraNote-Portable.zip** | Standalone EXE + language files — no installation needed |

### Features

- Sticky notes with drag, resize, multi-select, and always-on-top
- Per-note alarms with flexible repetition and snooze
- Note list with sorting, search (with highlighting), hover preview, folder management, and column visibility toggles
- In-note find dialog with case-sensitive option
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

- [Source Code](https://gitlab.com/HJS-cpu/ultranote)
- [All Releases](https://gitlab.com/HJS-cpu/ultranote/-/releases)
