# UltraNote

[![Build UltraNote](https://github.com/HJS-cpu/UltraNote/actions/workflows/build.yml/badge.svg)](https://github.com/HJS-cpu/UltraNote/actions/workflows/build.yml)

A modern recreation of **ATnotes** (the classic sticky notes app from 2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

![Windows 10/11](https://img.shields.io/badge/Windows-10%2F11-blue?logo=windows)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)
![License](https://img.shields.io/badge/License-Proprietary-red)

---

## ✨ Features

### 📝 Sticky Notes

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

### 📋 Note List

| Feature | Description |
|---------|-------------|
| **Overview** | Sortable list of all notes with title, text preview, folder, and timestamps |
| **Quick Actions** | Toggle visibility and always-on-top directly via checkboxes |
| **Hover Preview** | Hover over a note entry to preview it on screen |
| **Toolbar** | New, Edit, Delete, Show All, Hide All with icon toolbar |
| **Context Menu** | Right-click for rename, folder assignment, and more |

### ⚙️ Settings Dialog

| Tab | Options |
|-----|---------|
| **Layout** | Default background, text, and border colors; font selection; live preview |
| **Keyboard** | Customize all shortcuts (7 local + 2 global hotkeys) |
| **General** | Autosave interval, delete confirmation, preview toggle & delay, tray behavior, language |
| **Misc** | New note position, cascade step/reset, default folder |

### 🌐 Localization

- Full **English** and **German** language support
- INI-based language files — easy to add new languages
- Live language switching without restart

### 🔑 Global Hotkeys

| Shortcut | Action |
|----------|--------|
| **Ctrl+Shift+N** | Create new note from anywhere |
| **Ctrl+Shift+L** | Toggle note list from anywhere |

---

## 📥 Download

### 💾 Latest Build

**[⬇️ Download from GitHub Actions](https://github.com/HJS-cpu/UltraNote/actions/workflows/build.yml)**

Select the latest successful run and download the **UltraNote-Release** artifact.

> **Portable:** Single EXE, no installation needed. All data (notes, settings) stored next to the executable.

---

## 🖥️ System Requirements

- Windows 10 or later
- No installation required
- No external dependencies
- No registry access — fully portable

---

## 🏗️ Architecture

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
│   ├── SettingsDialog.h/cpp  # Settings dialog with 4 tabs
│   ├── Storage.h/cpp         # JSON persistence (hand-written parser)
│   ├── Localization.h/cpp    # INI-based language system
│   ├── Note.h                # Data model (NoteData, NoteLayout)
│   ├── Utils.h               # RAII GDI wrappers, helpers
│   └── Resource.h            # IDs for menus, icons, controls
├── lang/
│   ├── en.lng                # English
│   └── de.lng                # Deutsch
├── res/
│   ├── UltraNote.rc          # Resources
│   ├── UltraNote.ico         # Application icon
│   └── UltraNote.manifest    # ComCtl v6, DPI awareness
└── .github/workflows/
    └── build.yml             # CI: Debug + Release builds
```

---

## 🛠️ Building from Source

### Prerequisites

- Visual Studio 2022
- CMake 3.20+

### Build

```batch
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

The executable and language files will be in `build/Release/`.

### CI / GitHub Actions

Every push to `master` triggers automatic Debug and Release builds. Download the Release artifact from the [Actions](https://github.com/HJS-cpu/UltraNote/actions) tab.

---

## 🎯 Roadmap

- [x] **Phase 1** — MVP: Notes, tray, persistence, multi-select, localization
- [x] **Phase 1.5** — Note list toolbar, settings persistence, bug fixes
- [x] **Phase 1.7** — Note list preview, release build optimization
- [x] **Phase 2** — Settings dialog, global hotkeys, about dialog
- [ ] **Phase 3** — Alarm system, print support, minimize/restore
- [ ] **Phase 4** — Network features, note templates, import/export

---

## 🙏 Credits

- **Original concept:** [ATnotes](http://www.atnotes.de/) by Thomas Ascher (1999–2005)
- **Modern recreation:** HJS (2026)

---

## 📝 Changelog

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
- GitHub Actions CI pipeline
