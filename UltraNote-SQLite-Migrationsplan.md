# UltraNote — Migrationsplan: JSON → SQLite

## Übersicht

Dieses Dokument beschreibt den vollständigen Plan zur Migration des UltraNote-Speicherbackends von JSON-Dateien (`notes.json`, `settings.json`) auf eine SQLite-Datenbank. Die bestehende `Storage`-Klasse wird dabei als einzige Komponente ersetzt — das restliche Projekt bleibt nahezu unverändert.

**Projekt-Version bei Erstellung:** v0.6  
**Geschätzter Aufwand:** 1–2 konzentrierte Abende  
**Risiko:** Niedrig (saubere Isolation in `Storage`-Klasse)

---

## 1. Ist-Zustand

### Architektur

- **`Storage.h` / `Storage.cpp`** (~630 Zeilen) kapselt die gesamte Persistenz
- Zwei JSON-Dateien: `notes.json` (Notizen + Ordner + nextId) und `settings.json` (Key-Value)
- Speichermodell: Alles laden → im RAM halten → komplett rausschreiben (`SaveAll`)
- Atomic Write über `.tmp`-Datei + `MoveFileExW`-Rename
- Handgeschriebener JSON-Parser (Recursive Descent) und Serializer

### Aufrufstellen (nur diese sind betroffen)

| Datei | Methode | Zweck |
|---|---|---|
| `Application.cpp:49` | `Storage::LoadNotes()` | Einmaliges Laden beim Start |
| `Application.cpp:644` | `Storage::SaveNotes()` | Speichern bei Autosave / Exit |
| `SettingsDialog.cpp` | `Storage::LoadSettings()`, `LoadSettingsStr()`, `SaveAllSettings()` | Settings lesen/schreiben |
| `NoteListWindow.cpp` | `Storage::LoadSettings()`, `SaveSettings()` | Fenster-Einstellungen |

### Datenmodell

```cpp
struct NoteLayout {
    COLORREF backgroundColor;   // z.B. RGB(255, 255, 153)
    COLORREF textColor;
    COLORREF borderColor;
    std::wstring fontFace;
    int      fontSizePts;
    bool     fontBold;
    bool     fontItalic;
    bool     alwaysOnTop;
};

struct NoteData {
    uint64_t     id;
    std::wstring text;
    std::wstring title;
    std::wstring folder;
    int          x, y, width, height;
    bool         isMinimized;
    bool         isHidden;
    NoteLayout   layout;       // Flach — keine Verschachtelung nötig
    int64_t      createdAt;
    int64_t      modifiedAt;
};
```

---

## 2. SQLite-Datenbankschema

Dateiname: `ultranote.db` (im EXE-Verzeichnis, ersetzt `notes.json` + `settings.json`)

### Tabelle `meta`

```sql
CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- Initiale Werte
INSERT INTO meta (key, value) VALUES ('version', '1');
INSERT INTO meta (key, value) VALUES ('next_id', '1');
```

### Tabelle `folders`

```sql
CREATE TABLE IF NOT EXISTS folders (
    name TEXT PRIMARY KEY
);
```

> **Alternative:** Ordner nicht separat speichern, sondern per  
> `SELECT DISTINCT folder FROM notes WHERE folder != ''` ableiten.  
> Eigene Tabelle ist besser, da Ordner auch ohne zugehörige Notizen existieren können.

### Tabelle `notes`

```sql
CREATE TABLE IF NOT EXISTS notes (
    id              INTEGER PRIMARY KEY,
    text            TEXT    NOT NULL DEFAULT '',
    title           TEXT    NOT NULL DEFAULT '',
    folder          TEXT    NOT NULL DEFAULT '',
    x               INTEGER NOT NULL DEFAULT 100,
    y               INTEGER NOT NULL DEFAULT 100,
    width           INTEGER NOT NULL DEFAULT 200,
    height          INTEGER NOT NULL DEFAULT 150,
    is_minimized    INTEGER NOT NULL DEFAULT 0,
    is_hidden       INTEGER NOT NULL DEFAULT 0,
    bg_color        TEXT    NOT NULL DEFAULT '#FFFF99',
    text_color      TEXT    NOT NULL DEFAULT '#000000',
    border_color    TEXT    NOT NULL DEFAULT '#C8C850',
    font_face       TEXT    NOT NULL DEFAULT 'Arial',
    font_size_pts   INTEGER NOT NULL DEFAULT 10,
    font_bold       INTEGER NOT NULL DEFAULT 0,
    font_italic     INTEGER NOT NULL DEFAULT 0,
    always_on_top   INTEGER NOT NULL DEFAULT 0,
    created_at      INTEGER NOT NULL DEFAULT 0,
    modified_at     INTEGER NOT NULL DEFAULT 0
);
```

> **Hinweis:** Layout-Felder sind bewusst flach in `notes` aufgelöst statt in einer separaten Tabelle. Das NoteLayout-Struct hat eine 1:1-Beziehung zur Notiz — Normalisierung bringt hier keinen Vorteil.

### Tabelle `settings`

```sql
CREATE TABLE IF NOT EXISTS settings (
    key        TEXT PRIMARY KEY,
    value_type TEXT NOT NULL CHECK (value_type IN ('int', 'string')),
    int_value  INTEGER,
    str_value  TEXT
);
```

### Optionaler Index für Volltextsuche (späteres Feature)

```sql
-- Erst aktivieren wenn Volltextsuche implementiert wird
CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5(
    title, text, content='notes', content_rowid='id'
);
```

---

## 3. Neue Storage-Klasse

### Storage.h (neues Interface)

```cpp
#pragma once

#include "Note.h"
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <cstdint>

// Forward-Deklaration — kein sqlite3.h im Header nötig
struct sqlite3;

class Storage {
public:
    // Datenbank öffnen/erstellen (im EXE-Verzeichnis)
    static bool Initialize();
    static void Shutdown();

    // Notes — API bleibt identisch zum Aufrufer
    static std::vector<std::unique_ptr<NoteData>> LoadNotes(
        uint64_t& outNextId,
        std::vector<std::wstring>& outFolders);

    static bool SaveNotes(
        const std::vector<std::unique_ptr<NoteData>>& notes,
        uint64_t nextId,
        const std::vector<std::wstring>& folders);

    // Settings — API bleibt identisch
    static std::map<std::wstring, int> LoadSettings();
    static std::map<std::wstring, std::wstring> LoadSettingsStr();
    static bool SaveSettings(const std::map<std::wstring, int>& settings);
    static bool SaveAllSettings(
        const std::map<std::wstring, int>& intSettings,
        const std::map<std::wstring, std::wstring>& strSettings);

    // Hilfsfunktion: Pfad zur Datenbank
    static std::wstring GetDatabasePath();

    // Migration: JSON → SQLite (einmalig beim ersten Start)
    static bool MigrateFromJson();

private:
    static sqlite3* s_db;

    static bool CreateTables();
    static std::string WideToUtf8(const std::wstring& wide);
    static std::wstring Utf8ToWide(const std::string& utf8);
    static std::string ColorToHex(COLORREF c);
    static COLORREF HexToColor(const std::string& hex);
};
```

> **Wichtig:** Das öffentliche Interface (`LoadNotes`, `SaveNotes`, `LoadSettings`, etc.) bleibt identisch. Application.cpp und die anderen Aufrufer müssen nur minimal angepasst werden (Initialize/Shutdown hinzufügen).

### Storage.cpp — Implementierungs-Leitfaden

#### Initialisierung

```cpp
sqlite3* Storage::s_db = nullptr;

bool Storage::Initialize() {
    std::string dbPath = WideToUtf8(GetDatabasePath());
    int rc = sqlite3_open(dbPath.c_str(), &s_db);
    if (rc != SQLITE_OK) return false;

    // WAL-Modus für bessere Performance und Crash-Sicherheit
    sqlite3_exec(s_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(s_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    if (!CreateTables()) return false;

    // Prüfen ob Migration nötig ist
    // (notes.json existiert UND Datenbank ist leer)
    MigrateFromJson();

    return true;
}

void Storage::Shutdown() {
    if (s_db) {
        sqlite3_close(s_db);
        s_db = nullptr;
    }
}
```

#### SaveNotes — Komplettes Speichern (kompatibel zum bisherigen Modell)

```cpp
bool Storage::SaveNotes(const std::vector<std::unique_ptr<NoteData>>& notes,
                         uint64_t nextId,
                         const std::vector<std::wstring>& folders) {
    if (!s_db) return false;

    // Alles in einer Transaktion für Atomarität
    sqlite3_exec(s_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // 1. Alle bestehenden Notizen löschen und neu einfügen
    //    (einfachster Ansatz, kompatibel zum bisherigen "alles rausschreiben")
    sqlite3_exec(s_db, "DELETE FROM notes;", nullptr, nullptr, nullptr);

    const char* insertSql =
        "INSERT INTO notes (id, text, title, folder, x, y, width, height, "
        "is_minimized, is_hidden, bg_color, text_color, border_color, "
        "font_face, font_size_pts, font_bold, font_italic, always_on_top, "
        "created_at, modified_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(s_db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(s_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& note : notes) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(note->id));
        // ... alle 20 Felder binden ...
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    // 2. Ordner aktualisieren
    sqlite3_exec(s_db, "DELETE FROM folders;", nullptr, nullptr, nullptr);
    // ... folders einfügen ...

    // 3. nextId in meta aktualisieren
    // UPDATE meta SET value = ? WHERE key = 'next_id';

    sqlite3_exec(s_db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}
```

#### LoadNotes — Alle Notizen laden

```cpp
std::vector<std::unique_ptr<NoteData>> Storage::LoadNotes(
    uint64_t& outNextId,
    std::vector<std::wstring>& outFolders) {

    std::vector<std::unique_ptr<NoteData>> notes;
    outNextId = 1;

    if (!s_db) return notes;

    // nextId aus meta lesen
    // SELECT value FROM meta WHERE key = 'next_id';

    // Alle Notizen laden
    const char* sql = "SELECT * FROM notes ORDER BY id;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(s_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto note = std::make_unique<NoteData>();
            note->id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
            // ... alle Felder auslesen und in NoteData mappen ...
            notes.push_back(std::move(note));
        }
        sqlite3_finalize(stmt);
    }

    // Ordner laden
    // SELECT name FROM folders ORDER BY name;

    return notes;
}
```

---

## 4. Änderungen an bestehenden Dateien

### Application.cpp

Nur zwei Zeilen hinzufügen:

```cpp
// In Application::Init() — VOR dem LoadNotes-Aufruf:
Storage::Initialize();

// In Application::Shutdown() / Destruktor — NACH dem letzten SaveAll:
Storage::Shutdown();
```

Alle anderen Aufrufe (`LoadNotes`, `SaveNotes`, `SaveAll`, `MarkDirty`) bleiben unverändert.

### SettingsDialog.cpp, NoteListWindow.cpp

Keine Änderungen nötig — die Settings-API bleibt identisch.

### CMakeLists.txt

SQLite als Dependency einbinden. Empfehlung: **sqlite3 Amalgamation** direkt ins Projekt (eine `.c` + eine `.h` Datei):

```cmake
# SQLite Amalgamation (sqlite3.c + sqlite3.h ins src/ oder lib/ Verzeichnis)
add_library(sqlite3 STATIC lib/sqlite3.c)
target_include_directories(sqlite3 PUBLIC lib/)

# UltraNote gegen SQLite linken
target_link_libraries(UltraNote PRIVATE sqlite3)
```

> Die Amalgamation ist eine einzelne C-Datei (~250 KB). Download:  
> https://www.sqlite.org/download.html → "sqlite-amalgamation"  
> Kein externer Paketmanager nötig, kein vcpkg, kein Build-System-Ärger.

---

## 5. Migration bestehender Daten

### Strategie

Beim ersten Start nach dem Update:

1. `Storage::Initialize()` erstellt die leere Datenbank
2. `MigrateFromJson()` prüft ob `notes.json` existiert
3. Falls ja: JSON einlesen (mit dem alten Parser), in SQLite schreiben
4. `notes.json` → `notes.json.bak` umbenennen (nicht löschen!)
5. Dasselbe für `settings.json`

### Implementierung

```cpp
bool Storage::MigrateFromJson() {
    std::wstring notesJsonPath = GetExeDirectory() + L"\\notes.json";

    // Prüfen ob JSON existiert
    if (GetFileAttributesW(notesJsonPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        return true;  // Nichts zu migrieren

    // Prüfen ob DB bereits Daten hat
    // SELECT COUNT(*) FROM notes;
    // Wenn > 0: Migration bereits erfolgt, abbrechen

    // --- Alter JSON-Parser (kann als statische Hilfsfunktion bleiben) ---
    // Den bestehenden LoadNotes-JSON-Code in eine private
    // LoadNotesFromJson()-Methode auslagern

    uint64_t nextId = 1;
    std::vector<std::wstring> folders;
    auto notes = LoadNotesFromJson(nextId, folders);

    // In SQLite schreiben
    SaveNotes(notes, nextId, folders);

    // Settings migrieren
    // ... analog für settings.json ...

    // JSON-Dateien als Backup umbenennen
    std::wstring backupPath = notesJsonPath + L".bak";
    MoveFileW(notesJsonPath.c_str(), backupPath.c_str());

    return true;
}
```

> **Wichtig:** Den alten JSON-Parser für die Migration aufbewahren!  
> Am besten als private `LoadNotesFromJson()` in Storage.cpp belassen  
> oder in eine separate Datei `JsonMigration.cpp` auslagern.  
> Nach ein paar Versionen kann der Migrations-Code entfernt werden.

---

## 6. Optionaler Phase-2-Ausbau

Diese Verbesserungen sind nicht Teil der initialen Migration, werden aber durch SQLite erst möglich:

### Inkrementelles Speichern

Statt bei jedem Autosave alle Notizen komplett zu schreiben, nur die geänderte Notiz updaten:

```cpp
// Neue Methode (optional, Phase 2)
static bool SaveSingleNote(const NoteData& note);

// In Application.cpp dann:
// Statt SaveAll() → nur SaveSingleNote(*changedNote)
```

Dies erfordert, dass `MarkDirty()` die geänderte Notiz-ID trackt — z.B. über ein `std::set<uint64_t> m_dirtyNoteIds`.

### Volltextsuche mit FTS5

```sql
-- FTS-Tabelle anlegen
CREATE VIRTUAL TABLE notes_fts USING fts5(title, text, content='notes', content_rowid='id');

-- Trigger für automatische Synchronisation
CREATE TRIGGER notes_ai AFTER INSERT ON notes BEGIN
    INSERT INTO notes_fts(rowid, title, text) VALUES (new.id, new.title, new.text);
END;
CREATE TRIGGER notes_ad AFTER DELETE ON notes BEGIN
    INSERT INTO notes_fts(notes_fts, rowid, title, text)
        VALUES('delete', old.id, old.title, old.text);
END;
CREATE TRIGGER notes_au AFTER UPDATE ON notes BEGIN
    INSERT INTO notes_fts(notes_fts, rowid, title, text)
        VALUES('delete', old.id, old.title, old.text);
    INSERT INTO notes_fts(rowid, title, text) VALUES (new.id, new.title, new.text);
END;

-- Suche
SELECT n.* FROM notes n
JOIN notes_fts f ON n.id = f.rowid
WHERE notes_fts MATCH 'suchbegriff'
ORDER BY rank;
```

### Schema-Migration für zukünftige Versionen

```cpp
// In CreateTables() nach dem Erstellen:
int currentVersion = GetMetaInt("version");
if (currentVersion < 2) {
    // ALTER TABLE notes ADD COLUMN pinned INTEGER NOT NULL DEFAULT 0;
    SetMetaInt("version", 2);
}
if (currentVersion < 3) {
    // Nächste Schema-Änderung...
    SetMetaInt("version", 3);
}
```

---

## 7. Checkliste

- [ ] SQLite Amalgamation herunterladen und in `lib/` ablegen
- [ ] `CMakeLists.txt` anpassen
- [ ] `Storage.h` umschreiben (neues Interface)
- [ ] `Storage.cpp` neu implementieren (SQLite statt JSON)
- [ ] `MigrateFromJson()` implementieren (alter Parser bleibt temporär)
- [ ] `Application.cpp`: `Initialize()` / `Shutdown()` einbauen
- [ ] Testen: Frische Installation (leere DB)
- [ ] Testen: Migration von bestehender `notes.json`
- [ ] Testen: Autosave-Zyklus funktioniert
- [ ] Testen: Settings werden korrekt gelesen/geschrieben
- [ ] Optional: `notes.json`-Parser nach erfolgreicher Migration entfernen
