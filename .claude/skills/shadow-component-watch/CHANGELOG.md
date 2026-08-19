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

**Korrektur (siehe Eintrag unten, selber Tag):** Der oben gesetzte "Zuletzt geprüft"-Marker `de066e6b9` für `CurrentAccountHeaderButton.qml` war zu optimistisch — es wurden bei der Erstaufbau nur die Top-5-Commits plus der eine für `anySyncFolders` relevante Commit inhaltlich geprüft, nicht die komplette Spanne bis dorthin. Der erste reguläre Lauf des Skills (unten) hat das nachgeholt.

## 2026-08-17 — Erster regulärer Lauf: Nachhol-Review der Lücke

Referenz-Branch: `origin/stable-33.0` (`git fetch` durchgeführt).

**Paar: `TrayWindowHeader.qml` ↔ `SesTrayHeader.qml`**
`git log 568cbe171..origin/stable-33.0 -- src/gui/tray/TrayWindowHeader.qml` → keine neuen Commits. **Kein Handlungsbedarf**, Stand unverändert.

**Paar: `CurrentAccountHeaderButton.qml` ↔ `TrayWindowAccountMenu.qml`/`UserLine.qml`**
`git log de066e6b9..origin/stable-33.0 -- ...` ergab überraschend 24 bisher ungeprüfte Commits (2025-03-19 bis 2026-02-11) — die Erstaufbau-Prüfung hatte diese Lücke übersehen (s. Korrektur oben). Alle 24 durchgesehen:
- Dynamische Text-/Icon-Farben über `palette`/`Style.currentUserHeaderTextColor` (`2e321d238`, `896cd1196`, `0e346e8d3`), Status-Icon-Hintergrund/-Position (`0f84c803f`, `cf326c1c7`, `5248b59a1`, `bcb421cab`, `adaaf3634`), `parentBackgroundColor` fürs Status-Icon (`7f9d6a869`, `60e0cb196`) — **bewusst nicht portiert**: hängt alles am Online-Status-Indikator, der im Fork komplett deaktiviert ist (`SES-50`). `parentBackgroundColor` existiert als Property in `UserLine.qml`, wird aber nirgends gesetzt/genutzt — totes Überbleibsel derselben Abschaltung, keine Aktion nötig.
- SPDX-Lizenzkopf-Migration (`00994aa9e`) — rein Lizenzmetadaten, N/A.
- Label-Alignment/Spacing-Feintuning (`db23bd6f4`) — bezieht sich auf `CurrentAccountHeaderButton`s eigene Column-Struktur (separates Server-Label unterhalb des Namens); der Fork hat diese Zeile anders aufgebaut (Server erscheint nur als Fallback in der Status-Message-Zeile) — nicht 1:1 portierbar, geringer visueller Wert, **kein Portierungs-Kandidat**.
- `onObjectAdded`/`onObjectRemoved`-Deprecation-Fix (`6a40c0e48`, explizite Funktionsparameter statt injizierter Signal-Parameter) — **bereits portiert**, wenn auch mit anderer Syntax: `TrayWindowAccountMenu.qml` nutzt bereits `(index, object) => ...`-Arrow-Functions, was dieselbe Qt-Deprecation-Warnung vermeidet.
- Menübreite-Auto-Sizing-Weiterentwicklung (`8145c1ef7`, `b26c20e64`, Breiten-Teil von `7ce4f78c4`) sowie `addAccountButton`-Icon-Sizing-Verfeinerungen (`3adeb70af`, `c69d8edc9`, `53a165f56`, `adaa54cce`) — **bewusstes Design**: Fork nutzt feste `Style.sesAccountMenuWidth` und eine generische, geteilte `AccountMenuItem`-Komponente statt pro-Button-Maßen: kein Nachzug nötig, bereits als bekannte Abweichung in der Registry vermerkt.
- Windows-Ausnahme für Highlight-Textfarbe (`aa7ee7478`, `((...) && Qt.platform.os !== "windows")`) sowie Kontrastfarben-Logik (`be3a001ed`) — **bereits portiert**: exakt dieses Muster steckt in `UserLine.qml`s `statusItemColor`.
- Elide-Ergänzung für Menüeinträge (`d354b329f`) — abgedeckt durch `AccountMenuItem.qml`s eigenes Text-Handling, **kein Handlungsbedarf** erkennbar.
- **`1e7f20641`** ("fix: restore user status message type import") führt `onShowUserStatusMessageSelector` samt `userStatusDrawer.openUserStatusMessageDrawer(model.index)` wieder ein. Nachrecherchiert: in `origin/stable-33.0` hat jede Account-Zeile im "More actions"-Menü zwei getrennte, immer sichtbare Einträge — "Set status" (→ `showUserStatusSelector`, öffnet den Presence-Picker) und "Status message" (→ `showUserStatusMessageSelector`, Shortcut direkt zum Message-Editor; beides dieselbe `UserStatusSelectorPage.qml`, nur unterschiedlicher Einstiegs-`mode`). Im Fork sind **beide** Signale tot — `git grep` nach tatsächlichen Aufrufen (nicht nur Deklarationen) findet in ganz `src/gui/tray` keinen einzigen; `UserLine.qml`s "More actions"-Menü enthält nur noch `logInOutButton`/`removeAccountButton`, "Set status" und "Status message" wurden komplett entfernt (Commit `bb543a166`/`c89b7d636`, "SES-457 - Squashed commit").
  - **Verdict: bewusst nicht portiert.** Von Boris bestätigt (2026-08-17): das Produkt hat keine Collaboration-Features, User-Status ergibt daher grundsätzlich keinen Sinn — gilt für den kompletten Feature-Komplex (Indikator **und** beide Menüeinträge), auch wenn nur der Indikator einen expliziten `SES-50`-Kommentar im Code trägt. `registry.md` entsprechend zusammengefasst, damit künftige Läufe das als eine Abweichung behandeln statt es erneut als offenen Punkt zu melden.

**Ergebnis:** keine offenen Punkte mehr — alles bereits portiert oder nachvollziehbar bewusst nicht portiert (Status-Feature komplett, Produktentscheidung "keine Collaboration-Features").
