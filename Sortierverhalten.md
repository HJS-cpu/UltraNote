# Sortierverhalten der Notizliste

## Grundprinzip

Ein Klick auf einen Spaltenheader sortiert die Liste **aufsteigend** (▲).
Ein weiterer Klick auf die gleiche Spalte wechselt auf **absteigend** (▼).
Ein Klick auf eine andere Spalte sortiert diese **aufsteigend** (▲).

Der aktive Sortier-Pfeil (▲/▼) im Header zeigt immer die aktuelle Sortierung an.

## "Erster Klick scheint nicht zu funktionieren"

Beim Start der App sind die Notizen in **Einfuegereihenfolge** angeordnet — das entspricht gleichzeitig der **chronologischen Reihenfolge** (aelteste Notiz zuerst).

Klickt man nun auf "Erstellt", wird **aufsteigend nach Erstelldatum** sortiert. Da die Liste bereits in genau dieser Reihenfolge ist, aendert sich die Anzeige nicht. Der Pfeil ▲ erscheint und bestaetigt, dass die Sortierung aktiv ist — es gibt nur nichts umzusortieren.

Erst der **zweite Klick** (▼ absteigend) kehrt die Reihenfolge um und erzeugt eine sichtbare Aenderung.

Dieses Verhalten tritt bei jeder Spalte auf, deren natuerliche Reihenfolge bereits der aufsteigenden Sortierung entspricht. Es ist **Standard-Verhalten** (Windows Explorer, Outlook etc. verhalten sich identisch).

## Spalten und ihr Standardverhalten

| Spalte     | Erster Klick (▲)                     | Sichtbare Aenderung beim ersten Klick?         |
|------------|---------------------------------------|-------------------------------------------------|
| Titel      | Alphabetisch A→Z                     | **Ja** (Einfuegereihenfolge ≠ alphabetisch)     |
| Text       | Alphabetisch A→Z                     | **Ja** (Einfuegereihenfolge ≠ alphabetisch)     |
| Ordner     | Alphabetisch A→Z                     | Abhaengig von den Ordnernamen                   |
| Versteckt  | Sichtbare zuerst (☐ vor ☑)          | Nur wenn gemischte Werte vorhanden               |
| Im Vorderg.| Normale zuerst (☐ vor ☑)            | Nur wenn gemischte Werte vorhanden               |
| Erstellt   | Aelteste zuerst                       | **Nein** (entspricht Einfuegereihenfolge)        |
| Anhaenge   | Kein Vergleich implementiert          | Nein                                             |

## Technische Umsetzung

- Sortierung wird via `HeaderSubclassProc` ausgeloest (faengt `WM_LBUTTONUP` am Header-Control ab)
- `HDM_HITTEST` ermittelt die angeklickte Spalte (logischer Index)
- `ListView_SortItems` mit `CompareFunc` fuehrt die Sortierung durch
- `HDF_SORTUP` / `HDF_SORTDOWN` Flags zeigen den Sortier-Pfeil im Header
- Die Standard-Notification-Chain (HDN_ITEMCLICK → LVN_COLUMNCLICK) wird nicht verwendet, da `SetWindowTheme` auf dem Header diese unzuverlaessig macht
