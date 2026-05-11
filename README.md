# UltraNote

A modern recreation of **ATnotes** (the classic sticky notes app from 2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

---

## Features

- **Sticky Notes** — create, edit, drag, resize, multi-select, always-on-top, folders, attachments, clickable URLs
- **Note List** — sortable overview with search, hover preview, toolbar, context menu, reorderable columns
- **Alarms** — per-note alarms with flexible repetition, snooze, and popup
- **Find in Note** — modeless per-note search with case-sensitive and whole-word options
- **Settings Dialog** — five tabs: Layout, Keyboard, General, Misc, Note List
- **Localization** — full English and German support via INI-based language files
- **Global Hotkeys** — Ctrl+Shift+N for new note, Ctrl+Shift+L for note list; configurable per-note shortcuts (Delete, Always-on-Top, Hide) shown next to their menu entries

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

### v1.7.0 (2026-05-11)
- **Run at Windows startup:** new toggle in the General settings tab; writes a single `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\UltraNote` value (the only registry use in the project) and self-heals the path on the next launch if the EXE was moved
- **Live preview for the initial-text template:** the Misc tab shows the resolved text right below the editor — strftime variables (`%x`, `%X`, `%#c`, …) are expanded in real time against the current Windows locale
- **Robust format handling:** `ExpandInitialText` now validates each `%`-token individually, so partial or invalid specifiers (`%`, `%q`, `%E` without a letter) are passed through literally instead of triggering MSVC's `wcsftime` debug assertion

### v1.6.0 (2026-05-04)
- **Import & Export:** new `.unote` JSON format — export the current selection (or all visible notes) and import on another machine; imports always get fresh IDs so duplicates are never overwritten, unknown folders are auto-created, off-screen positions are clamped back on-screen
- **Print notes:** print one or more notes from the note window or the note list (toolbar / file menu / context menu) via the standard Windows print dialog; always black-on-white for legibility
- **New toolbar icons:** dedicated `import`, `export`, and `print` icons replace the previous Shell stock glyphs

### v1.5.0 (2026-04-30)
- **New per-note hotkey "Hide":** configurable shortcut (default Alt+H) hides the focused note — also works for all selected notes in the note list
- **Hotkey suffixes in context menus:** the configured shortcut for Delete, Always-on-Top, and Hide is shown next to each menu entry in both the note and the note-list context menu, kept in sync with the settings tab
- **Settings dialog stays in front:** when the note list is open, the settings dialog is owned by it — it can no longer be hidden behind the note list when it loses focus
- **Note list activation fixes:** title bar now activates correctly the first time the note list is opened after start-up; the note list window is pre-created during application start so the first `Show()` runs the warm activation path
- **Open from list never enters edit mode:** opening a note from the note list shows it in display mode; press Enter or double-click to edit
- **Alt-modified shortcuts work in display mode:** `WM_SYSKEYDOWN` is now handled in note windows and the note-list view, so Alt+H (Hide) and Alt+O (Always-on-Top) actually fire
- **Hotkey control accepts every modifier combination:** `HKM_SETRULES(0, 0)` ensures the hotkey edit returns the user's input verbatim, including Alt-only

### v1.4.0 (2026-04-24)
- **Note list stays open:** the minimize button is disabled and `SC_MINIMIZE` is blocked — the window can no longer be minimized via title bar, system menu, Win+Down, or taskbar right-click
- **Smaller binary:** all `.ico` resource frames re-encoded as PNG (BMP-DIB → PNG, lossless), shrinking the embedded resources by ~99 KB and the Release EXE by ~105 KB (−15 %)
- **Tooling:** reusable `tools/ico_to_png.py` converter with pixel-roundtrip verification

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
