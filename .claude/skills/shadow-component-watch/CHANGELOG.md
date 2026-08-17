# Changelog: Shadow-Component-Watch

Chronologisches Protokoll jeder Prüfung. Referenz-Branch beim jeweiligen Lauf siehe `../stable-merge-check/reference-branch.txt` (Stand zum Prüfzeitpunkt: `stable-33.0`).

## 2026-08-17 — Erstaufbau der Registry

**Paar: `TrayWindowHeader.qml` ↔ `SesTrayHeader.qml`**
Geprüfte Commits (komplette bisherige Historie, da Erstaufbau): `01e099546`, `6475c51ab`, `1ae041da0`, `19d70cdaa`, `568cbe171`.
- Featured-App-Signal (`featuredAppButtonClicked`), Property-Umbenennungen (`isFeaturedAppEnabled` → `isNcAssistantEnabled` → `isAssistantEnabled`), i18n-String "team folder" — **bereits portiert**, exakt übereinstimmend im aktuellen `SesTrayHeader.qml`.
- "More apps"-Menü inkl. Icon-Ladepfad-Fix (`19d70cdaa`) — **bewusst nicht portiert**: das ganze Multi-App-Menü existiert im Fork nicht (Ticket `SES-4`, einzelner "Website"-Button stattdessen), Fix daher gegenstandslos.

**Paar: `CurrentAccountHeaderButton.qml` ↔ `TrayWindowAccountMenu.qml`**
Geprüfte Commits (komplette bisherige Historie, da Erstaufbau): u. a. `60e0cb196` (color fix), `7f9d6a869` (`parentBackgroundColor` an `UserLine`), `bcb421cab`/`adaaf3634` (Status-Hintergrund), `de066e6b9` (anySyncFolders-Guard), `7ce4f78c4` (Sync-Error-Indikator in Account-Zeile).
- **`de066e6b9`** (Pause/Resume-Sync-Menüeintrag nur sichtbar wenn `Systray.anySyncFolders`) — fehlte in `TrayWindowAccountMenu.qml`. **Verdict: nachgezogen** — `enabled`/`visible: Systray.anySyncFolders` an `syncPauseButton` ergänzt (zugehörige C++-Property existierte bereits in `systray.h`/`.cpp`, keine neue Logik nötig).
- Online-Status-Indikator (farbiger Punkt) — im Fork explizit mit `visible: false // SES-50 Remove Inidcator` deaktiviert. **Verdict: bewusst nicht portiert** (Ticket `SES-50`).
- Dynamische Menübreite nach breitestem Eintrag vs. feste `Style.sesAccountMenuWidth` — **Verdict: bewusstes Design** (passt zum restlichen statischen Ses-Farbsystem), kein Ticket-Bezug bekannt, aber nicht überraschend genug für einen Portierungs-Kandidaten.
- `parentBackgroundColor`-Weiterreichung für kontrastabhängige Hintergründe — **Verdict: bewusstes Design**, entfällt weil der Fork statische `Style.ses*`-Farben statt dynamischer QPalette nutzt.
- Sync-Error-Indikator in der Account-Zeile (`7ce4f78c4`) — bereits vorhanden in `UserLine.qml` (`syncStatusColumn`/`syncStatusIndicator`). **Verdict: bereits portiert.**

**Sonstiges:** `TrayWindowHeaderBar.qml` (dritte "Karteileiche" im selben Ordner) wurde im Rahmen dieser Analyse gelöscht — kein Eintrag in der Registry, da reines Fork-Artefakt ohne jemaliges Upstream-Pendant (nie in `resources.qrc`, nie aus `MainWindow.qml`/`TrayWindowHeader.qml`/`SesTrayHeader.qml` referenziert).
