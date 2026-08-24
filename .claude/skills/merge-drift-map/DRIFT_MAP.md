# Merge-Drift-Landkarte

Wird vom [merge-drift-map](SKILL.md)-Skill Datei für Datei/Komponente für Komponente befüllt — kein Vollscan, sondern nur tatsächlich geprüfte Stellen. Trackt nur den **Ist-Zustand** je Datei, keine Verlaufs-/Rundenhistorie — eine erneute Prüfung aktualisiert die bestehende Zeile statt einen neuen Abschnitt anzuhängen.

## Legende

**Diff-Anteil** (geänderte Zeilen ÷ Zeilen auf `stable-33.0`): wie *oft* ein Merge hier vermutlich einen Textkonflikt auslöst — sagt nichts darüber, wie gefährlich der wäre.

**Qualitative Einstufung** (niedrig/mittel/hoch, aus [stable-merge-check](../stable-merge-check/SKILL.md)s Workflow A): wie *gefährlich* eine Abweichung tatsächlich ist — additive Whitelabel-Ergänzungen sind unkritisch, umbenannte/umstrukturierte Stellen können hart kollidieren oder sich sogar unbemerkt falsch zusammenführen.

Kombiniert: niedrig+niedrig = unauffällig, hoch+niedrig = viele harmlose Konflikte, niedrig+hoch = seltene, aber gefährliche Stellen, hoch+hoch = besondere Vorsicht.

## Erfasste Dateien

*Zuletzt aktualisiert: eigener Commit `0b9c2b950`, stable-Seite `28fdc6898`/`9e65fef46` (je Zeile s. Spalte, sofern abweichend geprüft)*

| Datei | Auf stable? | Diff-Anteil | Qualitative Einstufung | Bewusst/Drift | Zuletzt geprüft | Bemerkung |
|---|---|---|---|---|---|---|
| `src/gui/settingsdialog.cpp` | ja | ~49% (298/607) | **hoch** | gemischt | `9d3af4e33` / `9e65fef46` | bekannter Merge-Hotspot, `IONOS_BUILD`-Verzweigungen |
| `src/gui/application.cpp` | ja | ~35% (424/1205) | **hoch** | bewusst | `2f77361ec` / `9e65fef46` | Änderungen über nahezu die gesamte Datei verteilt (23 Hunks: Konstruktor, Destruktor, `setupAccountsAndFolders`, `setupLogging`, `event()`); mehrere Hunks mit Netto-Zeilenreduktion (echte Restrukturierung, nicht nur Anbau) — u. a. DWM-Dark-Titlebar, `sesStyle`-Konstruktion, ShellExtensionsServer |
| `src/gui/foldercreationdialog.cpp` | ja | ~74% (64/86) | **hoch** | gemischt | `2f77361ec` / `9e65fef46` | Styling-Teil additiv/bewusst, aber `accept()`-Guard `QDir(fullPath).exists()` entfernt + `ui->labelErrorMessage`→`ui->errorSnackbar` umbenannt — echte Logik-/Identifier-Änderung, höchstes Konfliktrisiko dieser Dialog-Gruppe |
| `src/gui/generalsettings.cpp` | ja | ~55% (451/827) | **hoch** | bewusst | `2f77361ec` / `9e65fef46` | großflächig über Konstruktor/Destruktor/fast alle Slots verteilt, u. a. Identifier-Rename `advanced_groupBox`→`advancedGroupBox` (Merge-Angleichung) und neue Card-Panel-Optik |
| `src/gui/tray/ActivityItemContent.qml` | ja | ~45% (137/307) | **hoch** | bewusst | `2f77361ec` / `9e65fef46` | `Button`→eigene `IconButton`-Komponente, komplett neue MenuItem-Hintergrund-Delegates mit Hover/Pressed/Tooltip — echte Restrukturierung |
| `src/gui/tray/UserLine.qml` | ja | **~110%** (286/261) | **hoch** | bewusst | `2f77361ec` / `9e65fef46` | Diffgröße übersteigt die gesamte stable-Datei — `Menu`→eigenes `Popup`, Avatar-Status-Overlay entfernt, `Button`→`IconButton`, Farblogik von Palette-Kaskaden auf `Style.ses*` — praktisch Neuimplementierung; am stärksten umgebaute Datei der gesamten Karte |
| `src/gui/folderstatusdelegate.cpp` | ja | ~72% (349/482) | **nicht vollständig geprüft** | — | `9d3af4e33` / `9e65fef46` | nur SES-578-Teiländerung inhaltlich angesehen, nicht volle Historie — Kandidat für eine vertiefte `stable-merge-check`-Runde |
| `src/gui/ignorelisttablewidget.cpp` | ja | ~76% (146/193) | mittel | bewusst | `2f77361ec` / `9e65fef46` | Großteil reiner Anbau (`customize*Style()`), aber `slotAddPattern()` von statischem `QInputDialog::getText()` auf Instanz mit `exec()`/`textValue()` umgebaut — echte Logikänderung |
| `src/gui/filedetails/ShareeSearchField.qml` | ja | ~13% (31/232) | mittel | gemischt | `2f77361ec` / `9e65fef46` | Namespace-Rename (projektweite Whitelabel-Umbenennung, systemweite Konfliktstelle) + bewusste SES-578-Dark-Mode-Fixes; vormals palette-basierte Werte durch `Style.*` ersetzt |
| `src/gui/UserStatusMessageView.qml` | ja | ~4% (10/229) | mittel | gemischt | `2f77361ec` / `9e65fef46` | Namespace-Rename (gleiche systemweite Konfliktlinie) + reine Ergänzung eines `indicator: Image {}`-Blocks |
| `theme/Style/Style.qml` | ja | ~60% (130/216) | niedrig | bewusst (Whitelabel) | `9d3af4e33` / `9e65fef46` | fast nur zusätzliche `ses*`-Properties, Kernfunktionen unverändert |
| `src/gui/folderwizard.cpp` | ja | ~63% (467/736) | niedrig | bewusst | `9d3af4e33` / `9e65fef46` | additive Whitelabel-Styles + eine bewusste Bugfix-Zeile (`ModernStyle`) |
| `src/gui/accountsettings.cpp` | ja | ~30% (586/1927) | niedrig | bewusst | `9d3af4e33` / `9e65fef46` | Struktur deckungsgleich, nur Branding-Werte |
| `src/gui/legacyaccountselectiondialog.cpp` | ja | ~61% (31/51) | niedrig | bewusst | `2f77361ec` / `9e65fef46` | hoher Prozentsatz nur wegen kleiner Ausgangsdatei; inhaltlich reine Additionen |
| `src/gui/selectivesyncdialog.cpp` | ja | ~19% (106/553) | niedrig | bewusst | `9d3af4e33` / `9e65fef46` | fast rein additiv |
| `src/gui/conflictdialog.cpp` | ja | ~21% (36/175) | niedrig | bewusst | `2f77361ec` / `9e65fef46` | reine Additionen, macOS-only QCheckBox-Styles |
| `src/gui/caseclashfilenamedialog.cpp` | ja | ~17% (47/280) | niedrig | bewusst | `2f77361ec` / `9e65fef46` | reine Additionen |
| `src/gui/invalidfilenamedialog.cpp` | ja | ~16% (51/314) | niedrig | bewusst | `2f77361ec` / `9e65fef46` | fast nur Additionen, 2 Deletions rein kosmetisch |
| `src/gui/tray/UnifiedSearchInputContainer.qml` | ja | ~15% (15/98) | niedrig | bewusst | `2f77361ec` / `9e65fef46` | reine Ergänzungen (Hintergrund-Rectangle, Farbzuweisungen) |
| `src/gui/application.h` | ja | ~2% (4/162) | niedrig | bewusst | `2f77361ec` / `9e65fef46` | reine Methodendeklarations-Additionen (`eventFilter`, `startTracking`/`stopTracking`) |
| `theme.cpp` (`src/libsync/theme.cpp`) | ja | ~17% (195/1156) | niedrig | bewusst | `9d3af4e33` / `9e65fef46` | vollständig gegen stable durchgegangen |
| `src/gui/owncloudgui.cpp` | ja | ~15% (115/760) | niedrig | bewusst | `9d3af4e33` / `9e65fef46` | |
| `src/libsync/theme.h` | ja | ~3% (23/682) | niedrig | bewusst | `9d3af4e33` / `9e65fef46` | |
| `src/gui/systray.cpp` | ja | ~3,6% (39/1086) | niedrig | bewusst | `0b9c2b950` / `28fdc6898` | fast rein additiv (neuer `darkModeChanged`-Connect, `WLTheme`-Styling fürs Kontextmenü); eine kleine unabhängige Logik-Ergänzung (`localPath.isEmpty()`-Guard in `presentShareViewInTray`), selbst additiv, keine bestehende Zeile geändert |
| `src/gui/CMakeLists.txt` | ja | ~7,3% (55/758) | niedrig | bewusst | `0b9c2b950` / `28fdc6898` | praktisch nur `list(APPEND client_SRCS ...)`-Blöcke für neue Whitelabel-/SES-Dateien plus ein neues `SKIP_MACDEPLOYQT`-Option; eine unabhängige Quoting-Korrektur bei `-executable=` |
| `src/common/utility.h` | ja | **0%** | keins | — | `9d3af4e33` / `9e65fef46` | identisch zu stable (frühere `isWindows11OrGreater()`-Verlagerung wieder zurückgerollt) |
| `src/common/utility_win.cpp` | ja | **0%** | keins | — | `9d3af4e33` / `9e65fef46` | dito |
| `src/gui/buttonstyle.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `9d3af4e33` | |
| `src/gui/stratotheme.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `9d3af4e33` | |
| `src/gui/basetheme.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `9d3af4e33` | |
| `src/gui/sessnackbar.cpp` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `9d3af4e33` | |
| `src/gui/sessnackbar.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `2f77361ec` | |
| `src/gui/SesComponents/SesErrorBox.qml` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `2f77361ec` | |
| `src/gui/wizard/dataprotectionpage.cpp` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `2f77361ec` | |
| `src/gui/wizard/dataprotectionsettingspage.cpp` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `2f77361ec` | |
| `src/gui/tray/AccountMenuItem.qml` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `2f77361ec` | SES-578-Neubau |
| `src/gui/tray/TrayWindowAccountMenu.qml` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `2f77361ec` | SES-578-Neubau |
| `src/gui/sesstyle.cpp` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `0b9c2b950` | zentrale `sesStyle`-Implementierung (269 Zeilen), Kern der SES-578-Theming-Arbeit |
| `src/gui/sesstyle.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | `0b9c2b950` | Header dazu (78 Zeilen) |

## Auffällige Kombinationen

- **hoch+hoch:** keine aktuell in der Karte.
- **hoch Diff-Anteil + niedriges/mittleres Risiko** (häufige, aber überwiegend harmlose Konflikte zu erwarten): `folderstatusdelegate.cpp` (72%, noch nicht vollständig qualitativ geprüft), `ignorelisttablewidget.cpp` (76%), `folderwizard.cpp` (63%), `legacyaccountselectiondialog.cpp` (61%), `Style.qml` (60%).
- **niedrig Diff-Anteil + hoch Risiko** (selten, aber gefährlich): keine aktuell in der Karte — die frühere `isWindows11OrGreater()`-Verlagerung wäre ein Beispiel gewesen, ist aber bereits zurückgerollt (daher jetzt 0% bei `utility.h`/`utility_win.cpp`).
- **Versteckte Logikänderungen unter additivem Styling-Diff:** `foldercreationdialog.cpp` und `ignorelisttablewidget.cpp` — bei künftigen Merges nicht nur auf Konflikt-Menge, sondern auf den Inhalt der geänderten Zeilen achten.
- **Ungünstigste Kombination (hoher Diff-Anteil + hohes Risiko):** `settingsdialog.cpp`, `application.cpp`, `foldercreationdialog.cpp`, `generalsettings.cpp`, `ActivityItemContent.qml`, `UserLine.qml` — verdienen vor jedem `stable-33.0`-Merge besondere Aufmerksamkeit.

Damit sind alle 46 von SES-578-Commits berührten `src/gui/`- und `src/libsync/`-Dateien in dieser Karte erfasst.
