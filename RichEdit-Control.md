# RichEdit-Control Migration: Klickbare Links im Edit-Modus

## Ziel

Das Standard-EDIT-Control in `NoteWindow::EnterEditMode()` durch ein **RichEdit-Control** ersetzen, um URLs auch waehrend der Bearbeitung als klickbare Links darzustellen.

Das RichEdit-Control im Plain-Text-Modus (`TM_PLAINTEXT`) verhaelt sich nahezu identisch zum EDIT-Control, bietet aber eingebaute URL-Erkennung (`EM_AUTOURLDETECT`).

## Betroffene Dateien

| Datei | Aenderung |
|-------|-----------|
| `src/main.cpp` | `LoadLibraryW(L"Msftedit.dll")` hinzufuegen |
| `src/NoteWindow.cpp` | `EnterEditMode()`: EDIT -> RichEdit, URL-Detection aktivieren |
| `src/NoteWindow.cpp` | `EditSubclassProc`: `EN_LINK` Notification abfangen |
| `src/NoteWindow.cpp` | `ExitEditMode()`: Text-Auslese ggf. anpassen |

## Schritt-fuer-Schritt

### 1. Msftedit.dll laden (main.cpp)

Nach `OleInitialize(nullptr)` (Zeile 13) einfuegen:

```cpp
// Load RichEdit 4.1+ control (Msftedit.dll provides "RichEdit50W" class)
LoadLibraryW(L"Msftedit.dll");
```

Kein `FreeLibrary` noetig — DLL bleibt bis Prozessende geladen.

### 2. EnterEditMode() aendern (NoteWindow.cpp)

**Aktueller Code** (ca. Zeile 756):
```cpp
m_hEditCtrl = CreateWindowExW(
    0, L"EDIT", m_data->text.c_str(),
    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL,
    TEXT_PADDING, TEXT_PADDING,
    rc.right - 2 * TEXT_PADDING,
    rc.bottom - 2 * TEXT_PADDING - abHeight,
    m_hwnd, nullptr, m_hInst, nullptr
);
```

**Neuer Code:**
```cpp
#include <richedit.h>   // Fuer MSFTEDIT_CLASS, EM_AUTOURLDETECT, EN_LINK etc.

m_hEditCtrl = CreateWindowExW(
    0, MSFTEDIT_CLASS, nullptr,   // "RichEdit50W", Text wird separat gesetzt
    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL,
    TEXT_PADDING, TEXT_PADDING,
    rc.right - 2 * TEXT_PADDING,
    rc.bottom - 2 * TEXT_PADDING - abHeight,
    m_hwnd, nullptr, m_hInst, nullptr
);

// Plain-Text-Modus (kein RTF, keine Formatierung — wie normales EDIT)
SendMessageW(m_hEditCtrl, EM_SETTEXTMODE, TM_PLAINTEXT, 0);

// Text setzen (nach EM_SETTEXTMODE, da dieser den Inhalt loescht)
SetWindowTextW(m_hEditCtrl, m_data->text.c_str());

// URL-Erkennung aktivieren
SendMessageW(m_hEditCtrl, EM_AUTOURLDETECT, TRUE, 0);

// EN_LINK Notification anfordern
LRESULT mask = SendMessageW(m_hEditCtrl, EM_GETEVENTMASK, 0, 0);
SendMessageW(m_hEditCtrl, EM_SETEVENTMASK, 0, mask | ENM_LINK);
```

Die restlichen Messages (`WM_SETFONT`, `EM_SETMARGINS`, `EM_SETSEL`) funktionieren identisch.

### 3. EN_LINK Notification abfangen (NoteWindow.cpp)

Im `EditSubclassProc` oder im `HandleMessage` des Elternfensters den `WM_NOTIFY` mit `EN_LINK` verarbeiten:

```cpp
case WM_NOTIFY: {
    NMHDR* pnm = reinterpret_cast<NMHDR*>(lParam);
    if (pnm->code == EN_LINK) {
        ENLINK* link = reinterpret_cast<ENLINK*>(lParam);
        if (link->msg == WM_LBUTTONUP) {
            // URL-Text aus dem Control extrahieren
            TEXTRANGEW tr;
            tr.chrg = link->chrg;
            int len = link->chrg.cpMax - link->chrg.cpMin;
            std::wstring url(static_cast<size_t>(len) + 1, L'\0');
            tr.lpstrText = &url[0];
            SendMessageW(link->nmhdr.hwndFrom, EM_GETTEXTRANGE, 0,
                          reinterpret_cast<LPARAM>(&tr));
            url.resize(wcslen(url.c_str()));

            // Im Browser oeffnen
            ShellExecuteW(m_hwnd, L"open", url.c_str(),
                          nullptr, nullptr, SW_SHOWNORMAL);
            return 1;  // Handled
        }
    }
    break;
}
```

**Wichtig:** `EN_LINK` wird als `WM_NOTIFY` an das **Elternfenster** geschickt, nicht an das RichEdit selbst. Daher muss dieser Code in `NoteWindow::HandleMessage()` stehen (nicht im EditSubclassProc).

### 4. ExitEditMode() — Text auslesen

`GetWindowTextW` funktioniert auch mit RichEdit. Alternativ kann `EM_GETTEXTEX` verwendet werden, ist aber fuer Plain-Text nicht noetig:

```cpp
// Bestehender Code funktioniert unveraendert:
int len = GetWindowTextLengthW(m_hEditCtrl);
std::wstring text(static_cast<size_t>(len), L'\0');
if (len > 0)
    GetWindowTextW(m_hEditCtrl, &text[0], len + 1);
```

### 5. Hintergrundfarbe setzen

Das RichEdit-Control hat standardmaessig weissen Hintergrund. Fuer die Notiz-Hintergrundfarbe:

```cpp
SendMessageW(m_hEditCtrl, EM_SETBKGNDCOLOR, 0,
             static_cast<LPARAM>(m_data->layout.backgroundColor));
```

## Bekannte Unterschiede EDIT vs. RichEdit

| Aspekt | EDIT | RichEdit (Plain-Text) |
|--------|------|----------------------|
| Zeilenumbruch intern | `\r\n` | `\r` (nur CR!) |
| URL-Erkennung | Nein | Ja (EM_AUTOURLDETECT) |
| Hintergrundfarbe | WM_CTLCOLOREDIT | EM_SETBKGNDCOLOR |
| Max. Text | 32 KB default | Unbegrenzt |
| DLL | Keine | Msftedit.dll |

**Kritisch:** RichEdit speichert Zeilenumbrueche als `\r` statt `\r\n`. Beim Auslesen in `ExitEditMode()` muss `\r` zu `\r\n` konvertiert werden, damit die Persistenz (notes.json) und das Owner-Draw-Rendering konsistent bleiben:

```cpp
// Nach GetWindowTextW:
std::wstring normalized;
normalized.reserve(text.size() + text.size() / 10);
for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == L'\r' && (i + 1 >= text.size() || text[i + 1] != L'\n'))
        normalized += L"\r\n";
    else
        normalized += text[i];
}
m_data->text = std::move(normalized);
```

## Aufwand

Geschaetzt 30-50 Zeilen Codeaenderung. Kein neuer Header noetig ausser `<richedit.h>`. Keine externen Abhaengigkeiten (Msftedit.dll ist in jeder Windows-Version seit Vista enthalten).
