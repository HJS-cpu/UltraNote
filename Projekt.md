# UltraNote - Machbarkeitsanalyse

> Arbeitstitel: **UltraNote**
> Ziel: Nachbau von ATnotes (v9.5, 2005) in modernem C++ mit zeitgemaesser Optik
> Sprache: C++17/20, Win32 API
> Datum: 2026-02-19

---

## 1. Das Original: ATnotes

ATnotes ist eine MFC-basierte (Microsoft Foundation Classes) Win32-Anwendung (~1 MB, PE32 i386) von Thomas Ascher aus Innsbruck (Oesterreich). Erstveroeffentlichung 1999, letzte Version 9.5 vom Januar 2005. Freeware, nicht Open Source, nicht mehr gepflegt.

Das Programm erstellt virtuelle Haftnotizen auf dem Windows-Desktop mit umfangreichen Verwaltungs-, Alarm- und Netzwerk-Funktionen.

---

## 2. Vollstaendiger Feature-Katalog (aus CHM-Hilfedatei extrahiert)

### 2.1 Notiz-Fenster (Kern)

- Erstellen (New Note) mit waehlbarem Layout
- Bearbeiten per Doppelklick oder Taste E/ENTER
  - ENTER = neue Zeile, CTRL+A = alles markieren, CTRL+ENTER = speichern, ESC = abbrechen
  - Automatisches Speichern bei Fokusverlust
  - Kontextmenue waehrend Bearbeitung (Textbearbeitungs-Befehle)
- Verschieben per Drag (Klick innerhalb der Notiz)
- Groesse aendern per Drag am Rand
- Mehrfachauswahl mit CTRL+Klick (gepunkteter Rahmen)
  - Mehrere Notizen gleichzeitig verschieben (aber nicht resizen)
- Minimieren auf erste Textzeile (Taste M oder -)
- Wiederherstellen auf volle Groesse (Taste R oder +)
- Kopieren in Zwischenablage (Text, Bitmap, Layout-Format)
- Einfuegen (Text und/oder Layout aus Zwischenablage)
- Paste Note: neue Notiz aus Zwischenablage erstellen
- Loeschen (mit optionaler Bestaetigungsdialog, Papierkorb)
  - SHIFT+DEL umgeht Papierkorb
- Drucken (automatisch auf Standarddrucker, Farbe, Zoom, mit/ohne Rahmen)
- Undo fuer Edit/Paste/Delete
- Always on Top (pro Notiz oder global)
- URL/Mail/Datei-Erkennung und Oeffnen via ShellExecute
- Anzeige ">>" wenn Text nicht komplett sichtbar
- Temporaere Vollgroesse bei Mouse-Hover (wenn zu klein)
- Schlagschatten (XP+)
- Tab-Breite konfigurierbar

#### Tastenkuerzel im Notiz-Fenster

| Taste | Funktion |
|-------|----------|
| ENTER / E | Bearbeiten |
| C | Kopieren |
| V | Einfuegen |
| DEL / D | Loeschen |
| P | Drucken |
| - / M | Minimieren |
| + / R | Wiederherstellen |
| H | Verstecken/Zeigen |
| SPACE / A | Alarm setzen/entfernen |
| S | An anderen PC senden |
| O | Always on Top umschalten |
| T | Layout-Einstellungen oeffnen |

### 2.2 Layout-System

- Schriftart (Font-Dialog)
- Textfarbe (manuell oder "Automatisch" = schwarz/weiss je nach Kontrast zum Hintergrund)
- Hintergrundfarbe (einfarbig oder Gradient-Start/End)
- Rahmenfarbe (manuell oder "Automatisch" = gleich Hintergrund)
- Textausrichtung: links, zentriert, rechts
- Gradient: keiner, horizontal, vertikal (Win2000/XP+)
- Desktop-Farben automatisch uebernehmen
- Zufallsfarben (komplett zufaellig oder aus benutzerdefinierter Farbliste)
- Transparenz:
  - Color-Key-Transparenz (bestimmte Farbe komplett transparent, Mausklicks durchreichen)
  - Stufenlose Transparenz (gesamte Notiz, von unsichtbar bis voll sichtbar)
- Layout-Vorlagen: speichern, laden, loeschen (Standard-Layout nicht loeschbar)

### 2.3 Alarm-System

- Aktivieren/Deaktivieren pro Notiz
- Datum und Uhrzeit
- Ablaufdatum (optional, mit optionalem Auto-Delete bei Ablauf)

#### Alarm-Typ
- Notiz behalten nach Alarm
- Notiz loeschen nach Alarm
- Wiederholen alle X Minuten/Stunden/Tage/Wochen/Monate/Jahre
- Taeglich wiederholen an bestimmten Wochentagen zu bestimmter Uhrzeit

#### Alarm-Aktion
- MessageBox mit Notiztext (mit Snooze "Remind again in X minutes" und Alarm-Einstellungen)
- Bring to Top + Notiz blinkt (Klick stoppt Blinken)
- Programm/Datei ausfuehren

#### Audio-Signal
- Keines
- Standard-Beep (PC-Speaker, keine Soundkarte noetig)
- WAV-Datei abspielen (mit optionalem Repeat bis Bestaetigung)

#### Sonstiges
- Flash-Intervall in ms konfigurierbar
- Vorankuendigung in Taskbar-Icon X Minuten vorher
- Anzahl Taskbar-Icon-Flashes konfigurierbar

### 2.4 Ordner-System

- Benutzerdefinierte Ordner (erstellen, umbenennen, loeschen, Reihenfolge aendern)
- Spezial-Ordner:
  - "Heute" (Notizen mit Alarm heute oder versteckte Notizen die heute sichtbar werden)
  - "Alle Ordner"
  - "Kein Ordner"
  - Papierkorb (mit Restore-Funktion)
- Notiz-Anzahl pro Ordner in Klammern angezeigt

### 2.5 Notizliste (Note List Window)

- Sortierbare Tabelle (Klick auf Spaltenheader, aufsteigend/absteigend)
- Mehrfachauswahl (SHIFT/CTRL+Klick)
- Doppelklick = Notiz bearbeiten
- Rechtsklick = Notiz-Kontextmenue

#### Filter (View-Menue)
- Alle Notizen
- Nur sichtbare Notizen
- Nur versteckte Notizen
- Nur Notizen mit Alarm
- Nur empfangene Notizen
- Suchergebnis-Notizen

#### Ansicht
- Normal / Minimiert / Wiederhergestellt
- Toolbar ein/aus
- Statusbar ein/aus
- Konfigurierbare Spalten
- Alternating Row Colors
- Grosse/kleine Ordner-Icons

#### Datei-Operationen
- Export nach .atn (ATnotes-Format) oder .txt
- Import aus .atn (Notizen anhaengen)

#### Edit-Menue
- Alles auswaehlen
- Auswahl umkehren
- Ordnerliste bearbeiten

### 2.6 Verstecken (Hide)

- Nicht verstecken (wieder sichtbar machen)
- Verstecken bis manuell in Notizliste aktiviert
- Verstecken bis zu einem bestimmten Datum/Uhrzeit (automatisch sichtbar)

### 2.7 Suche

- Textsuche ueber alle Notizen
- Case-sensitive (optional)
- Nur ganze Woerter (optional)
- Suchhistorie (letzte 16 Suchen)
- Ergebnisse in Notizliste angezeigt

### 2.8 Kalender

- Resizable, Anzahl Monate passt sich an Groesse an
- Verschiebbar per Drag auf Titel-Hintergrund
- Resize am Rand
- Farblich anpassbar:
  - Schriftart
  - Hintergrundfarbe (aussen/innen Monate)
  - Textfarbe (aktueller Monat / andere Monate)
  - Titel-Hintergrund/Textfarbe
  - Color-Key-Transparenz
  - Stufenlose Transparenz
- Kalenderwochennummern anzeigen
- "Heute" am unteren Rand anzeigen
- Heutiges Datum hervorheben
- Tage mit Alarmen fett darstellen
- Tooltip mit Alarm-Details bei Mouse-Hover auf Alarm-Tagen
- Kontextmenue: Zu heute springen, Datum kopieren, Always on Top

### 2.9 Netzwerk (Send To)

- Notizen per TCP/IP an andere PCs senden
- Empfaenger: Computername oder IP-Adresse
  - Optionaler Port pro Empfaenger (Doppelpunkt-Syntax)
  - Alias-Namen (nach Leerzeichen)
  - Mehrere Empfaenger kommagetrennt
- Empfaengerliste:
  - Speichern, Loeschen, Bearbeiten (Doppelklick)
  - Reihenfolge aendern (Move Up/Down)
  - Online-Status pruefen (gruen=online, rot=offline)
  - Rechtsklick-Menue: Umbenennen, Status einzeln aktualisieren
- Netzwerk-Browser fuer verfuegbare Computer
- Netzwerk-Einstellungen:
  - Eingehende Notizen akzeptieren ja/nein
  - Layout fuer eingehende Notizen
  - Ordner fuer eingehende Notizen
  - Always on Top fuer eingehende Notizen
  - Portnummer (gleicher Port = gleiche Gruppe)
  - Absendername
  - Audio-Signal bei Empfang
- Fehlgeschlagene Zustellungen: Empfaenger bleiben aktiv fuer Retry
- Absender-Info: Computername + optionaler Name + Datum/Uhrzeit

### 2.10 System Tray

- Icon im rechten Taskbar-Bereich
- Rechtsklick = Hauptmenue
- Doppelklick = konfigurierbare Aktion
- Naechsten Alarm als Tooltip anzeigen (optional)
- Taskbar-Icon blinkt bei Alarm-Vorankuendigung

### 2.11 Globale Hotkeys

- Systemweite Tastenkombinationen (auch aktiv in anderen Programmen)
- Windows-Taste als Modifier moeglich
- Konfigurierbar fuer: New Note, Paste Note, etc.

### 2.12 Einstellungen - Neue Notizen

- Startposition (links/oben)
- Kaskadierung (Einrueckung fuer mehrere neue Notizen)
- Auto-Minimieren nach Bearbeitung
- Alarm-Fenster automatisch nach Bearbeitung oeffnen
- Standard-Ordner
- Initialer Text (mit Platzhaltern fuer Datum/Uhrzeit/Cursorposition)

### 2.13 Sonstiges

- ~~Autostart mit Windows (Registry)~~ *Entfaellt - portables Programm*
- Einfuegen: Text anhaengen oder ersetzen (konfigurierbar)
- Cursor-Position beim Bearbeiten: alles markieren oder Cursor oben/unten
- Trennlinie fuer Multi-Export/Copy (konfigurierbar)
- Doppelklick-Aktion auf Tray-Icon (konfigurierbar)
- Menue-Stil: XP-Style, Hauptmenue umgedreht
- Datenspeicherort konfigurierbar (Notizen-Datei + Papierkorb-Datei)
- Lokalisierung: Englisch, Deutsch, Benutzerdefiniert (Uebersetzungstabelle in INI)

---

## 3. Technische Architektur

### 3.1 Empfohlener Tech-Stack

| Komponente | Empfehlung |
|------------|------------|
| Sprache | C++17 oder C++20 |
| UI-Framework | Pure Win32 API (kein MFC, kein Qt) |
| Build-System | Visual Studio 2022 / CMake |
| Compiler | MSVC (Visual C++) |
| Externe Abhaengigkeiten | Keine |

#### Begruendung
- Win32 API bietet maximale Kontrolle ueber Fenster-Verhalten und Rendering
- Kein Runtime-Overhead, kleinstes moegliches Binary
- Dem Original technisch am naechsten (MFC ist ein duenner Wrapper um Win32)
- Alle benoetigten APIs sind direkt verfuegbar
- Moderne C++ Features (smart pointers, std::filesystem, std::string, etc.) reduzieren Boilerplate

#### Alternativen (verworfen)

| Alternative | Grund fuer Ablehnung |
|-------------|---------------------|
| MFC | Veraltet, unnoetige Abhaengigkeit, schwerfaellig |
| Qt | Binary-Groesse 20+ MB, anderes Look & Feel |
| WTL | Kleinere Community, weniger Dokumentation |
| C# / WPF | Kein C++, .NET-Abhaengigkeit |

### 3.2 Projekt-Struktur

```
UltraNote/
  CMakeLists.txt              // Build-System (VS 2022, C++17, statische CRT)
  src/
    main.cpp                  // wWinMain, InitCommonControlsEx        [Phase 1]
    Resource.h                // IDs: Menues, Icons, Messages, Timer   [Phase 1]
    Utils.h                   // RAII GDI-Wrapper, Font/Pfad-Helper    [Phase 1]
    Note.h                    // NoteData + NoteLayout Datenmodell     [Phase 1]
    Storage.h/cpp             // JSON Persistenz (Notizen + Settings)  [Phase 1]
    Application.h/cpp         // Singleton: Tray, Lifecycle, Selektion [Phase 1]
    NoteWindow.h/cpp          // Owner-Draw Notiz-Fenster              [Phase 1]
    NoteListWindow.h/cpp      // Toolbar + ListView Notizliste         [Phase 1]
    AlarmManager.h/cpp        // Alarm-Verwaltung und Timer            [Phase 4]
    AlarmDialog.h/cpp         // Alarm-Einstellungen Dialog            [Phase 4]
    SettingsDialog.h/cpp      // Einstellungen Dialog (Tabs)           [Phase 6]
    SearchDialog.h/cpp        // Suchdialog                            [Phase 2]
    FolderListDialog.h/cpp    // Ordner-Verwaltung                     [Phase 2]
    CalendarWindow.h/cpp      // Kalender-Fenster                      [Phase 4]
    HideDialog.h/cpp          // Verstecken-Dialog                     [Phase 3]
    SendToDialog.h/cpp        // Netzwerk-Sende-Dialog                 [Phase 5]
    NetworkManager.h/cpp      // TCP/IP Senden/Empfangen               [Phase 5]
    Layout.h/cpp              // Layout-Vorlagen                       [Phase 3]
    HotkeyManager.h/cpp       // Globale Hotkeys                       [Phase 6]
    Localization.h/cpp        // Sprachumschaltung                     [Phase 6]
  lang/
    en.lng                    // Englische Sprachdatei (INI-Format)
    de.lng                    // Deutsche Sprachdatei (INI-Format)
  res/
    UltraNote.rc              // Menues, Icons, String-Tabelle
    UltraNote.ico             // Programm-Icon
    UltraNote.manifest        // ComCtl v6, DPI PerMonitorV2
```

### 3.3 Wichtige Win32 APIs

| Feature | API |
|---------|-----|
| Notiz-Fenster | `RegisterClassEx`, `CreateWindowEx`, `WM_PAINT`, `WM_NCHITTEST` |
| Gradient | `GradientFill` (msimg32) oder GDI+ `LinearGradientBrush` |
| Transparenz (stufenlos) | `SetLayeredWindowAttributes` mit `LWA_ALPHA` |
| Transparenz (Color-Key) | `SetLayeredWindowAttributes` mit `LWA_COLORKEY` |
| Klick-Durchreichung | `WS_EX_TRANSPARENT` Extended Style |
| Schlagschatten | `CS_DROPSHADOW` Class Style |
| Abgerundete Ecken (Win11) | `DwmSetWindowAttribute` mit `DWMWA_WINDOW_CORNER_PREFERENCE` |
| System Tray | `Shell_NotifyIcon` |
| Globale Hotkeys | `RegisterHotKey` / `UnregisterHotKey` |
| WAV-Wiedergabe | `PlaySound` (winmm) |
| Drucken | `StartDoc`, `StartPage`, GDI-Zeichenbefehle |
| TCP-Netzwerk | Winsock2 (`WSAStartup`, `socket`, `connect`, `send`, `recv`) |
| Kalender-Control | `MonthCal_Class` Common Control |
| ListView | `WC_LISTVIEW` Common Control mit Custom Draw |
| Font-Dialog | `ChooseFont` Common Dialog |
| Farb-Dialog | `ChooseColor` Common Dialog |
| URL oeffnen | `ShellExecute` |
| ~~Autostart~~ | ~~Registry API~~ *Entfaellt - portabel* |
| INI-Dateien | `GetPrivateProfileString` / `WritePrivateProfileString` |
| Timer | `SetTimer` / `KillTimer` |

### 3.4 Dateiformat

Das Original verwendet ein proprietaeres `.dat`-Format (Header: "ATnotes\0" + binaere Daten).

UltraNote verwendet:
- **`notes.json`** im EXE-Verzeichnis (UTF-8, hand-geschriebener Parser/Writer)
- **`settings.json`** im EXE-Verzeichnis (Key-Value-Paare fuer UI-Einstellungen wie Fensterposition/-groesse, Spaltenbreiten)
- Atomares Speichern via `.tmp` + `MoveFileExW`
- Auto-Save alle 30 Sekunden + Save bei Exit
- Farben als `#RRGGBB` Hex-Strings
- Optionaler .atn-Import waere ein Bonus-Feature fuer spaeter

---

## 4. Machbarkeitsbewertung pro Modul

### Einfach (Standard Win32, wenig Komplexitaet)

- System Tray Icon + Kontextmenue
- Globale Hotkeys
- Autostart (Registry)
- Suche (Textsuche ueber gespeicherte Notizen)
- Ordner-System (Datenstruktur + Dialoge)
- INI/JSON-Persistenz
- URL/Mail/Datei-Erkennung + Oeffnen (`ShellExecute`)
- Drucken (GDI Print)
- Verstecken-Dialog
- Lokalisierung (String-Tabelle)

### Mittel (sorgfaeltige Implementierung noetig)

- Notiz-Fenster (Owner-Draw, Edit-Modus, Resize, Drag, Mehrfachauswahl)
- Alarm-System (Timer, Wiederholungslogik, MessageBox + Flash)
- Notizliste (ListView mit Custom Draw, Sortierung, Filter)
- Kalender (MonthCal Control oder Custom, Alarm-Hervorhebung)
- Layout-Vorlagen (Serialisierung)
- Verstecken mit Timer (Timer + Sichtbarkeitsmanagement)
- Einstellungen-Dialog (Multi-Tab)
- WAV-Wiedergabe mit Repeat

### Anspruchsvoll (aber machbar)

- Transparenz-Varianten (stufenlos + Color-Key + Klick-Durchreichung)
- Gradient-Hintergrund (GradientFill oder GDI+)
- Netzwerk-Modul (TCP-Sockets, Hintergrund-Thread, Online-Check)
- Drag & Drop mit korrektem Hit-Testing (Move vs. Resize vs. Edit)
- DPI-Awareness (Per-Monitor V2)

---

## 5. Entwicklungsplan (Phasen)

### Phase 1 - MVP (Minimum Viable Product) ✓

Ziel: Funktionierendes Basisprogramm mit Notiz-Verwaltung.

- [x] Projektstruktur anlegen (CMake, Ressourcen)
- [x] WinMain + Message Loop
- [x] System Tray Icon mit Basis-Kontextmenue (New Note, Show/Hide Notes, Note List, Exit)
- [x] Notiz-Fensterklasse (erstellen, anzeigen, verschieben, Groesse aendern)
- [x] Notiz-Bearbeitung (Doppelklick -> Edit, CTRL+ENTER -> Save, ESC -> Cancel)
- [x] Einfaches Layout (Hintergrundfarbe, Textfarbe, Font)
- [x] Persistenz: Notizen in JSON speichern/laden (portabel, im EXE-Verzeichnis)
- [x] Notizen loeschen (mit Bestaetigungsdialog)
- [x] Mehrfachauswahl mit CTRL+Klick
- [x] Notizliste (ListView, Sortierung)
- [x] Lokalisierung (INI-basierte .lng-Dateien, Deutsch/Englisch, zur Laufzeit umschaltbar)
- [x] Menue-Icons (Shell-Stock-Icons via SHGetStockIconInfo, Tray- und Notiz-Kontextmenue)
- [x] Notizliste: Icon-Toolbar (New, Edit, Delete, Show All, Hide All)
- [x] Notizliste: Settings-Persistenz (Fensterposition/-groesse, Spaltenbreiten in settings.json)
- [x] Bugfix: Show/Hide All setzt isHidden-Flag korrekt, erstellt Fenster fuer versteckte Notizen
- [x] Bugfix: SIID_SETTINGS durch SIID_WORLD ersetzt (Win10-Kompatibilitaet)

### Phase 2 - Kern-Features

Ziel: Vollstaendige Notiz-Verwaltung wie im Original.

- [ ] Kopieren/Einfuegen (Text + Layout)
- [ ] Paste Note (neue Notiz aus Zwischenablage)
- [ ] Undo fuer Edit/Paste/Delete
- [ ] Minimieren/Restore
- [ ] Ordner-System (Erstellen, Umbenennen, Loeschen, Zuweisen)
- [ ] Suche (Text, Case-Sensitive, Whole Word)
- [ ] Papierkorb mit Restore
- [ ] Drucken
- [ ] URL/Mail/Datei-Erkennung und Oeffnen

### Phase 3 - Optik und Layout

Ziel: Erweiterte Darstellungsoptionen.

- [ ] Layout-Vorlagen (speichern, laden, loeschen)
- [ ] Gradient-Hintergrund (horizontal, vertikal)
- [ ] Transparenz (stufenlos + Color-Key)
- [ ] Schlagschatten
- [ ] Desktop-Farben / Zufallsfarben
- [ ] Verstecken (manuell + zeitgesteuert)

### Phase 4 - Alarm und Kalender

Ziel: Erinnerungsfunktionen.

- [ ] Alarm-System komplett (einmalig, wiederkehrend, taeglich)
- [ ] Alarm-Aktionen (MessageBox/Snooze, Bring-to-Top+Flash, Run)
- [ ] Alarm-Audio (Beep, WAV, WAV-Repeat)
- [ ] Kalender-Fenster (mit Alarm-Hervorhebung)

### Phase 5 - Netzwerk

Ziel: Notizen zwischen PCs austauschen.

- [ ] TCP-Server (Hintergrund-Thread, eingehende Notizen empfangen)
- [ ] TCP-Client (Notizen senden)
- [ ] Send-To-Dialog (Empfaengerliste, Online-Status)
- [ ] Netzwerk-Einstellungen (Port, Absendername, akzeptieren ja/nein)

### Phase 6 - Polish

Ziel: Feinschliff und Vollstaendigkeit.

- [ ] Globale Hotkeys (konfigurierbar, Windows-Taste)
- [ ] Import/Export (.txt, eigenes Format)
- [ ] Lokalisierung (Deutsch/Englisch/Benutzerdefiniert)
- [ ] Einstellungen-Dialog (alle Sektionen)
- [ ] Notizliste: Filter, alternating row colors, konfigurierbare Spalten
- [ ] DPI-Awareness
- [ ] Abgerundete Ecken (Win10/11)

---

## 6. Risiken und Mitigationen

| Risiko | Bewertung | Mitigation |
|--------|-----------|------------|
| Notiz-Fenster Owner-Draw Komplexitaet | Mittel | Solide Fensterklasse frueh als Fundament, iterativ erweitern |
| Verwaltung vieler gleichzeitiger Notiz-Fenster | Mittel | Sauberes Datenmodell (ID-basiert), Fenster-Map, klare Trennung Model/View |
| Alarm-Wiederholungslogik (Monate/Jahre) | Mittel | Kalender-Arithmetik sorgfaeltig implementieren, Edge-Cases testen |
| Netzwerk-Kompatibilitaet mit Original | Hoch | Eigenes Protokoll, keine Kompatibilitaet zum Original angestrebt (undokumentiert) |
| DPI-Scaling bei Owner-Draw | Mittel | Per-Monitor DPI Awareness V2 Manifest, DPI-skalierte Zeichenoperationen |
| Dateiformat des Originals (.dat/.atn) | Niedrig | Eigenes JSON-Format, optionaler .atn-Import als Bonus |
| Windows-Version Kompatibilitaet | Niedrig | Minimum Windows 10, moderne APIs nutzen wo moeglich |
| Transparenz + Klick-Durchreichung | Mittel | Sorgfaeltiges Testen, Fallback auf nicht-transparente Notizen |

---

## 7. Modernisierungs-Ideen (gegenueber Original)

Moegliche optische und funktionale Verbesserungen gegenueber dem 2005er Original:

- Abgerundete Ecken (Win11 native oder per Region)
- Smooth-Scrolling in Notizen
- Markdown-Unterstuetzung (fett, kursiv, Listen) - optional
- Mica/Acrylic-Effekte (Win11) statt einfacher Transparenz
- Dark Mode Unterstuetzung
- Multi-Monitor-Awareness
- High-DPI Unterstuetzung (Per-Monitor V2)
- JSON-basiertes Dateiformat (menschenlesbar)
- Bessere Font-Rendering (DirectWrite statt GDI)
- Animierte Uebergaenge (Fade-In/Out)

---

## 8. Fazit

**Die Machbarkeit ist hoch.** ATnotes ist ein Feature-reiches aber technisch gut beherrschbares Projekt. Saemtliche Features lassen sich mit der Win32-API und modernem C++ umsetzen, ohne externe Bibliotheken. Der groesste Aufwand liegt in den vielen Detail-Features (Alarm-Wiederholungslogik, Transparenz-Varianten, Netzwerk), nicht in grundsaetzlich schwierigen Problemen.

Das Projekt eignet sich hervorragend als phasenweiser Aufbau. Nach Phase 1 hat man bereits ein nutzbares Programm, und jede weitere Phase fuegt sinnvolle Funktionalitaet hinzu.

Die gesamte Anwendung kann als einzelne EXE ohne externe Abhaengigkeiten ausgeliefert werden - genau wie das Original.
