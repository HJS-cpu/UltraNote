## UltraNote v1.0.0 — Portable Sticky Notes for Windows

A modern recreation of ATnotes (2005) built with pure Win32 API and C++17. Lightweight, portable, and dependency-free.

### What's New in v1.0.0

- **Search highlighting:** Matches from the note list search are now highlighted in orange across all open notes. First match is automatically selected when entering edit mode.
- **Pixel-perfect text rendering:** Fixed sub-pixel drift between edit and view modes — text no longer shifts when leaving edit mode.
- **Instant hidden-state sync:** The note list checkbox updates immediately whenever a note's visibility changes (toolbar, context menu, or list click).
- **Compact rename dialog:** Rename window now sizes exactly to its controls, no more empty space at higher DPI.

### Download

| Package | Description |
|---------|-------------|
| **UltraNote-Portable.zip** | Standalone EXE + language files — no installation needed |

### Features

- Sticky notes with drag, resize, multi-select, and always-on-top
- Note list with sorting, search (with highlighting), hover preview, and folder management
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
