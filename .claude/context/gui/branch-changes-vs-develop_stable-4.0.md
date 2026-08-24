# Component-Context: alle vom Feature-Branch geänderten GUI-Komponenten (ggü. Basis `develop_stable-4.0`)

Erzeugt am 2026-08-24 mit dem [component-context](../../skills/component-context/SKILL.md)-Skill, angewendet auf jede echte UI-Komponente (Dialog/Widget/Delegate/QML-View), die im Diff `HEAD` vs. `origin/develop_stable-4.0` unter `src/gui/` auftaucht.

**Update 2026-08-24 (nach dem Merge):** `feature/SES-578_DarkMode_Tray` wurde per `e7a6ff00a` in `develop_stable-33.0` gemerged (zusammen mit `bugs/SES-596_LINUX_Vereinheitlichung_der_Schriftart`). `HEAD` bezieht sich seither auf `develop_stable-33.0`. Die Analyse wurde wiederholt und gegen den vorherigen Stand (Basis: `feature/SES-578_DarkMode_Tray`-Tip `cba5a70e8`) abgeglichen — Ergebnis siehe **Merge-Konsistenz-Check** unten. Ursprünglich 138 Diff-Dateien → 45 Komponenten; nach dem Merge 145 Diff-Dateien → 51 Komponenten (6 neu, ausschließlich durch SES-596 bedingt, keine Verluste).

**Wichtig zur Richtung:** `develop_stable-4.0` ist **nicht** ein künftiges Merge-Ziel, sondern die **Basis, von der dieser Feature-Branch abgezweigt wurde** (letzter Entwicklungsstand vor SES-578). Der Diff zeigt also **alles, was dieser Branch seit dem Abzweigen selbst verändert hat** — nützlich als Übersicht des eigenen Änderungsumfangs (z. B. für Review/PR-Beschreibung), nicht als Merge-Risiko-Einschätzung gegen ein Ziel. Reine Backend-/Model-/Platform-Dateien ohne eigene UI (`accountmanager.cpp`, `folderman.cpp`, `socketapi.cpp`, macOS-FileProvider-Backend-`.mm`, `main.cpp` etc.) wurden bewusst ausgeschlossen.

**Methodik-Hinweis:** Der Klickpfad/die Einordnung wurde, wo möglich, aus der bestehenden `.claude/context/gui/`-Doku übernommen (dort mit "laut Doku" markiert) statt für jede Komponente einzeln neu aus dem Code hergeleitet — das wäre bei dieser Menge nicht mehr leistbar gewesen. Wo keine bestehende Doku existierte, wurde gezielt nachgeschaut (vermerkt). **Dieses Dokument ist ein einmaliges Sammel-Ergebnis, keine von einem Skill gepflegte Datei** (der `component-context`-Skill selbst schreibt/pflegt bewusst kein eigenes Registry-File — siehe dessen SKILL.md) — es veraltet wie jede Momentaufnahme und wird nicht automatisch nachgezogen.

## Merge-Konsistenz-Check (Consistency-Methode)

Um sicherzustellen, dass der Merge `feature/SES-578_DarkMode_Tray` → `develop_stable-33.0` keine SES-578-Arbeit stillschweigend verändert/verloren hat, wurde die Analyse unabhängig wiederholt und systematisch mit dem vorherigen Ergebnis verglichen, statt dem neuen Diff blind zu vertrauen:

1. **Datei-Diff neu berechnet:** `git diff --name-only HEAD origin/develop_stable-4.0 -- src/gui` vorher (138 Dateien, Basis `cba5a70e8`) gegen nachher (145 Dateien, Basis `e7a6ff00a`) verglichen. Ergebnis: **7 Dateien neu, keine entfernt.**
2. **Jede neu in den Diff gerutschte Datei erklärt** (Klarstellung: keine dieser 7 Dateien wurde neu erstellt — alle existierten schon vorher, teils seit Jahren, z. B. `main.cpp` seit 2011; sie waren nur bis dahin byte-identisch mit `develop_stable-4.0` und sind erst jetzt zum ersten Mal davon abgewichen): `git diff cba5a70e8 origin/develop_stable-4.0 -- <datei>` ergab für alle 7 (`FileDetailsPage.qml`, `main.cpp`, `ActivityItem.qml`, `IconButton.qml`, `SecondaryPillButton.qml`, `TrayFolderListItem.qml`, `TrayFoldersMenuButton.qml`) **0 Diff-Zeilen unmittelbar vor** `70a6a94dd` ("SES-596 fix undefined font fallback on Linux/macOS") — und dieser eine Commit ist laut `git show --stat 70a6a94dd` auch tatsächlich der einzige, der alle 7 anfasst. Kein Zusammenhang mit einer Merge-Konfliktauflösung.
3. **Überlapp mit dem alten 138-Datei-Set geprüft:** `git diff --name-only cba5a70e8 HEAD -- src/gui` ergab 11 seither veränderte Dateien, davon 4 aus dem ursprünglichen Set (`basetheme.h`, `ActivityItemContent.qml`, `CurrentAccountHeaderButton.qml`, `UnifiedSearchResultListItem.qml`). Jede dieser 4 Diffs einzeln gelesen: durchweg additive Font-Fallback-Änderungen (`Utility::isWindows() ? "Segoe UI" : "Open Sans"`, `font.family: Style.sesOpenSansRegular`, `font: parent.font`) — **keine SES-578-Dark-Mode-Logik berührt, keine Farbwerte verändert.**
4. **Fazit:** Merge ist **konsistent und unauffällig** — die einzige Veränderung ggü. dem vorherigen Analysestand ist die vollständig erklärbare, additive SES-596-Font-Fallback-Arbeit. Keine der bestehenden 45 Komponenten-Einträge unten musste inhaltlich korrigiert werden (nur "Letzte Änderungen" ergänzt, wo betroffen). Auffällig, aber unkritisch: `CurrentAccountHeaderButton.qml` — als "funktional inaktiv" dokumentiert, wurde aber trotzdem vom globalen Font-Fallback-Fix mitgezogen (spricht für einen breiten, dateibasierten statt render-baum-basierten Fix-Ansatz in SES-596, keine Reaktivierung der Datei).

## Legende

- **Status**: aktiv genutzt | ungenutzt/legacy (aber auf stable geführt) | gelöscht (nur zur Vollständigkeit erwähnt)
- **Diff-Grund**: warum sich die Datei gegenüber der Basis `develop_stable-4.0` geändert hat (SES-578-Dark-Mode-Fix, ältere fork-eigene Abweichung, oder schlicht ein seit dem Abzweigen nachgezogener Versionsstand)

---

## App & Connection Settings (Root-Cluster)

### AccountSettings (`accountsettings.cpp/.h/.ui`)
**Navigation:** Tray → Account-Menü → Zahnrad/Einstellungen → Settings-Dialog-Toolbar → Account-Tab (pro Account einer)
**Bisherige Entscheidungen:** 2026-08-21 Card-Panel-Hintergründe wieder entfernt (SES-578); `connectionSettingsPanel`-Sichtbarkeitsentscheidung (siehe NetworkSettings unten) wirkt hier rein.
**Doku:** COMPONENTS.md Zeile 14 — "Fork-specific: `whitelabeltheme.h`/`WLTheme`, `#ifndef IONOS_BUILD`-Guard, `STRATO_WL_BUILD`-ExpandMemory-Link".
**Letzte Änderungen:** `cba5a70e8`, `ffeb8e7e4`, `dd2b5fcc4`, `b9e530504` — alle SES-578 Dark-Mode-Feinschliff.
**Diff-Grund:** SES-578-Änderungen noch nicht in `develop_stable-4.0`.

### GeneralSettings (`generalsettings.cpp/.ui`)
**Navigation:** Settings-Dialog-Toolbar → "Allgemein"-Tab
**Bisherige Entscheidungen:** 2026-08-12 zweimal (SES-576: erst als bewusster Redesign-Stand bestätigt, dann strukturell wieder an stable-33.0 angeglichen — erster Eintrag überholt); 2026-08-21 Card-Panel-Revert (SES-578).
**Doku:** COMPONENTS.md Zeile 71 — "prime merge-conflict hotspot", GA4-Tracking auf fast jedem Control, `IONOS_BUILD`/`STRATO_WL_BUILD`-Verzweigungen.
**Letzte Änderungen:** `dd2b5fcc4`, `b9e530504`, `80100217e`, `45446a124`.
**Diff-Grund:** SES-576-Strukturangleichung + SES-578-Dark-Mode — beides noch nicht in `develop_stable-4.0` gemerged.

### NetworkSettings (`networksettings.cpp/.ui`)
**Navigation:** Settings-Dialog-Toolbar → "Netzwerk"-Tab, bzw. eingebettet als `connectionSettingsPanel` in AccountSettings
**Bisherige Entscheidungen:** **Kontext:** `connectionSettingsPanel` (NetworkSettings-Widget) in AccountSettings sollte nicht mehr sichtbar sein — Eintrag in DECISIONS.md Zeile 9 (Details dort, nicht dupliziert).
**Doku:** COMPONENTS.md Zeile 72 — "mostly generic upstream logic, minor whitelabel touch (`doNotUseProxy()`/`forceSystemNetworkProxy()`)".
**Letzte Änderungen:** überwiegend ältere Palette-/Hintergrund-Fixes (`293ac3836`, `4c79d7586`, `e6b62126b`), keine aktuellen SES-578-Commits.
**Diff-Grund:** ältere, bereits vor SES-578 bestehende Whitelabel-Abweichung.

### SettingsDialog (`settingsdialog.cpp/.h/.ui`)
**Navigation:** Tray → Account-Menü → Zahnrad, oder Tray → Rechtsklick-Kontextmenü → "Einstellungen"
**Bisherige Entscheidungen:** keine direkten Treffer in DECISIONS.md unter diesem Namen (Kontrast-Fix-Eintrag 2026-08-17 betrifft das Account-Icon in der Toolbar, siehe unten separat).
**Doku:** COMPONENTS.md Zeile 70 — "Heavily fork-specific ... structurally diverges a lot from upstream's simpler settings dialog", bekannter Merge-Hotspot laut `.claude/context/gui/CLAUDE.md`.
**Letzte Änderungen:** `dd2b5fcc4`, `fa36ab8b0` (Min/Max-Buttons wiederhergestellt), `d4351e804`, `ad5512832` — alle SES-578.
**Diff-Grund:** SES-578 Dark-Mode-Arbeit am zentralen Settings-Fenster, größter struktureller Abstand zu `develop_stable-4.0`.

### FolderStatusDelegate (`folderstatusdelegate.cpp/.h`)
**Navigation:** Settings-Dialog → Account-Tab → Ordnerliste (kein eigener Dialog, nur die Zeilen-Darstellung der Ordnerliste)
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** COMPONENTS.md Zeile 41 — "Uses whitelabeltheme.h for custom styling"; merge-drift-map: 72% Diff-Anteil, **nicht vollständig geprüft**.
**Letzte Änderungen:** `8534f6865` (Tint-Helper-Konsolidierung), `4bfe75f6b`, `45446a124`.
**Diff-Grund:** SES-578-Feinschliff; siehe merge-drift-map für offenen Vertiefungsbedarf.

### FolderWizard (`folderwizard.cpp/.h`)
**Navigation:** Tray/Settings → "Ordner hinzufügen" → mehrseitiger Wizard (lokaler Pfad → Remote-Pfad → Selective Sync/VFS)
**Bisherige Entscheidungen:** keine direkten Treffer (aber `ModernStyle`-Regression-Fix `1f735ebbd` "fixing a SES-457 regression" ist im Git-Log dokumentiert).
**Doku:** COMPONENTS.md Zeile 48 — `IONOS_BUILD`-Ifdef für whitelabel-VFS-Checkbox-Setup.
**Letzte Änderungen:** `2d32bc2ab`, `1f735ebbd` — SES-578.
**Diff-Grund:** SES-578-Hintergrund-/Selective-Sync-Header-Fixes.

### SelectiveSyncDialog (`selectivesyncdialog.cpp`)
**Navigation:** Account-Kontextmenü → "Ordner auswählen", oder Teil des FolderWizard-Flows
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** COMPONENTS.md Zeile 49 — "Includes whitelabeltheme.h for customizeStyle()".
**Letzte Änderungen:** `2d32bc2ab`, `4bfe75f6b`, `45446a124`.
**Diff-Grund:** SES-578.

### IgnoreListTableWidget (`ignorelisttablewidget.cpp`)
**Navigation:** Settings-Dialog → Allgemein → "Ignorierte Dateien bearbeiten" → Tabellen-Widget innerhalb des `IgnoreListEditor`-Dialogs
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** COMPONENTS.md Zeile 51 — "Generic Nextcloud upstream" (Basis-Widget), aber merge-drift-map vermerkt eine echte Logikänderung (`slotAddPattern()` von statischem `QInputDialog::getText()` auf Instanz umgebaut) unter überwiegend additivem Styling-Diff — **Vorsicht bei künftigen Merges**.
**Letzte Änderungen:** `4bfe75f6b`, `45446a124`.
**Diff-Grund:** SES-578 + die o. g. Logikänderung.

### AddCertificateDialog (`addcertificatedialog.cpp`)
**Navigation:** Login-Flow bei client-zertifikatsbasierter Authentifizierung → Zertifikat-Auswahl-Dialog
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** COMPONENTS.md Zeile 27 — generisch, keine Fork-Vermerke.
**Letzte Änderungen:** überwiegend alte Merges/SPDX-Migration, keine aktuellen SES-578-Commits.
**Diff-Grund:** reiner Versionsstand-Unterschied zu `develop_stable-4.0` (SES-551-Merge-Historie), keine dark-mode-spezifische Änderung erkennbar — **geringste Priorität dieser Liste**.

### CaseClashFilenameDialog / InvalidFilenameDialog / ConflictDialog (`caseclashfilenamedialog.cpp`, `invalidfilenamedialog.cpp`, `conflictdialog.cpp`)
**Navigation:** Sync-Konflikt-Benachrichtigung (Tray-Activity oder automatisch bei erkanntem Konflikt) → jeweiliger Rename-/Konflikt-Dialog
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** COMPONENTS.md Zeilen 53/58/59 — generisch upstream, `invalidfilenamedialog.cpp` inkl. `whitelabeltheme.h`.
**Letzte Änderungen:** alle drei zuletzt via `45446a124` (dialogBackgroundColor/titleColor-Rollout SES-578).
**Diff-Grund:** SES-578, gebündelt über den gemeinsamen `dialogBackgroundColor()`/`titleColor()`-Refactor.

### LegacyAccountSelectionDialog (`legacyaccountselectiondialog.cpp`)
**Navigation:** Erststart/Migration von einem älteren Client → Checklisten-Dialog zur Auswahl der zu importierenden Legacy-Accounts
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** COMPONENTS.md Zeile 20 — "Fork-specific: styled via whitelabeltheme.h/WLTheme und buttonstyle.h".
**Letzte Änderungen:** `45446a124`, `be5a49f7a` (SES-554), `f389830ee` (SES-493 Legacy-Import-Darkmode).
**Diff-Grund:** SES-578 + ältere SES-493-Dark-Mode-Historie.

### LinkButton (`linkbutton.cpp/.h`)
**Navigation:** kein eigener Klickpfad — wiederverwendetes Hyperlink-Label, u. a. in GeneralSettings/DataProtection-Seiten eingebettet.
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** COMPONENTS.md Zeile 133 — **kürzlich korrigiert** (in diesem Gespräch, s. gui-context-refresh-Lauf heute): war fälschlich als "kein Fork-Marker" gelistet, nutzt aber `WLTheme.settingsLinkColor()`.
**Letzte Änderungen:** `cba5a70e8` (Re-Apply on `changeEvent()`, SES-578).
**Diff-Grund:** SES-578.

### sesSnackBar (`sessnackbar.cpp/.h`)
**Navigation:** kein eigener Klickpfad — erscheint kontextabhängig als Benachrichtigungsleiste (z. B. Fehler-/Erfolgsmeldung) in mehreren Dialogen.
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** COMPONENTS.md Zeile 129 — "Fork-specific, not present upstream".
**Letzte Änderungen:** `cba5a70e8` (Zustand jetzt getrackt, re-styled on `changeEvent()`), `8534f6865`, `4bfe75f6b`.
**Diff-Grund:** fork-only (`n/a` bei stable-33.0 in merge-drift-map) + SES-578-Reaktivität.

---

## Wizard (`src/gui/wizard/`)

*Basis-Doku bereits vollständig in [`wizard/CLAUDE.md`](wizard/CLAUDE.md).*

### DataProtectionPage / DataProtectionSettingsPage
**Navigation:** Account-Setup-Wizard, Flow2-Auth-Zweig (`IONOS_BUILD`) → nach Credentials → Datenschutz-Consent-Seite → optional "Einstellungen" → Detail-Seite
**Bisherige Entscheidungen:** keine Treffer zu SES-578; ältere SES-391/SES-522-Einträge im Git-Log (Connect-Mehrfachausführung, STRATO-Übersetzung) — nicht in DECISIONS.md, da reine Bugfixes.
**Doku:** wizard/CLAUDE.md — "whitelabel consent/tracking notice (IONOS/STRATO)".
**Letzte Änderungen:** beide `45446a124` (SES-578).
**Diff-Grund:** SES-578 + fork-only (existiert nicht auf `origin/stable-33.0`, laut merge-drift-map).

### OwncloudAdvancedSetupPage
**Navigation:** Account-Setup-Wizard → letzte Seite: lokaler Ordner, Sync-Modus, Quota
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** wizard/CLAUDE.md — "final page; local folder picker, sync-everything/selective-sync/VFS radio choice".
**Letzte Änderungen:** `cba5a70e8` (Q_OS_MAC-Guard für `syncModeLabel` entfernt, SES-578).
**Diff-Grund:** SES-578.

### OwncloudConnectionMethodDialog / OwncloudSetupPage / OwncloudWizard / Flow2AuthCredsPage / Flow2AuthWidget
**Navigation:** Account-Setup-Wizard, jeweils frühe Server-URL-/Auth-Auswahl-Schritte bzw. der Wizard-Container selbst
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** wizard/CLAUDE.md deckt alle fünf ab — größtenteils generischer Upstream-Code, keine SES-578-Berührung.
**Letzte Änderungen:** überwiegend `b7d4a8b53` (SES-551-Merge) oder älter — **keine aktuellen SES-578-Commits**.
**Diff-Grund:** reiner Versionsstand-Unterschied zu `develop_stable-4.0`, nicht dark-mode-getrieben — niedrige Priorität.

---

## SesComponents (fork-eigen, kein Upstream-Pendant)

### SesErrorBox.qml
**Navigation:** z. B. `filedetails/ShareDetailsPage.qml` bei Passwort-Fehlern
**Bisherige Entscheidungen:** keine direkten Treffer.
**Doku:** SesComponents/CLAUDE.md — "reusable inline error banner".
**Letzte Änderungen:** `8534f6865` (Tint-Helper-Konsolidierung), `cd9d7535e`.
**Diff-Grund:** fork-only.

### SesTrayHeader.qml
**Navigation:** Tray-Popup-Header (Account-Menü-Button, "Website"-Link, Ordner-Button)
**Shadow-Component:** Fork-Ersatz für `TrayWindowHeader.qml` — siehe shadow-component-watch/registry.md, zuletzt geprüft `568cbe171`, keine offenen Portierungs-Kandidaten.
**Doku:** SesComponents/CLAUDE.md + tray/CLAUDE.md.
**Letzte Änderungen:** `7cc9c7e88` (SES-589).
**Diff-Grund:** fork-only.

---

## Tray (`src/gui/tray/`)

*Basis-Doku bereits vollständig in [`tray/CLAUDE.md`](tray/CLAUDE.md).*

### MainWindow.qml
**Navigation:** Tray-Icon-Klick → gesamtes Popup-Fenster
**Bisherige Entscheidungen:** 2026-08-17 `panelBackgroundColor()` entfernt, Settings/Tray-Hintergrund vereinheitlicht.
**Letzte Änderungen:** `26fe47ea7` (SES-579, Unified Search reaktiviert).
**Diff-Grund:** mehrere SES-579/SES-578-Runden.

### TrayWindowAccountMenu.qml / UserLine.qml
**Navigation:** Tray → Header → Account-Button → Dropdown mit Account-Zeilen
**Shadow-Component:** Fork-Ersatz für `CurrentAccountHeaderButton.qml` — registriert, zuletzt geprüft `60e0cb196`, keine offenen Punkte (User-Status-Feature bewusst nicht portiert, SES-50).
**Bisherige Entscheidungen:** Chevron-Dark-Mode-Fix (`9647f1174`), Status-Feature-Abweichung (SES-50), beide 2026-08-17/18.
**Letzte Änderungen:** `9647f1174`, `78b411a57` (SES-579), `7de66b861`/`b84dca7b4` (SES-579 Layout-Fixes).
**Diff-Grund:** SES-578/579 kombiniert, hoher Diff-Anteil laut merge-drift-map (`UserLine.qml` ~110%).

### AccountMenuItem.qml / ActivityItemContent.qml / SyncStatus.qml / UnifiedSearchInputContainer.qml / UnifiedSearchResultListItem.qml / UserStatusMessageView.qml
**Navigation:** jeweils Teil des Tray-Popup-Baums (Account-Menü-Zeile, Activity-Feed-Eintrag, Sync-Status-Icon, Unified-Search-Feld/-Ergebnis, User-Status-Anzeige) — Details siehe tray/CLAUDE.md.
**Bisherige Entscheidungen:** Unified-Search-Reaktivierung 2026-08-18 (SES-579) betrifft `UnifiedSearchInputContainer.qml` direkt.
**Letzte Änderungen:** alle zuletzt via `f3477411d` (SES-578 Tray-Dark-Mode) oder `26fe47ea7` (SES-579); `ActivityItemContent.qml`/`UnifiedSearchResultListItem.qml` zusätzlich `70a6a94dd` (SES-596 Font-Fallback, siehe Merge-Konsistenz-Check).
**Diff-Grund:** SES-578/579 (+ SES-596 bei den beiden genannten).

### CurrentAccountHeaderButton.qml / TrayWindowHeader.qml (Legacy)
**Status:** laut tray/CLAUDE.md **ungenutzt** (nicht im aktiven Render-Baum), aber **bewusst nicht gelöscht** — `origin/stable-33.0` führt beide Dateien noch aktiv (Entscheidung 2026-08-17, "Grundsatz: toter Code bleibt erhalten, solange stable ihn noch führt").
**Letzte Änderungen:** `CurrentAccountHeaderButton.qml` zusätzlich `70a6a94dd` (SES-596 Font-Fallback — mitgezogen, obwohl inaktiv, siehe Merge-Konsistenz-Check).
**Diff-Grund:** rein struktureller Altbestand, keine aktive Weiterentwicklung nötig — **niedrigste Priorität**, ggf. sogar ignorierbar für einen `develop_stable-4.0`-Abgleich, da funktional inaktiv.

### Neu im Diff seit dem Merge (nicht neu erstellt, s. Merge-Konsistenz-Check Punkt 2): ActivityItem.qml / IconButton.qml / SecondaryPillButton.qml / TrayFolderListItem.qml / TrayFoldersMenuButton.qml
**Navigation:** Teil des Tray-Popup-Baums — Activity-Feed-Zeile, generischer Icon-Button-Baustein, sekundäre Pill-Button-Variante, Ordner-Picker-Popup-Zeile bzw. -Button (Details siehe tray/CLAUDE.md, alle dort bereits als Bausteine gelistet).
**Bisherige Entscheidungen:** keine Treffer (nur eine beiläufige Erwähnung von `TrayFoldersMenuButton.qml` als Analogie-Beispiel im Chevron-Fix-Eintrag vom 2026-08-18, keine eigene Entscheidung dazu).
**Letzte Änderungen:** ausschließlich `70a6a94dd` (SES-596 Font-Fallback) — vorher gab es zu diesen 5 Dateien keinen Unterschied zu `develop_stable-4.0`.
**Diff-Grund:** ausschließlich SES-596, nicht SES-578 — vor dem Merge nicht im Diff, da diese Dateien bis dahin identisch mit der Basis waren.

---

## macOS FileProvider UI (`src/gui/macOS/ui/`)

*Basis-Doku bereits vollständig in [`macOS/CLAUDE.md`](macOS/CLAUDE.md).*

### FileProviderSettings.qml / FileProviderFileDelegate.qml / FileProviderStorageInfo.qml / FileProviderSyncStatus.qml / FileProviderEvictionDialog.qml
**Navigation:** Settings-Dialog → Account-Tab → "Virtual Files"-Einstellungen (macOS-only) bzw. Datei-Detailansicht/Eviction-Liste
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** macOS/CLAUDE.md — Backing-Klassen `FileProviderSettingsController`/`FileProviderItemMetadata`.
**Letzte Änderungen:** überwiegend ältere Button-Farb-Fixes (`f05f53c56` SES-482, `996ae46c6` SES-467) oder generische Upstream-Fixes (`9d2333c4e`, `89104e77b`) — **keine aktuellen SES-578-Commits**.
**Diff-Grund:** reiner Versionsstand-Unterschied, macOS-only, niedrige Priorität für diesen (Windows-fokussierten) Dark-Mode-Branch.

---

## Sonstiges

### ShareeSearchField.qml (`filedetails/`)
**Navigation:** Datei-Detailansicht → Freigabe-Tab → Personen-Suchfeld
**Bisherige Entscheidungen:** 2026-08-18 Unified-Search-Wiederaktivierung referenziert dieses Feld als Farbvorbild.
**Letzte Änderungen:** `cd9d7535e` (SES-578).
**Diff-Grund:** SES-578 + Namespace-Rename (merge-drift-map: "mittel"-Risiko).

### FileDetailsPage.qml (`filedetails/`, neu seit dem Merge)
**Navigation:** Datei-Detailansicht (Freigabe-/Aktivitäts-Tab-Container)
**Bisherige Entscheidungen:** keine Treffer.
**Letzte Änderungen:** ausschließlich `70a6a94dd` (SES-596 Font-Fallback) — vorher kein Unterschied zu `develop_stable-4.0`.
**Diff-Grund:** ausschließlich SES-596.

### FileActionsWindow.qml (`integration/`)
**Navigation:** Tray/Explorer-Kontextmenü → serverdefinierte Datei-Aktion → Popup-Fenster mit Aktionsliste
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** integration/CLAUDE.md — "fairly new, generic Nextcloud functionality, not obviously fork-specific".
**Letzte Änderungen:** generische Upstream-Fixes (`dc844a287`, `cfbd9042f`), keine SES-578-Berührung.
**Diff-Grund:** reiner Versionsstand-Unterschied, niedrige Priorität.

### WebFlowCredentialsDialog (`creds/`)
**Navigation:** Login/Re-Auth-Flow → Anmeldedialog (hostet Flow2AuthWidget oder WebEngine-View)
**Bisherige Entscheidungen:** keine Treffer.
**Doku:** creds/CLAUDE.md — "styles itself via whitelabeltheme.h/WLTheme ... BRICKMAKERS whitelabel-branding code".
**Letzte Änderungen:** generische Upstream-Fixes, keine SES-578-Berührung.
**Diff-Grund:** reiner Versionsstand-Unterschied.

---

## Zusammenfassung: Änderungsumfang dieses Branches gegenüber seiner Basis `develop_stable-4.0`

Diese Einordnung beschreibt **wie viel eigene Arbeit** in jeder Komponente steckt, seit der Branch von `develop_stable-4.0` abgezweigt wurde — z. B. relevant für eine Review-/PR-Beschreibung oder um abzuschätzen, welche Komponenten am stärksten vom SES-578-Umbau betroffen sind. **Keine Merge-Zielrichtung, keine Konflikt-Prognose** (dafür `stable-merge-check`/`merge-drift-map` gegen den tatsächlichen Referenz-Branch `stable-33.0`).

**SES-578-Kernarbeit** (größter Änderungsumfang ggü. der Basis, überwiegend Dark-Mode-getrieben): SettingsDialog, GeneralSettings, AccountSettings, sesSnackBar, LinkButton, FolderStatusDelegate, MainWindow.qml, TrayWindowAccountMenu.qml/UserLine.qml, SesErrorBox.qml, ShareeSearchField.qml, DataProtectionPage/-SettingsPage.

**SES-578 mitgezogen, aber selbst nicht Kernthema**: FolderWizard, SelectiveSyncDialog, IgnoreListTableWidget, CaseClashFilenameDialog/InvalidFilenameDialog/ConflictDialog, LegacyAccountSelectionDialog, OwncloudAdvancedSetupPage, restliche Tray-QML-Dateien (AccountMenuItem, ActivityItemContent, SyncStatus, UnifiedSearch*, UserStatusMessageView).

**Änderung liegt vor SES-578** (ältere, seit dem Abzweigen von `develop_stable-4.0` bereits bestehende Abweichung, keine aktuelle SES-578-Berührung): AddCertificateDialog, NetworkSettings, alle macOS-FileProvider-QML-Dateien, FileActionsWindow.qml, WebFlowCredentialsDialog, OwncloudConnectionMethodDialog/OwncloudSetupPage/OwncloudWizard/Flow2AuthCredsPage/Flow2AuthWidget.

**Bewusst funktional inaktiv, nicht anfassen**: CurrentAccountHeaderButton.qml, TrayWindowHeader.qml (auf stable noch geführt, bewusst nicht gelöscht).

**Neu seit dem Merge, ausschließlich SES-596** (Font-Fallback-Fix, keine Dark-Mode-Berührung): FileDetailsPage.qml, ActivityItem.qml, IconButton.qml, SecondaryPillButton.qml, TrayFolderListItem.qml, TrayFoldersMenuButton.qml.
