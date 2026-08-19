# Decision Log

Chronologisches Protokoll: Komponenten-Ausblenden/-Entfernen-Entscheidungen und bewusste Nähe/Abweichung zu stable-x.y. Format, Ablauf und **was hier NICHT reingehört** (Bugfixes, Build-/Kompilierbarkeits-Fixes, reine Aufräumarbeiten) siehe [SKILL.md](SKILL.md).

**Hinweis zum Bestand unten:** Die Einträge bis 2026-08-17 (exkl. dem Avatar-Icon-Kontrast-Eintrag) wurden am 2026-08-18 rückwirkend aus 10 vergangenen Session-Transkripten per Agent-Analyse rekonstruiert, dann am selben Tag auf Boris' engeren Maßstab hin gefiltert (ursprünglich 26 Kandidaten, u. a. Bugfixes/Build-Fixes/reine Aufräumarbeiten entfernt) — bitte gegenlesen.

## 2026-08-11 — Verbindungseinstellungen-Panel ausblenden statt entfernen

**Kontext:** `connectionSettingsPanel` (NetworkSettings-Widget) in AccountSettings sollte nicht mehr sichtbar sein.

**Entscheidung:** Nur `setVisible(false)` (accountsettings.cpp), Code/Widget bleibt vollständig bestehen — analog zum bestehenden `fileProviderPanel`-Muster.

**Verworfene Alternative:** Komponente/Code entfernen — abgelehnt, um die Nähe zu `stable-x.y` zu halten: stable führt diese Komponente weiterhin aktiv, ein Entfernen würde den Diff unnötig vergrößern und jeden künftigen Merge hier komplizierter machen als ein einfaches Sichtbarkeits-Flag.

**Status:** aktiv

## 2026-08-12 — GeneralSettings.ui zunächst als bewusster Redesign-Stand bestätigt (SES-576)

**Kontext:** Merge-Risiko-Analyse zeigte tiefgreifenden Strukturumbau von `generalsettings.ui` gegenüber stable-33.0 (Row-Neunummerierung, Grid→VBox, `aboutAndUpdatesGroupBox` aufgesplittet).

**Entscheidung:** Umbau zunächst als gewollte Redesign-Entscheidung bestätigt und belassen, obwohl das künftige Merges konfliktträchtig macht.

**Status:** überholt durch den Eintrag "GeneralSettings.ui strukturell wieder an stable-33.0 angeglichen" (selber Tag)

## 2026-08-12 — GeneralSettings.ui strukturell wieder an stable-33.0 angeglichen (SES-576)

**Kontext:** Nach genauerer Prüfung: Widget-/Layout-Namen wichen so stark ab (`advanced_groupBox` vs. `advancedGroupBox`, andere Layout-Klasse), dass jede Zeile künftiger stable-Merges kollidieren würde.

**Entscheidung:** Namen/Nummerierung an stable-33.0 angeglichen, eigene BRICKMAKERS-Sektionen ans Ende gehängt statt eingemischt, äußere Layout-Klasse zurück auf `QGridLayout`.

**Verworfene Alternative(n):** Reparenting der BRICKMAKERS-Widgets zur Laufzeit in C++ (mehr Boilerplate, keine Designer-Vorschau); eigener Wrapper `GeneralSettingsPage` um unangetastete `GeneralSettings` — zu großer Umbau, widerspricht "so nah wie möglich an stable-33.0 ohne viel Arbeit".

**Status:** aktiv

## 2026-08-14 — Merge-robuste Zwei-Loop-Struktur für Avatar-Reapply in `customizeStyle()` (SES-576)

**Kontext:** Server-Avatar im Settings-Dialog war per `#ifndef IONOS_BUILD` deaktiviert (Regression aus SES-457-Whitelabel-Umbau); nach Entfernen des Guards musste `customizeStyle()` das Avatar nach jedem Theme-Icon-Reset erneut anwenden.

**Entscheidung:** Zwei Schleifen behalten (generischer Icon-Reset, dann Avatar-Reapply über bestehendes `updateAccountAvatar()`) statt einer zusammengeführten Ein-Loop-Version mit Reverse-Lookup.

**Verworfene Alternative:** Ein-Loop-Version mit Reverse-Lookup-Map — sauberer, aber hätte `AvatarJob::makeCircularAvatar` im IONOS-Zweig dupliziert statt die gemeinsame Funktion zu nutzen; künftige stable-33.0-Änderungen an `updateAccountAvatar()` würden dann still divergieren statt automatisch übernommen zu werden.

**Status:** aktiv

## 2026-08-17 — Verwaiste `TrayWindowHeaderBar.qml` gelöscht

**Kontext:** Beim Aufbau des `shadow-component-watch`-Registry fiel eine dritte, ähnlich benannte Datei im selben Ordner auf.

**Entscheidung:** Gelöscht — per `-S`-Pickaxe über die komplette Historie bestätigt: nie in `resources.qrc`, nie aus `MainWindow.qml`/`TrayWindowHeader.qml`/`SesTrayHeader.qml` referenziert, nie in `origin/stable-33.0` existent. Reines totes Fork-Artefakt ohne Upstream-Pendant.

**Status:** aktiv

## 2026-08-17 — Grundsatz: toter Code bleibt erhalten, solange stable-33.0 ihn noch führt (SES-576)

**Kontext:** `AuthenticationDialog`, `PasswordInputDialog`, `TrayWindowHeader.qml`, `CurrentAccountHeaderButton.qml` u. a. wirken im Fork ungenutzt (naheliegend: als Cleanup entfernen). Anders als bei `TrayWindowHeaderBar.qml` (s.o.) führt `origin/stable-33.0` diese Dateien aber weiterhin aktiv.

**Entscheidung:** Kein Löschversuch. `stable-merge-check`-Skill um eine Prüfung erweitert: vor jeder Löschempfehlung per `git show origin/<branch>:<file>` prüfen, ob stable die Datei noch führt. Falls ja: nur als "im Fork ungenutzt, bewusst erhalten für Merge-Kompatibilität" dokumentieren, nicht löschen.

**Verworfene Alternative:** Sofortige Entfernung aus Code-Hygiene-Gründen — verworfen, vergrößert den Diff zu stable statt ihn zu verkleinern. Boris: "ich möchte hier keinen weiteren diff zum stable-33.0 riskieren" / "Nähe zu stable-33.0 schlägt Code-Hygiene." (Gleiches Prinzip wie beim Verbindungseinstellungen-Panel-Eintrag oben, hier auf tote Dateien statt ein UI-Panel angewendet — siehe auch [[feedback_removal-safety-criteria]].)

**Status:** aktiv

## 2026-08-17 — User-Status-Feature (Set status/Status message/Indikator) als bewusste Abweichung dokumentiert (SES-50)

**Kontext:** Beim `shadow-component-watch`-Review stellte sich heraus, dass in `UserLine.qml`/`TrayWindowAccountMenu.qml` nicht nur der Online-Status-Indikator (bekannter `SES-50`-Kommentar), sondern auch beide Menüeinträge ("Set status", "Status message") komplett unverdrahtet sind — kein UI-Weg mehr, den Status zu setzen.

**Entscheidung:** Von Boris bestätigt: beabsichtigt, da das Produkt keine Collaboration-Features hat und Status daher keinen Sinn ergibt. Gesamter Feature-Komplex (Indikator + beide Menüeinträge) als eine zusammengehörige, bewusste Abweichung in `registry.md` dokumentiert, damit künftige Skill-Läufe das nicht erneut als offenen Punkt aufwerfen.

**Status:** aktiv

## 2026-08-17 — Kontrast-Fix für Account-Icon im Settings-Dialog: avatar-spezifisch statt generischer stable-Mechanismus (SES-576)

**Kontext:** Account-Button in `settingsdialog.cpp`s Toolbar hatte zu wenig Kontrast — sowohl das SVG-Fallback-Glyph (Marken-Navy) als auch das nachgeladene Server-Avatar/Initialen-Bild.

**Entscheidung:** Avatar-spezifischer Fix in `settingsdialog.cpp` (Kreis/Rand in `palette(WindowText)`), statt den generischen, für alle Icons geltenden Mechanismus aus stable-33.0 zu übernehmen. Committed als `ef168b21d`.

**Verworfene Alternative:** Invertierungs-Logik aus stable-33.0s `Theme::createColorAwareIcon` (`src/libsync/theme.cpp`) wiederherstellen. Verworfen: unsere Marken-SVGs sind Navy statt Schwarz/Weiß, RGB-Invertierung ergibt nur einen unbefriedigenden Khakiton statt Kontrast. Von Boris nach Test verworfen — festgehalten, damit dieser Weg nicht erneut versucht wird.

**Offener Punkt:** `Theme::createColorAwareIcon` ignoriert `palette` fork-weit komplett (nicht nur fürs Avatar-Icon) — relevant, falls an anderer Stelle ebenfalls über mangelnden Icon-Kontrast diskutiert wird.

**Status:** aktiv

## 2026-08-18 — Unified Search im Tray wieder aktiviert (SES-579)

**Kontext:** Die Unified-Search-Leiste (`UnifiedSearchInputContainer.qml`) im Tray war seit `SES-589` per `visible: false` ausgeblendet (Commit `f545c860a`, "removed searchbar") — laut Commit-Message zurückgestellt, weil sie damals kein direktes `develop_stable-4.0`-Element war.

**Entscheidung:** Sichtbarkeit wiederhergestellt und ins Whitelabel-Design integriert: Such-/Ergebnis-Hover-Styling an fork-eigene Farben/Komponenten angeglichen (Hintergrund/Rand/Text wie `ShareeSearchField`, `Style.sesHover` statt `palette.highlight`, einheitliche Eingabefeldhöhe). Commit `26fe47ea7`. Die zugehörigen C++-Modelle (`UnifiedSearchResultsListModel` etc.) waren die ganze Zeit unverändert im Baum vorhanden und sind jetzt wieder aktiv genutzt statt totem Code.

**Status:** aktiv

## 2026-08-18 — Chevron im Account-Umschalter: generisches Icon statt Marken-SVG (SES-578)

*Rekonstruiert am 2026-08-19 nach versehentlichem Datenverlust (Branch-Wechsel hat den ursprünglichen, noch nicht committeten Eintrag überschrieben) — aus dem Kontext-Fragment dieses Gesprächs und der Commit-Message von `9647f1174` zusammengesetzt, nicht das Original-Wording.*

**Kontext:** Im Zuge der Dark-Mode-Unterstützung für den Tray fiel auf, dass der Chevron-Pfeil im Account-Umschalter (`TrayWindowAccountMenu.qml`) im Dark Mode nicht die Farbe wechselt. Ursache: `source: Style.sesChevron` lädt die markenspezifische Asset-Datei `ses-chevron.svg` (`WLTheme.chevronIcon()`) direkt — deren Farbe ist fest ins SVG kodiert. Der `image://svgimage-custom-color/`-Provider, der Icons dynamisch nach `Style.sesTrayFontColor` einfärbt, kann nur Dateien im generischen `:/client/theme/`-Verzeichnis finden, keine markenspezifischen `ses/`-Assets.

**Entscheidung:** Auf generisches `caret-down.svg` über den `svgimage-custom-color`-Provider umgestellt, getönt mit `Style.sesTrayFontColor` — gleiches Muster wie `TrayFoldersMenuButton.qml`/`CurrentAccountHeaderButton.qml`. Commit `9647f1174`.

**Offener Punkt:** Icon-Form weicht leicht vom bisherigen Marken-Chevron ab — ggf. später durch einen passenden Marken-Asset von Design ersetzen.

**Status:** aktiv
