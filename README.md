# UltraNote

A modern recreation of **ATnotes** (the classic sticky notes app from 2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

---

## Features

### Sticky Notes

| Feature | Description |
|---------|-------------|
| **Create & Edit** | Click to create, double-click or Enter to edit |
| **Drag & Resize** | Move notes freely, resize from any edge |
| **Multi-Select** | Ctrl+Click to select multiple notes, move them together |
| **Always on Top** | Pin individual notes above all windows |
| **Hide / Show** | Hide notes without deleting them |
| **Paste as Note** | Create a new note directly from clipboard content |
| **Copy to Clipboard** | Copy note text with a single keystroke |
| **Folders** | Organize notes into custom folders |
| **Clickable Links** | URL's rendered as blue underlined links, single-click to open in browser (toggleable) |
| **Attachments** | Drag & drop files onto notes, click to open, stored as path references |

### Note List

| Feature | Description |
|---------|-------------|
| **Overview** | Sortable list of all notes with title, text preview, folder, and last modified timestamp |
| **Quick Actions** | Toggle visibility and always-on-top directly via checkboxes — list updates instantly |
| **Hover Preview** | Hover over a note entry to preview it on screen |
| **Toolbar** | New, Edit, Rename, Delete, Alarm, Show All, Hide All with icon toolbar |
| **Context Menu** | Right-click for rename, folder assignment, alarm, and more; multi-select aware |
| **Status Bar** | Shows total / filtered note count |
| **Search & Highlight** | Live search across note titles and text; matches highlighted in orange inside opened notes, first match auto-selected on edit |
| **Column Reordering** | Drag & drop columns in the header with red insertion-arrow indicator at the target gap; order persists across sessions |
| **Zebra Striping** | Optional alternating row background (configurable) |

### Alarms

| Feature | Description |
|---------|-------------|
| **Per-note alarms** | Attach an alarm to any note, opened via toolbar or note context menu |
| **Repetition** | Once, Daily, Every N days, Weekly (weekday picker), Monthly on day, Monthly on Nth weekday, Quarterly, Yearly |
| **End conditions** | Never, After N occurrences, On specific date |
| **Popup** | Topmost popup with sound loop; Close / Snooze / Open Note actions |
| **Snooze & pause** | Configurable snooze minutes; pause individual alarms without losing settings |
| **List columns** | Next alarm, Interval, Status (● active / ○ paused or expired) — visibility toggleable |

### Find in Note

| Feature | Description |
|---------|-------------|
| **Per-note dialog** | Modeless find dialog opened from the note context menu |
| **Options** | Case-sensitive, Whole word only (word chars = letters, digits, underscore) |
| **Wrap-around** | Continues from the top when the bottom is reached; viewport pre-scrolls to the match |

### Settings Dialog

| Tab | Options |
|-----|---------|
| **Layout** | Default background, text, and border colors; font selection; live preview |
| **Keyboard** | Customize all shortcuts (2 local + 2 global hotkeys) |
| **General** | Autosave interval, delete confirmation, preview toggle & delay, clickable links toggle, tray behavior, language |
| **Misc** | New note position, cascade step/reset, default folder, initial text with strftime variables |
| **Note List** | Date format (YYYY-MM-DD or DD.MM.YYYY), zebra striping toggle |

### Localization

- Full **English** and **German** language support
- INI-based language files — easy to add new languages
- Live language switching without restart

### Global Hotkeys

| Shortcut | Action |
|----------|--------|
| **Ctrl+Shift+N** | Create new note from anywhere |
| **Ctrl+Shift+L** | Toggle note list from anywhere |

---

## Download

> **Portable:** Single EXE, no installation needed. All data (notes, settings) stored next to the executable.

Download the latest version from [GitHub Releases](https://github.com/HJS-cpu/UltraNote/releases) or [GitLab Releases](https://gitlab.com/HJS-cpu/ultranote/-/releases).

---

## System Requirements

- Windows 10 or later
- No installation required
- No external dependencies
- No registry access — fully portable

---

## Architecture

| Aspect | Detail |
|--------|--------|
| **Language** | C++17, pure Win32 API |
| **UI Framework** | No MFC, no Qt, no WTL — raw Win32 |
| **Build System** | CMake + Visual Studio 2022 |
| **Dependencies** | None — single EXE with static CRT |
| **Data Storage** | JSON files in EXE directory (`notes.json`, `settings.json`) |
| **Unicode** | Full Unicode support throughout (UTF-16) |

### Project Structure

```
UltraNote/
├── src/
│   ├── main.cpp              # Entry point
│   ├── Application.h/cpp     # Singleton: tray, note lifecycle, hotkeys
│   ├── NoteWindow.h/cpp      # Sticky note window: owner-draw, drag, resize
│   ├── NoteListWindow.h/cpp  # Note list: toolbar, ListView, sorting
│   ├── SettingsDialog.h/cpp  # Settings dialog with 5 tabs
│   ├── FindInNoteDialog.h/cpp # Modeless per-note find dialog
│   ├── AlarmScheduler.h/cpp  # Alarm scheduling utility
│   ├── AlarmPopupWindow.h/cpp # Topmost alarm popup
│   ├── AlarmConfigDialog.h/cpp # Alarm configuration dialog
│   ├── TrayBubbleWindow.h/cpp # Custom tray hover balloon
│   ├── HeaderDragOverlay.h/cpp # Red arrow column-insertion indicator
│   ├── Storage.h/cpp         # JSON persistence (hand-written parser)
│   ├── Localization.h/cpp    # INI-based language system
│   ├── Note.h                # Data model (NoteData, NoteLayout, AlarmConfig)
│   ├── Utils.h               # RAII GDI wrappers, helpers
│   └── Resource.h            # IDs for menus, icons, controls
├── lang/
│   ├── en.lng                # English
│   └── de.lng                # Deutsch
├── res/
│   ├── UltraNote.rc          # Resources
│   ├── UltraNote.ico         # Application icon
│   └── UltraNote.manifest    # ComCtl v6, DPI awareness
├── .github/workflows/build.yml # GitHub Actions CI
└── .gitlab-ci.yml              # GitLab CI: build → upload → release
```

---

## Building from Source

### Prerequisites

- Visual Studio 2022
- CMake 3.20+

### Build

```batch
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

The executable and language files will be in `build/Release/`.

### CI/CD

Every push to `master` triggers automatic builds on both GitHub Actions and GitLab CI. Tagged commits (`v*`) create releases with portable ZIP downloads on both platforms.

---

## Credits

- **Inspired by** ATnotes by Thomas Ascher
- **Modern recreation:** HJS (2026)

---

## Changelog

### v1.3.0 (2026-04-23)
- **In-place edit context menu:** right-click inside a note while editing opens a dedicated editor menu with Paste/Select all/Cut/Copy/Delete, a Date/Time submenu (12 strftime formats with live preview), and Insert file path via the system picker
- **Custom edit-menu icons:** Cut, Select all, Date/Time and Insert-path now ship as dedicated `.ico` artwork; Segoe MDL2 Assets / Segoe Fluent Icons glyph rendering removed entirely
- **Edit-mode modal guard:** `GetOpenFileNameW` no longer tears down the EDIT control (`m_editModalOpen` short-circuits `WM_KILLFOCUS`)

### v1.2.0 (2026-04-21)
- **Column drag indicator:** ATnotes-style red arrows above and below the header, connected by a red stripe, mark the insertion gap while dragging a column — replaces Windows' pale default insertion line
- **Whole-word search:** in-note find dialog gains a "Whole word only" option; word boundary uses Unicode letter/digit/underscore
- **Alarm access:** open the alarm config directly from the note list toolbar and the note context menu
- **Alarm polish:** scheduler and config-dialog UX refinements (spin-edit alignment, preview formatting, paused-state handling); new alarm icon artwork
- **Tray tooltip:** custom hover balloon window replaces the default szTip tooltip
- **Delete default:** note-delete confirmation now defaults to Yes (folder-delete stays defaulting to No)

### v1.1.0 (2026-04-18)
- **Alarm system:** per-note alarms with 8 repetition types (Once, Daily, Every N, Weekly, Monthly on day / Nth weekday, Quarterly, Yearly); 3 end conditions; topmost popup with sound + snooze; 3 new note list columns (Next, Interval, Status) with visibility toggle
- **Find in note:** per-note find dialog with case-sensitive toggle, viewport pre-scroll, and selection that persists while the dialog has focus
- **Alarm config dialog UX:** two aligned edit columns, UpDown spinners on all numeric fields, section separators, vertically centered spinner boxes matching radio button midlines
- **Unfiled folder filter:** notes without a folder get their own list entry (red-slashed folder icon)
- **Editor fixes:** eliminated wordwrap drift between edit and view modes (EM_SETRECTNP); cursor now scrolls into view on entering edit mode for long notes
- **Note list polish:** current folder check-marked in context menu; robust foreground restore for minimized window; Segoe UI rename dialog

### v1.0.0 (2026-04-16)
- **Search highlighting:** matches from note list search are highlighted in orange across all open notes; first match auto-selected when entering edit mode
- **Text rendering fix:** eliminated sub-pixel drift between edit and view modes — characters no longer shift when leaving edit mode
- **Instant hidden-state sync:** note list checkbox updates immediately on any visibility change (toolbar, context menu, or list click)
- **Compact rename dialog:** window now sizes exactly to its controls regardless of DPI

### v0.9 Beta (2026-04-15)
- New Settings tab **Note List** with date format selector and zebra striping toggle
- Column reordering via drag & drop, persisted across sessions
- "Last Modified" column replacing "Created"
- Multi-select aware context menu (Edit/Rename grayed for multi-select)
- Compact rename dialog (180x58 DLU, right-aligned buttons)
- Preview no longer jumps already-visible notes; pauses when settings dialog is open
- Header sort arrows (up/down) reliable after first click
- Various sort and focus fixes

### v0.8 Alpha (2026-04-14)
- Clickable URL detection in notes (http, https, ftp, www) — blue underlined links with hand cursor
- Toggleable via Settings > General > Display
- Attachment bar: drag & drop files onto notes, click to open
- Initial text variables (strftime) for new notes
- Status bar in note list showing total/filtered count
- Fix About dialog link typo

### v0.6 Alpha (2026-02-21)
- Custom icon set replacing Windows shell stock icons
- New icons: New, Search, About, Pin, ShowAll, HideAll, Delete, Copy, Exit, Group
- Settings dialog: group headers with icons in General tab
- Search function in note list toolbar with live filtering

### v0.5 Alpha (2026-02-20)
- Initial public release
- Sticky notes with full editing, drag, resize, and multi-select
- System tray with context menu
- Note list with sorting, toolbar, hover preview, and folder management
- Settings dialog (Layout, Keyboard, General, Misc)
- Global hotkeys (Ctrl+Shift+N, Ctrl+Shift+L)
- Paste clipboard as new note
- About dialog with version info
- English and German localization
- JSON-based portable storage
- GitLab CI pipeline
