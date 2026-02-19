# UltraNote - Claude Code Kontext

## Projekt

Nachbau von ATnotes (Sticky Notes fuer Windows, 2005) in modernem C++.
Detaillierte Analyse und Feature-Katalog: siehe `Projekt.md`.

## Tech-Stack

- **Sprache:** C++17 (oder C++20 wo sinnvoll)
- **UI:** Pure Win32 API - kein MFC, kein Qt, kein WTL
- **Build:** Visual Studio 2022 / CMake
- **Compiler:** MSVC
- **Externe Abhaengigkeiten:** Keine
- **Ziel:** Einzelne EXE ohne Runtime-Abhaengigkeiten
- **Portabel:** Alle Daten (Notizen, Einstellungen) im EXE-Verzeichnis, keine Registry, kein AppData

## Projektstruktur

```
UltraNote/
  CMakeLists.txt              # Build-System (VS 2022, C++17, statische CRT)
  src/
    main.cpp                  # wWinMain Entry Point
    Resource.h                # IDs: Menues, Icons, Messages, Timer
    Utils.h                   # RAII GDI-Wrapper, Font/Pfad-Helper, Shell-Icon-Konvertierung
    Note.h                    # Datenmodell (NoteData, NoteLayout)
    Storage.h/cpp             # JSON Persistenz (Notizen + Settings, hand-geschriebener Parser)
    Application.h/cpp         # Singleton: Tray, Notiz-Lifecycle, Selektion, Show/Hide All
    NoteWindow.h/cpp          # Notiz-Fenster: Owner-Draw, Drag, Resize, Edit
    NoteListWindow.h/cpp      # Notizliste: Toolbar, ListView, Sortierung, Settings-Persistenz
  lang/
    en.lng                    # Englische Sprachdatei (INI-Format)
    de.lng                    # Deutsche Sprachdatei (INI-Format)
  res/
    UltraNote.rc              # Menues, Icons, String-Tabelle
    UltraNote.ico             # Programm-Icon (Platzhalter)
    UltraNote.manifest        # ComCtl v6, DPI PerMonitorV2
  build/                      # CMake Build-Output (nicht eingecheckt)
```

## Coding-Konventionen

- **Klassen:** PascalCase (`NoteWindow`, `AlarmManager`)
- **Methoden:** PascalCase (`CreateNote`, `SaveToFile`)
- **Member-Variablen:** m_camelCase (`m_noteText`, `m_backgroundColor`)
- **Konstanten/Enums:** ALL_CAPS (`WM_NOTE_CREATED`, `MAX_NOTES`)
- **Dateinamen:** PascalCase passend zur Hauptklasse (`NoteWindow.h/cpp`)
- **Kommentare im Code:** Englisch
- **UI-Strings:** Deutsch und Englisch (Lokalisierung via String-Tabelle)
- **Einrueckung:** 4 Spaces, kein Tab

## Regeln

- Keine externen Libraries einfuehren
- Kein MFC, kein ATL (pure Win32 + C++ STL)
- Unicode durchgaengig (WCHAR / std::wstring, UNICODE Define)
- Speicher: RAII und Smart Pointers wo moeglich, raw Win32 Handles via Wrapper
- Fehlerbehandlung: Win32 Fehlercodes pruefen, HRESULT wo angebracht
- Keine C-Style Casts, stattdessen static_cast/reinterpret_cast
- Windows 10 als Minimum-Zielplattform

## Design-Prinzipien

- **Portabel:** Kein Registry-Zugriff, keine festen Pfade. Alles relativ zum EXE-Verzeichnis.
- **Minimal:** Moeglichst wenig Spezial-Komponenten. Standard Win32 Controls bevorzugen.
- **Kernfokus:** Verwaltung mehrerer Notizen wie im Original ist Prioritaet. Alarm und Netzwerk sind spaetere Phasen.

## Build

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
cmake --build build --config Release
```

CMake-Pfad (falls nicht im PATH): `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`

## Aktuelle Phase

**Phase 1 - MVP** (abgeschlossen)

Implementiert: WinMain, Message Loop, System Tray (mit Kontextmenue), Notiz-Fenster (erstellen, bearbeiten, verschieben, resizen), Owner-Draw Rendering, Edit-Modus (EDIT Control, CTRL+ENTER/ESC), einfaches Layout (Farben, Font), JSON-Persistenz (notes.json im EXE-Verzeichnis, Auto-Save 30s), Loeschen mit Bestaetigungsdialog, Mehrfachauswahl (CTRL+Klick, gepunkteter Rahmen, gemeinsames Verschieben), Notizliste (ListView, Sortierung), Rechtsklick-Kontextmenue, Always on Top, Copy to Clipboard, Single-Instance Check, Lokalisierung (INI-basiert .lng-Dateien, Deutsch/Englisch), Menue-Icons (Shell-Stock-Icons via SHGetStockIconInfo).

**Phase 1.5 - Notizliste Erweiterungen** (abgeschlossen)

Implementiert: Icon-Toolbar in Notizliste (New, Edit, Delete, Show All, Hide All; Shell-Stock-Icons in ImageList, NM_CUSTOMDRAW fuer Hintergrundfarbe, TBN_GETINFOTIP fuer lokalisierte Tooltips), Settings-Persistenz (settings.json: Fensterposition/-groesse und Spaltenbreiten der Notizliste werden gespeichert/geladen, Monitor-Validierung), Bugfix Show/Hide All (isHidden-Flag korrekt setzen, Fenster fuer versteckte Notizen erstellen), Bugfix Settings-Icon (SIID_SETTINGS durch SIID_WORLD ersetzt wegen Win10-Kompatibilitaet).

**Naechste Phase: Phase 2 - Kern-Features**

## Referenz

Das ATnotes-Original ist installiert unter: `C:\Tools\Sonstige Tools\ATnotes\`
Die extrahierte CHM-Hilfe liegt unter: `C:\Users\HJS\AppData\Local\Temp\atnotes_chm\`
