# Merge-Drift-Landkarte

Wird vom [merge-drift-map](SKILL.md)-Skill Datei für Datei/Komponente für Komponente befüllt — kein Vollscan, sondern nur tatsächlich geprüfte Stellen.

## Legende

**Diff-Anteil** (geänderte Zeilen ÷ Zeilen auf `stable-33.0`): wie *oft* ein Merge hier vermutlich einen Textkonflikt auslöst — sagt nichts darüber, wie gefährlich der wäre.

**Qualitative Einstufung** (niedrig/mittel/hoch, aus [stable-merge-check](../stable-merge-check/SKILL.md)s Workflow A): wie *gefährlich* eine Abweichung tatsächlich ist — additive Whitelabel-Ergänzungen sind unkritisch, umbenannte/umstrukturierte Stellen können hart kollidieren oder sich sogar unbemerkt falsch zusammenführen.

Kombiniert: niedrig+niedrig = unauffällig, hoch+niedrig = viele harmlose Konflikte, niedrig+hoch = seltene, aber gefährliche Stellen, hoch+hoch = besondere Vorsicht.

## SES-578-Dateien (Erstbefüllung)

*Eigener Commit: `9d3af4e33` (2026-08-20) · stable-Seite: `9e65fef46`*

| Datei | Auf stable? | Diff-Anteil | Qualitative Einstufung | Bewusst/Drift | Bemerkung |
|---|---|---|---|---|---|
| `src/gui/settingsdialog.cpp` | ja | ~49% (298/607) | **hoch** | gemischt | bekannter Merge-Hotspot, `IONOS_BUILD`-Verzweigungen |
| `src/gui/folderstatusdelegate.cpp` | ja | ~72% (349/482) | **nicht vollständig geprüft** | — | nur SES-578-Teiländerung heute inhaltlich angesehen, nicht volle Historie |
| `theme/Style/Style.qml` | ja | ~60% (130/216) | niedrig | bewusst (Whitelabel) | fast nur zusätzliche `ses*`-Properties, Kernfunktionen unverändert |
| `src/gui/folderwizard.cpp` | ja | ~63% (467/736) | niedrig | bewusst | additive Whitelabel-Styles + eine bewusste Bugfix-Zeile (`ModernStyle`) |
| `src/gui/accountsettings.cpp` | ja | ~30% (586/1927) | niedrig | bewusst | Struktur deckungsgleich, nur Branding-Werte |
| `src/gui/selectivesyncdialog.cpp` | ja | ~19% (106/553) | niedrig | bewusst | fast rein additiv |
| `theme.cpp` (`src/libsync/theme.cpp`) | ja | ~17% (195/1156) | niedrig | bewusst | heute vollständig gegen stable durchgegangen |
| `src/gui/owncloudgui.cpp` | ja | ~15% (115/760) | niedrig | bewusst | |
| `src/libsync/theme.h` | ja | ~3% (23/682) | niedrig | bewusst | |
| `src/common/utility.h` | ja | **0%** | keins | — | seit heutigem Revert exakt identisch zu stable |
| `src/common/utility_win.cpp` | ja | **0%** | keins | — | dito |
| `src/gui/buttonstyle.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/stratotheme.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/basetheme.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/sessnackbar.cpp` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |

**Auffällige Kombinationen:**
- **hoch+hoch:** keine in dieser ersten Runde.
- **hoch Diff-Anteil + niedriges Risiko** (häufige, aber harmlose Konflikte zu erwarten): `folderstatusdelegate.cpp` (72%, aber noch nicht vollständig qualitativ geprüft — Kandidat für eine vertiefte `stable-merge-check`-Runde), `folderwizard.cpp` (63%), `Style.qml` (60%).
- **niedrig Diff-Anteil + hoch** (selten, aber gefährlich): keine aktuell in der Karte — die frühere `isWindows11OrGreater()`-Verlagerung wäre ein Beispiel gewesen, ist aber bereits zurückgerollt (daher jetzt 0% bei `utility.h`/`utility_win.cpp`).
- `settingsdialog.cpp` bleibt die Datei mit der ungünstigsten Kombination (hoher Diff-Anteil, hohes Risiko) — verdient vor jedem Merge besondere Aufmerksamkeit.

## SES-578-Dateien (Zweite Runde — alle verbleibenden Branch-Dateien)

*Eigener Commit: `2f77361ec`/`9d3af4e33` (2026-08-20) · stable-Seite: `9e65fef46`*

| Datei | Auf stable? | Diff-Anteil | Qualitative Einstufung | Bewusst/Drift | Bemerkung |
|---|---|---|---|---|---|
| `src/gui/application.cpp` | ja | ~35% (424/1205) | **hoch** | bewusst | Änderungen über nahezu die gesamte Datei verteilt (23 Hunks: Konstruktor, Destruktor, `setupAccountsAndFolders`, `setupLogging`, `event()`); mehrere Hunks mit Netto-Zeilenreduktion (echte Restrukturierung, nicht nur Anbau) — u. a. DWM-Dark-Titlebar, `sesStyle`-Konstruktion, ShellExtensionsServer |
| `src/gui/application.h` | ja | ~2% (4/162) | niedrig | bewusst | reine Methodendeklarations-Additionen (`eventFilter`, `startTracking`/`stopTracking`) |
| `src/gui/foldercreationdialog.cpp` | ja | ~74% (64/86) | **hoch** | gemischt | Styling-Teil additiv/bewusst, aber `accept()`-Guard `QDir(fullPath).exists()` entfernt + `ui->labelErrorMessage`→`ui->errorSnackbar` umbenannt — echte Logik-/Identifier-Änderung, höchstes Konfliktrisiko dieser Dialog-Gruppe |
| `src/gui/legacyaccountselectiondialog.cpp` | ja | ~61% (31/51) | niedrig | bewusst | hoher Prozentsatz nur wegen kleiner Ausgangsdatei; inhaltlich reine Additionen |
| `src/gui/generalsettings.cpp` | ja | ~55% (451/827) | **hoch** | bewusst | großflächig über Konstruktor/Destruktor/fast alle Slots verteilt, u. a. Identifier-Rename `advanced_groupBox`→`advancedGroupBox` (Merge-Angleichung) und neue Card-Panel-Optik |
| `src/gui/ignorelisttablewidget.cpp` | ja | ~76% (146/193) | mittel | bewusst | Großteil reiner Anbau (`customize*Style()`), aber `slotAddPattern()` von statischem `QInputDialog::getText()` auf Instanz mit `exec()`/`textValue()` umgebaut — echte Logikänderung |
| `src/gui/conflictdialog.cpp` | ja | ~21% (36/175) | niedrig | bewusst | reine Additionen, macOS-only QCheckBox-Styles |
| `src/gui/caseclashfilenamedialog.cpp` | ja | ~17% (47/280) | niedrig | bewusst | reine Additionen |
| `src/gui/invalidfilenamedialog.cpp` | ja | ~16% (51/314) | niedrig | bewusst | fast nur Additionen, 2 Deletions rein kosmetisch |
| `src/gui/filedetails/ShareeSearchField.qml` | ja | ~13% (31/232) | mittel | gemischt | Namespace-Rename (projektweite Whitelabel-Umbenennung, systemweite Konfliktstelle) + bewusste SES-578-Dark-Mode-Fixes; vormals palette-basierte Werte durch `Style.*` ersetzt |
| `src/gui/tray/ActivityItemContent.qml` | ja | ~45% (137/307) | **hoch** | bewusst | `Button`→eigene `IconButton`-Komponente, komplett neue MenuItem-Hintergrund-Delegates mit Hover/Pressed/Tooltip — echte Restrukturierung |
| `src/gui/tray/UnifiedSearchInputContainer.qml` | ja | ~15% (15/98) | niedrig | bewusst | reine Ergänzungen (Hintergrund-Rectangle, Farbzuweisungen) |
| `src/gui/tray/UserLine.qml` | ja | **~110%** (286/261) | **hoch** | bewusst | Diffgröße übersteigt die gesamte stable-Datei — `Menu`→eigenes `Popup`, Avatar-Status-Overlay entfernt, `Button`→`IconButton`, Farblogik von Palette-Kaskaden auf `Style.ses*` — praktisch Neuimplementierung |
| `src/gui/UserStatusMessageView.qml` | ja | ~4% (10/229) | mittel | bewusst | Namespace-Rename (gleiche systemweite Konfliktlinie) + reine Ergänzung eines `indicator: Image {}`-Blocks |
| `src/gui/SesComponents/SesErrorBox.qml` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/wizard/dataprotectionpage.cpp` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/wizard/dataprotectionsettingspage.cpp` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/sessnackbar.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/tray/AccountMenuItem.qml` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | SES-578-Neubau |
| `src/gui/tray/TrayWindowAccountMenu.qml` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | SES-578-Neubau |

**Auffällige Kombinationen (zweite Runde):**
- **hoch+hoch:** keine (kein Fall mit hohem Diff-Anteil *und* separat als besonders gefährlich markiertem Muster über die reine Einstufung hinaus).
- **Neue "niedrig Diff-Anteil + hoch Risiko"-Kandidaten:** keine.
- **Bemerkenswert:** `foldercreationdialog.cpp` und `ignorelisttablewidget.cpp` sind Fälle, in denen sich echte Logikänderungen unter überwiegend additivem Styling-Diff verstecken — bei künftigen Merges nicht nur auf Konflikt-Menge, sondern auf den Inhalt der geänderten Zeilen achten. `tray/UserLine.qml` (110%) ist die am stärksten umgebaute Datei der gesamten Karte.
- `application.cpp` ist nach dieser Runde die zweite Datei (neben `settingsdialog.cpp`) mit expliziter **hoch**-Einstufung bei gleichzeitig beträchtlichem Diff-Anteil — beide sind zentrale Bootstrap-/Struktur-Dateien und verdienen vor jedem `stable-33.0`-Merge besondere Aufmerksamkeit.
