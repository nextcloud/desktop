# Farbschema-Landkarte

Wird vom [color-scheme-map](SKILL.md)-Skill Komponente für Komponente befüllt — kein Vollscan, sondern nur die Komponenten, die tatsächlich geprüft wurden.

**Hinweis (2026-08-21):** `StratoTheme::dialogBackgroundColor()` (stratotheme.h:17-19) war zwischenzeitlich im Light Mode auf `#FAFAFA` geändert, ist jetzt aber bewusst wieder auf `#F7F7F9` zurückgestellt — damit teilen sich Settings-Seiten und Tray-`MainWindow` (`WLTheme.trayBackgroundColor()`, stratotheme.h:33-34, ebenfalls `#F7F7F9`/`#1F2024`) exakt denselben Hintergrund in beiden Modi. Alle Abschnitte dieser Karte, die `dialogBackgroundColor()` referenzieren, sind mit diesem Wert wieder konsistent.

## Tray (`src/gui/tray/*.qml` + `src/gui/SesComponents/SesTrayHeader.qml`)

*Zuletzt geprüfter Commit: `9647f1174` (2026-08-18)*

Der Tray war laut Doku die Referenzimplementierung für SES-578 — entsprechend ist die Grundstruktur solide: eine kleine Menge zentraler `Style.*`-Properties (aus `theme/Style/Style.qml`, letztlich `WLTheme`/`Theme` gespeist) wird von praktisch allen ~35 QML-Dateien wiederverwendet, statt dass jede Datei eigene Werte hätte.

### Gemeinsame Style-Properties (in fast allen Komponenten verwendet)

| Property | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|
| `Style.sesBackgroundColor` | `#F7F7F9` | `#1F2024` | `WLTheme.trayBackgroundColor()`, stratotheme.h:33 | ✅ theme-aware |
| `Style.sesBorderColor` | `#8493B3` | `#454C5E` | `WLTheme.trayBorderColor()`, stratotheme.h:25 | ✅ theme-aware |
| `Style.sesTrayFontColor` | `#2F2F70` | `#C9CBEF` | `WLTheme.trayFontColor()`, stratotheme.h:21 | ✅ theme-aware |
| `Style.sesMenuBorder` | `#2E4360` | `#5B7699` | inline `Theme.darkMode ? ... : ...`, Style.qml:273 | ✅ theme-aware |
| `Style.sesHover` | `#F2F5F8` | `#2D3138` | inline Ternary, Style.qml:263 | ✅ theme-aware |
| `Style.sesSelectedColor` | `#F4F7FA` | `#333844` | inline Ternary, Style.qml:266 | ✅ theme-aware |
| `Style.sesButtonPressed` | `#D6D6E4` | `#3A3B52` | `WLTheme.toolButtonPressedColor()`, stratotheme.h:128 | ✅ theme-aware |
| `Style.sesAccountMenuHover` | `#EDEEF3` | `#282A36` | `WLTheme.toolButtonHoveredColor()`, stratotheme.h:124 | ✅ theme-aware |
| `Style.sesIconDarkColor` | `#2F2F70` | `#C9CBEF` | `WLTheme.iconDarkColor()`, stratotheme.h:144 | ✅ theme-aware |
| `Style.sesIconColor` | `#2f2f70` | `#C9CBEF` | `WLTheme.buttonIconColor()`, stratotheme.h:108 | ✅ theme-aware |
| `Style.sesActionHover` | `#eeeff9` | `#2A2B3D` | `WLTheme.buttonHoveredColor()`, stratotheme.h:116 | ✅ theme-aware |
| `Style.sesActionPressed` | `#D6D6E4` | `#3A3B52` | `WLTheme.buttonPressedColor()`, stratotheme.h:120 | ✅ theme-aware |
| `Style.sesWhite` | `#FFFFFF` | = `sesBackgroundColor` | inline Ternary, Style.qml:260 | ✅ theme-aware |
| `Style.sesPillButtonPrimaryBackgroundColor` | `#272CB2` | `#5B60D6` | `WLTheme.pillButtonPrimaryColor()`, stratotheme.h:92 | ✅ theme-aware |
| `Style.sesPillButtonSecondaryBackgroundColor` / `Style.buttonBackgroundColor` | `#E4E4ED` | `#2E2F3D` | `WLTheme.pillButtonSecondaryColor()`, stratotheme.h:96 | ✅ theme-aware |
| `Style.sesPillButtonBorderColor` | `#FFFFFF` | `#FFFFFF` (identisch) | `WLTheme.pillButtonBorderColor()`, stratotheme.h:100 — Kommentar in basetheme.h:382/stratotheme.h:72 bestätigt "stays white in both modes" | ℹ️ bewusst fix |
| `Style.ncTextColor` / `Style.ncSecondaryTextColor` | = `sesTrayFontColor` | = `sesTrayFontColor` | Alias, Style.qml:16/18 | ✅ theme-aware |
| `Style.backgroundColor` | `#FFFFFF` | `#1E2126` | inline Ternary, Style.qml:22 | ✅ theme-aware |
| `Style.errorBoxBackgroundColor` (MainWindow.qml:305, Sync-Warnbanner) | `Qt.rgba(0.89,0.18,0.18,1)` fix | identisch | Style.qml:33, per Konvention bewusst fix (wie `ErrorBox.qml`/`SesErrorBox.qml`) | ℹ️ bewusst fix |
| `Style.ncTextBrightColor` (EncryptionTokenDiscoveryDialog.qml:32, `brightText`-Palette-Rolle) | `"white"` fix | identisch | Style.qml:17, keine Begründung im Code | ⚠️ Bruch: kein Dark-Wert |
| `Style.currentUserHeaderColor`/`Style.accentColor` (Fallback `ncBlue`, nur wenn noch kein Account eingerichtet) | `APPLICATION_WIZARD_HEADER_BACKGROUND_COLOR` (Compile-Konstante) | identisch | `Theme::wizardHeaderBackgroundColor()`, theme.cpp:784 — `Q_PROPERTY(... CONSTANT)`, keine `themedColor()`-Anbindung | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert (Edge-Case, nur vor der ersten Account-Einrichtung sichtbar) |
| `Style.adjustedCurrentUserHeaderColor` | `Qt.darker(currentUserHeaderColor, 1.5)` | `Qt.lighter(currentUserHeaderColor, 2)` | inline Ternary, Style.qml:29 | ✅ theme-aware (Formel reagiert, Basiswert ist aber meist der dynamische Server-Header, s.o.) |

### Ambient `palette.*`-Bindungen

`MainWindow.qml:40-41` setzt am Wurzel-`ApplicationWindow` `palette.base: Style.sesBackgroundColor` und `palette.windowText: Style.sesTrayFontColor` — dank QMLs Context-Property-Vererbung erben **alle** Kind-Items diese beiden Rollen automatisch, ohne dass jede Datei sie einzeln setzen muss (anders als auf der QWidget-Seite, wo genau diese Weitergabe in `AccountSettings`/`SettingsDialog` zuvor fehlte). `palette.base`/`palette.windowText` gelten daher überall im Baum als ✅ theme-aware, auch wenn einzelne Dateien nur `palette.base`/`palette.windowText` referenzieren, ohne es selbst zu setzen (z. B. `NCBusyIndicator.qml:13`, `ListItemLineAndSubline.qml:23`, `TrayWindowHeader.qml:80/90/123`, `TrayFolderListItem.qml:15`, `UnifiedSearchPlaceholderView.qml:23`, `UnifiedSearchResultNothingFound.qml:23`).

Andere Palette-Rollen (`palette.dark`, `palette.light`, `palette.highlight`, `palette.mid`, `palette.brightText`, `palette.alternateBase`) werden **nicht** in `MainWindow.qml` überschrieben — sie referenzieren die native Qt-Palette direkt (z. B. `NCProgressBar.qml:21/34`, `ListItemLineAndSubline.qml:24`, `TalkReplyTextField.qml:27/29/45/46`, `UnifiedSearchResultItem.qml:29/30`, `UnifiedSearchResultFetchMoreTrigger.qml:20`, `EditFileLocallyLoadingDialog.qml:17/41/43/64`, `EncryptionTokenDiscoveryDialog.qml:59/80`, `MainWindow.qml:649/662/686/699/713/843`). Status **ℹ️ ambient** — verlässt sich auf Qt's natives `QStyleHints::colorScheme()`-Palette-Tracking, das laut früherer Analyse dieser Sitzung auf Windows 11 grundsätzlich funktioniert.

### Komponenten-spezifische Brüche

| Fundstelle | Wert | Status | Notiz |
|---|---|---|---|
| `MainWindow.qml:409` (`border.color`, KI-Assistent-Reset-Bestätigungsdialog) | `"#808080"` fix | ⚠️ Bruch: kein Dark-Wert | Einziger Dialog-Rahmen im Tray, der nicht über `Style.sesBorderColor`/`Style.sesMenuBorder` läuft — wirkt im Dark Mode vermutlich zu hell. |
| `MainWindow.qml:420` (`DropShadow.color`) | `"#80000000"` fix | ℹ️ bewusst fix | Schlagschatten bleiben konventionell schwarz/halbtransparent unabhängig vom Theme — unkritisch. |
| `CallNotificationDialog.qml:132` (`darkenerRect`, Hintergrundbild-Abdunklung) | `"black"`, `opacity: 0.4` fix | ℹ️ bewusst fix | Abdunklung eines Hintergrund-**Fotos** (Anruferbild), kein UI-Chrome — theme-unabhängig sinnvoll. |
| `PrimaryPillButton.qml:26` (Button-Text) | `"white"` fix | ⚠️ Bruch: kein Dark-Wert | Kein Kommentar; praktisch vermutlich unkritisch, da der Pill-Hintergrund (`sesPillButtonPrimaryBackgroundColor`, `#272CB2`/`#5B60D6`) in beiden Modi kräftig genug für weißen Text ist — aber nicht wie z. B. `buttonstyle.h`/`titleColor()` bewusst dokumentiert. |
| `CurrentAccountHeaderButton.qml`, `TrayWindowHeader.qml` | — | — nicht bewertet | Laut `tray/CLAUDE.md` Legacy/unbenutzt (durch `SesTrayHeader.qml`/`TrayWindowAccountMenu.qml` ersetzt, nicht im aktiven Render-Baum) — bewusst von dieser Prüfung ausgeschlossen. |

### Ausgeschlossen (keine echten Farbwerte)

Mehrere `color: "white"`-Treffer sind reine `OpacityMask`-Maskenformen (`visible: false`, z. B. `ActivityItemContent.qml:69`, `CallNotificationDialog.qml:114/171`, `UnifiedSearchResultItemSkeleton.qml:77/120/156`, `UnifiedSearchResultItemSkeletonContainer.qml:56`) — sie definieren nur die Maskenform für ein rundes/geformtes Bild, werden nie selbst gerendert, und sind daher kein Dark-Mode-Thema.

**Brüche (Zusammenfassung):**
1. `Style.ncTextBrightColor` fix `"white"` (Style.qml:17) — betrifft `brightText`-Palette-Rolle in `EncryptionTokenDiscoveryDialog.qml`.
2. `Theme::wizardHeaderBackgroundColor()` (`Q_PROPERTY CONSTANT`, theme.cpp:784) — Fallback-Farbe für `ncBlue`/`accentColor`/`currentUserHeaderColor`, nur relevant bevor ein Account eingerichtet ist.
3. `MainWindow.qml:409` — Dialograhmen `#808080`, einziger nicht themefähiger Rahmen im Tray.
4. `PrimaryPillButton.qml:26` — weißer Button-Text ohne Dark-Variante/Begründung, praktisch aber unkritisch.

## AccountSettings (`src/gui/accountsettings.cpp/.h/.ui`)

*Zuletzt geprüfter Commit: working tree auf `0b9c2b950` (2026-08-21, noch uncommitted)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.dialogBackgroundColor()` | accountsettings.cpp:605, 998, 1178, 1917, 1932, 1936 | `#F7F7F9` (identisch mit Tray `trayBackgroundColor()`, siehe Hinweis oben) | `#1F2024` | `StratoTheme::dialogBackgroundColor()`, stratotheme.h:17-19 (überschreibt `themedColor("#FFFFFF","#1E2126")` in basetheme.h:441) | ✅ theme-aware |
| `WLTheme.titleColor()` | accountsettings.cpp:997, 1177, 1431, 1925, 1928 | `#000000` | `#D6E4F5` | `themedColor()` in basetheme.h:318 (keine Strato-Override) | ✅ theme-aware |
| `WLTheme.settingsLinkColor()` | accountsettings.cpp:1342, 1367, 1920 | `#272CB2` | `#5B60D6` | `StratoTheme::settingsLinkColor()`, stratotheme.h:37 | ✅ theme-aware |
| `WLTheme.trayBackgroundColor()` | accountsettings.cpp:736 | `#F7F7F9` | `#1F2024` | `StratoTheme::trayBackgroundColor()`, stratotheme.h:33 | ✅ theme-aware |
| `WLTheme.menuBorderColor()` | accountsettings.cpp:737 | `#2E4360` | `#5B7699` | `themedColor()` in basetheme.h:449 (keine Strato-Override) | ✅ theme-aware |
| `WLTheme.menuTextColor()` | accountsettings.cpp:738 | `#29294d` | `#C9CBEF` | `StratoTheme::menuTextColor()`, stratotheme.h:132 | ✅ theme-aware |
| `WLTheme.menuSelectedItemColor()` | accountsettings.cpp:740 | `#D6D6E4` | `#282A36` | `StratoTheme::menuSelectedItemColor()`, stratotheme.h:136 | ✅ theme-aware |
| `WLTheme.menuPressedItemColor()` | accountsettings.cpp:741 | `#5A6782` | `#3A3B52` | `StratoTheme::menuPressedItemColor()`, stratotheme.h:148 | ✅ theme-aware |
| `WLTheme.menuPressedTextColor()` | accountsettings.cpp:739 | `#FFFFFF` | `#FFFFFF` (identisch) | `StratoTheme::menuPressedTextColor()`, stratotheme.h:140 — **fixer Rückgabewert, kein `themedColor()`** | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert — Regression ggü. `basetheme.h:457` (`themedColor("#001B41","#D6E4F5")`), das Strato hier mit einem festen `"#FFFFFF"` überschreibt |
| Fixe Fehler-Box: `color:#ffffff; background-color:#bb4d4d;` + Link `#c1c8e6` | accountsettings.cpp:1337-1339, 1350, 1362-1364, 1375 (`showConnectionLabel`, Fehlerfall) | — | — | Hartcodiert, per Kommentar (Z. 1333) explizit als "fixed, self-contained contrast pair - deliberately not themed" begründet | ℹ️ bewusst fix |
| `color: black; background: lightgrey;` | accountsettings.cpp:1280 (E2E-Mnemonic-Dialog, `ui.lineEdit`) | `black`/`lightgrey` (fix) | identisch | Hartcodiert, keine Begründung im Code | ⚠️ Bruch: kein Dark-Wert |
| `color: red` | accountsettings.ui:352 (`selectiveSyncNotification`) | `red` (fix) | identisch | Im `.ui`-Formular hartcodiert, in `customizeStyle()`/sonst im `.cpp` nie überschrieben | ⚠️ Bruch: kein Dark-Wert |
| `palette().highlight().color()` → `toolTipStyle` | accountsettings.cpp:1930-1932 | — | — | `QPalette`-Rolle, aber Ergebnis wird nie per `setStyleSheet()` angewendet | ℹ️ ambient — vermutlich **dead code** (außerhalb des Farbschema-Scopes, aber hier notiert: `color`/`toolTipStyle` werden berechnet und nie verwendet) |

**Brüche:**
1. `StratoTheme::menuPressedTextColor()` (stratotheme.h:140) gibt fest `"#FFFFFF"` zurück statt `themedColor(light, dark)` — im Kontextmenü (accountsettings.cpp:739) betrifft das den Text von `QMenu::item:pressed`. Da der zugehörige Hintergrund (`menuPressedItemColor()`) im Light-Mode `#5A6782` ist, bleibt es noch lesbar, ist aber nicht bewusst getestet/geplant — eher eine übersehene Vereinfachung.
2. `ui.lineEdit` im E2E-Mnemonic-Dialog (accountsettings.cpp:1280) ist fest hellgrau/schwarz, unabhängig vom OS-Theme — fällt in einem sonst abgedunkelten Dialog optisch heraus.
3. `selectiveSyncNotification` (accountsettings.ui:352) hat fest `color: red` ohne jede Theme-Anbindung — funktional meist noch lesbar (Rot auf Hell/Dunkel), aber nicht über `themedColor()` geführt wie der Rest der Seite.

**Hinweis (2026-08-21):** `WLTheme.panelBackgroundColor()` für die Card-Panels `accountStatusPanel`/`fileProviderPanel`/`syncFoldersPanel`/`connectionSettingsPanel` (aus Commit `b9e530504`) wurde wieder entfernt — Referenzscreenshot des Nutzers (stable-33.0-Optik) zeigt einen durchgängig einheitlichen Settings-Hintergrund ohne abgesetzte Panels. Getter + Q_PROPERTY komplett aus `basetheme.h` entfernt (ungenutzt). Der Dateibaum (`_folderList`) in dieser Komponente ist von dieser Betrachtung bewusst ausgenommen — wird separat behandelt. Durch das Entfernen von zwei Blöcken in `basetheme.h` (Q_PROPERTY-Zeile + Kommentar/Methode) können sich Zeilenangaben zu `basetheme.h` in anderen, noch nicht seit diesem Commit neu geprüften Abschnitten dieser Karte um 1-7 Zeilen verschoben haben — bei Bedarf beim nächsten Check der jeweiligen Komponente korrigieren.

## Dialog-Familie: caseclashfilenamedialog / conflictdialog / foldercreationdialog / invalidfilenamedialog / legacyaccountselectiondialog

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

Alle fünf Dialoge teilen dasselbe Whitelabel-Muster (Commit `45446a124` "roll out dialogBackgroundColor()/titleColor() dark-mode reuse across dialogs", Basis `bb543a166`): `#include "buttonstyle.h"`/`"whitelabeltheme.h"`, `buttonStyle`-Property auf den Buttons, eine `customizeStyle()`-Methode, die per `setStyleSheet()`/`setPalette()` dieselben vier Getter kombiniert. **Alle fünf rufen `customizeStyle()` nur einmal im Konstruktor auf, keiner hängt es an `changeEvent()`/`paletteChanged`** — ein Theme-Wechsel bei bereits offenem Dialog würde ihn nicht neu einfärben (übergreifende Beobachtung, kein Einzel-Bruch).

| Property | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|
| `WLTheme.dialogBackgroundColor()` | `#F7F7F9` | `#1F2024` | `StratoTheme::dialogBackgroundColor()`, stratotheme.h:17-18 | ✅ theme-aware |
| `WLTheme.titleColor()` | `#000000` | `#D6E4F5` | `themedColor()` basetheme.h:319-320 (keine Strato-Override) | ✅ theme-aware |
| `WLTheme.folderWizardPathColor()` | `#97A3B4` | `#A8B4C6` | `themedColor()` basetheme.h:327-328 (keine Strato-Override) | ✅ theme-aware |
| `WLTheme.menuBorderColor()` | `#2E4360` | `#5B7699` | `themedColor()` basetheme.h:457-458 (keine Strato-Override) | ✅ theme-aware |

Fundstellen: `caseclashfilenamedialog.cpp:296,297,302,311,316,317` · `conflictdialog.cpp:186,187,192` (+ zwei `Q_OS_MAC`-only QCheckBox-Styles `titleColor()` bei 202/206) · `foldercreationdialog.cpp:106,118,123,124` (`dialogBackgroundColor()` hier direkt per `setPalette(QPalette(QPalette::Window, …))`) · `invalidfilenamedialog.cpp:331,336,345,350,351` · `legacyaccountselectiondialog.cpp:25,67,72,74`.

**Brüche:** keine — alle Treffer ✅ theme-aware.

**Hinweis (kein Farb-, sondern Merge-Risiko-Fund):** `foldercreationdialog.cpp` hat neben dem Styling auch echte Logik verändert (`accept()`-Guard `QDir(fullPath).exists()` entfernt, `ui->labelErrorMessage`→`ui->errorSnackbar` umbenannt) — siehe [[project_ionos-build-dead-branch-pattern]]-artige Kategorie "unauffällige Nebenänderung im selben Commit", separat in DRIFT_MAP.md als **hoch** eingestuft. Lohnt eine eigene Prüfung, ob der entfernte Guard beabsichtigt war.

## GeneralSettings (`src/gui/generalsettings.cpp`)

*Zuletzt geprüfter Commit: working tree auf `0b9c2b950` (2026-08-21, noch uncommitted)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.dialogBackgroundColor()` | generalsettings.cpp:869 | `#F7F7F9` (identisch mit Tray `trayBackgroundColor()`, siehe Hinweis oben) | `#1F2024` | `StratoTheme::dialogBackgroundColor()`, stratotheme.h:17-19 | ✅ theme-aware |
| `WLTheme.titleColor()` | generalsettings.cpp:872,876,880,884,887,901,904,907 | `#000000` | `#D6E4F5` | `themedColor()` basetheme.h:320 | ✅ theme-aware |
| `WLTheme.folderWizardSubtitleColor()` | generalsettings.cpp:895,898 | `#104996` | `#5FA8E0` | `themedColor()` basetheme.h:324 | ✅ theme-aware |

**Brüche:** keine. (Hinweis: `WLTheme.panelBackgroundColor()` für QGroupBox-Card-Panels wurde am 2026-08-20 in Commit `b9e530504` eingeführt und am 2026-08-21 wieder entfernt — Referenzscreenshot des Nutzers zeigt für die Settings-Seite einen durchgängig einheitlichen Hintergrund ohne abgesetzte Panels, analog zur stable-33.0-Optik. Getter + Q_PROPERTY komplett aus `basetheme.h` entfernt, da sonst ungenutzt.)

## IgnoreListTableWidget (`src/gui/ignorelisttablewidget.cpp`)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.dialogBackgroundColor()` | ignorelisttablewidget.cpp:210,232,267,296 | `#F7F7F9` | `#1F2024` | stratotheme.h:18 | ✅ theme-aware |
| `WLTheme.titleColor()` | ignorelisttablewidget.cpp:211,217,226,233,238,265,278 | `#000000` | `#D6E4F5` | basetheme.h:320 | ✅ theme-aware |
| `WLTheme.folderWizardPathColor()` | ignorelisttablewidget.cpp:287 | `#97A3B4` | `#A8B4C6` | basetheme.h:328 | ✅ theme-aware |
| `WLTheme.menuBorderColor()` | ignorelisttablewidget.cpp:292 | `#2E4360` | `#5B7699` | basetheme.h:458 | ✅ theme-aware |

Kommentar bei Zeile 296 begründet bewusst, dass das Eingabefeld sich nur über den Rahmen absetzt statt über eine eigene Füllfarbe.

**Brüche:** keine.

## Systray-Kontextmenü (`src/gui/systray.cpp`)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.trayBackgroundColor()` | systray.cpp:237 | `#F7F7F9` | `#1F2024` | stratotheme.h:34 | ✅ theme-aware |
| `WLTheme.menuBorderColor()` | systray.cpp:238 | `#2E4360` | `#5B7699` | basetheme.h:458 | ✅ theme-aware |
| `WLTheme.menuTextColor()` | systray.cpp:242 | `#29294d` | `#C9CBEF` | stratotheme.h:133 | ✅ theme-aware |
| `WLTheme.menuSelectedItemColor()` | systray.cpp:243 | `#D6D6E4` | `#282A36` | stratotheme.h:137 | ✅ theme-aware |
| `WLTheme.menuPressedItemColor()` | systray.cpp:244 | `#5A6782` | `#3A3B52` | stratotheme.h:149 | ✅ theme-aware |
| `WLTheme.menuPressedTextColor()` | systray.cpp:245 | `#FFFFFF` | `#FFFFFF` (identisch) | `StratoTheme::menuPressedTextColor()`, stratotheme.h:141 — fixer Rückgabewert, kein `themedColor()` | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert |

**Brüche:** `menuPressedTextColor()` (stratotheme.h:141) — derselbe bereits unter AccountSettings dokumentierte Bruch, hier im Tray-Kontextmenü bestätigt (zweite Verwendungsstelle desselben Getters).

## ButtonStyle (`src/gui/buttonstyle.h`, fork-eigen)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

Reine Weiterleitung an `WLTheme`-Getter (`PrimaryButtonStyle`/`SecondaryButtonStyle`/`MoreOptionsButtonStyle`), keine eigenen Hex-Werte.

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `buttonPrimaryColor()` | 75,80,119 | `#272CB2` | `#5B60D6` | stratotheme.h:50 | ✅ theme-aware |
| `buttonPrimaryHoverColor()` | 86,91 | `#2944CC` | `#2944CC` (identisch) | `StratoTheme`-Override stratotheme.h:55, eigener TODO-Kommentar "no established Strato dark counterpart" | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert (offen bekannt) |
| `buttonPrimaryPressedColor()` | 97,102 | `#272CB2` | `#5B60D6` | stratotheme.h:59 | ✅ theme-aware |
| `buttonDisabledColor()` | 108,113,202,207,297,302 | `#EDEEF3` | `#282A36` | stratotheme.h:89 | ✅ theme-aware |
| `buttonPrimaryFocusedBorderColor()` | 124 | `#CDD5E3` | `#FFFFFF` | stratotheme.h:63 | ✅ theme-aware |
| `buttonDisabledFontColor()` | 130,224,319 | `#BDBDBD` | `#5A5D63` | basetheme.h:415 | ✅ theme-aware |
| `white()` | 135,141,146,213,235,240,308 | `#FFFFFF` | identisch | basetheme.h:435, fixer Wert | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert |
| `buttonSecondaryColor()` | 169 | `#F7F7F9` | `#2E2F3D` | stratotheme.h:68 | ✅ theme-aware |
| `buttonSecondaryBorderColor()` | 174,185,196 | `#CDD5E3` | `#FFFFFF` | stratotheme.h:73 | ✅ theme-aware |
| `buttonSecondaryHoverColor()` | 180 | `#EDEEF3` | `#282A36` | stratotheme.h:77 | ✅ theme-aware |
| `buttonSecondaryPressedColor()` | 191 | `#D6D6E4` | `#3A3B52` | stratotheme.h:81 | ✅ theme-aware |
| `buttonSecondaryFocusedBorderColor()` | 218 | `#8493B3` | `#454C5E` | stratotheme.h:85 | ✅ theme-aware |
| `titleColor()` | 229 | `#000000` | `#D6E4F5` | basetheme.h:320 | ✅ theme-aware |
| `dialogBackgroundColor()` | 264,269 | `#F7F7F9` | `#1F2024` | stratotheme.h:18 | ✅ theme-aware |
| `buttonHoveredColor()` | 275,280 | `#eeeff9` | `#2A2B3D` | stratotheme.h:117 | ✅ theme-aware |
| `buttonPressedColor()` | 286,291 | `#D6D6E4` | `#3A3B52` | stratotheme.h:121 | ✅ theme-aware |
| `black()` | 313,324 | `#000000` | identisch | basetheme.h:439, fixer Wert | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert — **Zeile 324 (`MoreOptionsButtonStyle::buttonFontColor()`) sitzt auf `dialogBackgroundColor()` dunkel `#1F2024`, also potenziell fast-schwarzer Text auf fast-schwarzem Grund** |
| `buttonIconColor()` | 330 | `#2f2f70` | `#C9CBEF` | stratotheme.h:109 | ✅ theme-aware |
| `buttonIconHoverColor()` | 335 | `#2f2f70` | `#C9CBEF` | stratotheme.h:113 | ✅ theme-aware |

**Brüche:**
1. `buttonPrimaryHoverColor()` (stratotheme.h:55) — kein Dark-Wert, laut eigenem TODO noch offen (Design-Input fehlt).
2. `white()`/`black()` (basetheme.h:435/439) — fixe Werte, betreffen v. a. `MoreOptionsButtonStyle::buttonFontColor()` (Zeile 324): **schwarzer Text auf `dialogBackgroundColor()` im Dark Mode (`#1F2024`) — praktisch vermutlich unlesbar, konkretester Kontrast-Fund dieser Analyse-Runde.**

## SettingsDialog (`src/gui/settingsdialog.cpp`)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.dialogBackgroundColor()` | 532,546-547,553-555,558-561 (nur `IONOS_BUILD`) | `#F7F7F9` (identisch mit Tray `trayBackgroundColor()`, siehe Hinweis oben) | `#1F2024` | stratotheme.h:17-19 | ✅ theme-aware |
| `WLTheme.menuBorderColor()` (rechter Rahmen `#settings_navigation_scroll`, neu am 2026-08-21) | 559-561 | `#2E4360` (identisch mit Tray `Style.sesMenuBorder`) | `#5B7699` | basetheme.h:457-458 (keine Strato-Override) | ✅ theme-aware |
| `WLTheme.toolButtonHoveredColor()` | 533 | `#EDEEF3` | `#282A36` | stratotheme.h:124-125 | ✅ theme-aware |
| `WLTheme.toolButtonPressedColor()` | 534 | `#D6D6E4` | `#3A3B52` | stratotheme.h:128-129 | ✅ theme-aware |
| `WLTheme.menuSelectedItemColor()` | 535 | `#D6D6E4` | `#282A36` | stratotheme.h:136-137 | ✅ theme-aware |
| `WLTheme.buttonSecondaryBorderColor()` | 538 | `#CDD5E3` | `#FFFFFF` | stratotheme.h:71-73 | ✅ theme-aware |
| `WLTheme.titleColor()` (als `highlightTextColor`) | 539 | `#000000` | `#D6E4F5` | basetheme.h:319-320 | ✅ theme-aware |
| `WLTheme.menuTextColor()` | 544 | `#29294d` | `#C9CBEF` | stratotheme.h:132-133 | ✅ theme-aware |
| `palette(highlight)`/`palette(highlighted-text)` (`TOOLBAR_CSS`, `IONOS_BUILD`) | 118 | OS-Theme | OS-Theme | ambient | ℹ️ ambient — verifiziert reaktiv über `changeEvent()`→`customizeStyle()` (306-312) |
| `palette(window)`/`palette(alternate-base/light)` (`BACKGROUND_PALETTE`, Nicht-`IONOS_BUILD`-Zweig) | 583-596 | OS-Theme | OS-Theme | ambient | ℹ️ ambient — verifiziert reaktiv |
| `palette().color(QPalette::WindowText/Window)` (Avatar-Ring/Glyph) | 468,684,688 | OS-Theme | OS-Theme | ambient, mit Begründungskommentar | ℹ️ ambient — verifiziert reaktiv |
| `Theme::createColorAwareIcon(iconPath, palette())` | 560,609,666,697 | OS-Theme | OS-Theme | ambient | ℹ️ ambient — verifiziert reaktiv |

**Brüche:** keine.

## FolderStatusDelegate (`src/gui/folderstatusdelegate.cpp`)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `option.palette` (Zeilentext) | 285,287,334 | OS-Theme | OS-Theme | Qt liefert `option.palette` pro Paint-Aufruf neu | ℹ️ ambient (zwangsläufig aktuell) |
| `WLTheme.warningBorderColor()` | 355 | `#F4BFAB` | `#C98F5E` | basetheme.h:521-522 | ✅ theme-aware |
| `WLTheme.errorBorderColor()` | 358 | `#FF004C` | `#FF6688` | stratotheme.h:152-153 | ✅ theme-aware |
| `WLTheme.infoBorderColor()` | 384 | `#11C7E6` | `#4DD9F0` | basetheme.h:529-530 | ✅ theme-aware |
| `WLTheme.dialogBackgroundColor()` (Fortschrittsleisten-Base) | 425 | `#F7F7F9` | `#1F2024` | stratotheme.h:17-18 | ✅ theme-aware |
| `WLTheme.syncProgressColor()` (Fortschrittsleisten-Highlight) | 426 | `#009850` | identisch | `StratoTheme::syncProgressColor()`, stratotheme.h:45-47 — fixer Rückgabewert, überschreibt `basetheme.h:368-369` (`themedColor("#359ada","#4FB6F0")`) | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert |
| `BaseTheme::tintedFillFromBorder(borderColor)` (Fehler/Warn/Info-Box-Füllung) | 330 | abgeleitet | abgeleitet | Formel auf o.g. Border-Farben, Kommentar 326-329 begründet bewusste Ableitung statt fixer Pastelltöne | ✅ theme-aware |

**Brüche:** `StratoTheme::syncProgressColor()` (stratotheme.h:45-47) fest `#009850` — betrifft die Sync-Fortschrittsleiste; Grün bleibt im Dark Mode vermutlich noch kontrastreich genug, aber nicht bewusst dokumentiert.

## FolderWizard (`src/gui/folderwizard.cpp`)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.dialogBackgroundColor()` | 186,216,256,290,301,722,757,967 | `#F7F7F9` | `#1F2024` | stratotheme.h:17-18 | ✅ theme-aware |
| `WLTheme.titleColor()` | 189,201,289,304,677,686,756,760 | `#000000` | `#D6E4F5` | basetheme.h:319-320 | ✅ theme-aware |
| `WLTheme.folderWizardSubtitleColor()` | 193-196,673,763-766 | `#104996` | `#5FA8E0` | basetheme.h:323-324 | ✅ theme-aware |
| `WLTheme.folderWizardPathColor()` | 210,295 | `#97A3B4` | `#A8B4C6` | basetheme.h:327-328 | ✅ theme-aware |
| `WLTheme.menuBorderColor()` | 215,300 | `#2E4360` | `#5B7699` | basetheme.h:457-458 | ✅ theme-aware |
| `WLTheme.white()` (Button-Text, nur `Q_OS_MAC`) | 223,310 | `#FFFFFF` | identisch | basetheme.h:434-436, fixer Wert | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert |

**Brüche:** `white()` (basetheme.h:434-436) — nur macOS-Zweig, vermutlich unkritisch (analog zum bekannten `menuPressedTextColor()`-Muster), aber nicht als bewusste Dark-Mode-Ausnahme kommentiert.

## OwncloudGui (`src/gui/owncloudgui.cpp`)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

**Keine Farbtreffer.** Alle `WLTheme.*`-Aufrufe (`syncOfflineIcon`, `syncSyncingIcon`, `syncPausedIcon`, `syncSuccessIcon`, `syncWarningIcon`) liefern Icon-Dateipfade, keine Farbwerte.

## SelectiveSyncDialog (`src/gui/selectivesyncdialog.cpp`)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.dialogBackgroundColor()` | 81,105,122,138,582,598 | `#F7F7F9` | `#1F2024` | stratotheme.h:17-18 | ✅ theme-aware |
| `WLTheme.titleColor()` | 80,115,127,134,569 | `#000000` | `#D6E4F5` | basetheme.h:319-320 | ✅ theme-aware |
| `WLTheme.white()` (OK-Button-Text) | 591 | `#FFFFFF` | identisch | basetheme.h:434-436, fixer Wert | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert |
| `palette(dark)` (Header-Trennlinie, nur `Q_OS_MAC`) | 109 | OS-Theme | OS-Theme | ambient, Kommentar 107-108 bewusst analog zum Tray | ℹ️ ambient (bewusst) |

**Brüche:** `white()` (basetheme.h:434-436), gleiches Muster wie `folderwizard.cpp`.

**Hinweis:** `customizeStyle()` wird nur im Konstruktor aufgerufen (Zeile 576), nicht an `changeEvent()` gehängt — anders als `settingsdialog.cpp`. Farbwerte selbst sind theme-aware definiert, werden aber bei einem Theme-Wechsel während der Dialog offen ist ggf. nicht neu angewendet.

## Wizard: DataProtection-Seiten (`src/gui/wizard/dataprotectionpage.cpp`, `dataprotectionsettingspage.cpp`, fork-eigen)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.titleColor()` | dataprotectionpage.cpp:83 · dataprotectionsettingspage.cpp:96,108,121 | `#000000` | `#D6E4F5` | basetheme.h:319-320 | ✅ theme-aware |
| `WLTheme.folderWizardSubtitleColor()` | dataprotectionsettingspage.cpp:76,86 | `#104996` | `#5FA8E0` | basetheme.h:323-324 | ✅ theme-aware |

**Brüche:** keine.

## SesErrorBox.qml / ShareeSearchField.qml (FileDetailsPage-Umfeld)

*Zuletzt geprüfter Commit: `8534f6865` (2026-08-20) — heute im selben Lauf per SES-578 gefixt (Commits `cd9d7535e`, `8534f6865`)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `Style.tintedFill(Style.sesErrorBoxBorder, 0.2)` (Hintergrund) | SesErrorBox.qml:34 | `#FF004C`@20% | `#FF6688`@20% | `Style.tintedFill()` Style.qml:231, neuer gemeinsamer Helfer `BaseTheme::tintedFillFromBorder` | ✅ theme-aware |
| `Style.sesErrorBoxBorder` | SesErrorBox.qml:35 | `#FF004C` | `#FF6688` | `WLTheme.trayErrorBorderColor()`, stratotheme.h:156-157 | ✅ theme-aware |
| `Style.sesErrorBoxText` | SesErrorBox.qml:63 | `#CC0052` | `#FF8FB3` | `WLTheme.trayErrorTextColor()`, stratotheme.h:160-161 | ✅ theme-aware |
| `Style.sesSearchFieldContent` (Placeholder/Icon-Tint) | ShareeSearchField.qml:34,116 | `#97A3B4` | `#B7C1CE` | inline `Theme.darkMode`-Ternary, Style.qml:280 | ✅ theme-aware |
| `Style.sesTrayFontColor` (Text/Cursor) | ShareeSearchField.qml:43,44 | `#2F2F70` | `#C9CBEF` | stratotheme.h:21-22 | ✅ theme-aware |
| `Style.sesBackgroundColor` (Field-/Popup-Background) | ShareeSearchField.qml:93,173 | `#F7F7F9` | `#1F2024` | stratotheme.h:33-34 | ✅ theme-aware |
| `Style.sesMenuBorder` (Field-/Popup-Border) | ShareeSearchField.qml:94,174 | `#2E4360` | `#5B7699` | inline Ternary, Style.qml:279 | ✅ theme-aware |
| `Style.sesHover` (Vorschlagsliste-Highlight) | ShareeSearchField.qml:197 | `#F2F5F8` | `#2D3138` | inline Ternary, Style.qml:269 | ✅ theme-aware |
| `palette.placeholderText` (Busy-Indicator/Clear-Icon-Tint) | ShareeSearchField.qml:132,154 | — | — | ambient, kein eigener Wert | ℹ️ ambient, unverifiziert — weder `FileDetailsPage.qml` noch `ShareDetailsPage.qml` rebinden `placeholderText` |

**Brüche:** keine (beide Dateien wurden in dieser Session bereits korrigiert). Einzige offene Frage: `palette.placeholderText` ist ambient/unverifiziert, praktisch niedrige Priorität.

## UserStatusMessageView.qml

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `Style.ncBlue` (Feldrahmen bei `showBorder`) | 65 | `#0082c9` | identisch | `Theme::wizardHeaderBackgroundColor()`, theme.cpp:784, `Q_PROPERTY CONSTANT` | ⚠️ Bruch: Getter selbst hat keinen Dark-Wert |
| `palette.dark` (Feldrahmen-Fallback, Emoji-Dialog-Border) | 65,96 | — | — | ambient | ℹ️ ambient, unverifiziert — `MainWindow.qml` rebindet nur `base`/`windowText`, `dark` bleibt System-Palette |
| `palette.button` (Feld-Hintergrund) | 69 | — | — | ambient | ℹ️ ambient, unverifiziert |
| `palette.base` (Emoji-Dialog-Hintergrund) | 94 | — | — | ambient | ℹ️ ambient, verifiziert — `MainWindow.qml:40` setzt `palette.base` an der Wurzel |
| `Style.sesTrayFontColor` (ComboBox-Indicator-Icon-Tint, neu) | 192 | `#2F2F70` | `#C9CBEF` | stratotheme.h:21-22 | ✅ theme-aware |

**Brüche:** `Style.ncBlue`/`Theme::wizardHeaderBackgroundColor()` (theme.cpp:784) — derselbe bereits im Tray-Abschnitt dokumentierte Bruch, hier als zweite Verwendungsstelle bestätigt.

## Application (`src/gui/application.cpp`/`.h`)

*Zuletzt geprüfter Commit: `9d3af4e33` (2026-08-20)*

**Keine Farbtreffer.** Die Datei behandelt Style-Auswahl (`sesStyle`-Base-Style, s. Fix von heute) und natives Dark-Titlebar-Chrome (`applyImmersiveDarkMode()`, DWM-Attribut, kein Farbwert) — beides betrifft Dark-Mode-*Verhalten*, aber keine `WLTheme`/Hex-Farbwerte im Sinne dieser Karte.

## SesSnackBar-Header (`src/gui/sessnackbar.h`)

*Zuletzt geprüfter Commit: `2f77361ec` (2026-08-20)*

**Keine Farbtreffer in der Header-Datei** — nur Deklarationen (`updateStyleSheet(QColor)`, `errorStyle()`/`warningStyle()`/`successStyle()`). Die tatsächlichen Farbwerte stehen in `src/gui/sessnackbar.cpp` (fork-only) — dort bislang noch nicht geprüft, offen für eine künftige Erfassung bei Bedarf.

## Ergänzung Tray-QML (AccountMenuItem, ActivityItemContent, TrayWindowAccountMenu, UnifiedSearchInputContainer, UserLine)

*Sanity-Check-Commit: `2f77361ec` (2026-08-20)*

Sanity-Check gegen die bestehende Tray-Sektion (s.o.) durchgeführt — keine neuen Farbmuster gefunden, die nicht bereits über die dort erfassten `Style.*`-Properties abgedeckt sind. `UnifiedSearchInputContainer.qml` hat neu `palette.placeholderText` durch `Style.sesSearchFieldContent`/`Style.sesTrayFontColor` ersetzt (jetzt ✅ theme-aware statt ambient). Keine neuen Brüche.

## Externe Vorgabe: IONOS-Farbschema-Abgleich (STRUXD-157 Figma-Screenshots, 2026-08-27)

*Kein Code-Scan, sondern Abgleich einer externen Design-Vorgabe (6 Screenshots aus Figma, `STRUXD-157 iOS app UI/UX concept for EasyNextcloud`, kein direkter Figma-Zugriff verfügbar) gegen die zu diesem Zeitpunkt bereits in dieser Karte erfassten Getter.*

**Bestätigte Vorgabe-Werte** (im Figma-Inspector als Hex sichtbar):
- `Color/Blue Ionos/B4` = `#1474C4` — Primary-Button Default, Selection-/Checked-Akzent, Links im Dark Mode
- `Color/Blue Ionos/B5` = `#095BB1` — Primary-Button Pressed (bewusst dunkler als Default statt heller — Muster: Pressed = dunklerer Farbwert)
- `Color/Cool Grey Ionos/C1`/`C8` — nur als Token-Name referenziert, Hex im Screenshot nicht aufgeklappt, daher nicht verifizierbar

**Abgleich:**

| Komponente/Zustand | Vorgabe (Dark) | Vorher im Code (Dark) | Fundstelle | Befund |
|---|---|---|---|---|
| Primary-Button, Default | `#1474C4` (B4) | `#5B60D6` | `buttonPrimaryColor()`, stratotheme.h:50 | ⚠️ Bruch — Indigo/Violett statt Blau |
| Primary-Button, Pressed | `#095BB1` (B5), dunkler als Default | `#5B60D6` — identisch zum Default | `buttonPrimaryPressedColor()`, stratotheme.h:59 | ⚠️ Bruch — kein Pressed-Kontrast im Dark Mode |
| Primary-Button, Hover | kein eigener Vorgabewert erfasst (Screenshots zeigten nur Default/Pressed/Disabled) | `#2944CC` fix, identisch zum Light-Wert | `buttonPrimaryHoverColor()`, stratotheme.h:55, eigener TODO-Kommentar | ⚠️ bereits dokumentierter Bruch |
| Pill-Button Primary (Tray) | `#1474C4` (B4), gleiche Familie wie Buttons | `#5B60D6` | `pillButtonPrimaryColor()`, stratotheme.h:93 | ⚠️ Bruch — teilte sich den Wert mit `buttonPrimaryColor()` |
| Secondary-Button, Rahmen (Default) | weißer Outline-Rahmen | `#FFFFFF` | `buttonSecondaryBorderColor()`, stratotheme.h:73 | ✅ Übereinstimmung |
| Secondary-Button, Pressed | Kontrastumkehr: Fläche weiß, Text dunkel | `#3A3B52` — bleibt dunkel | `buttonSecondaryPressedColor()`, stratotheme.h:81 | ⚠️ Bruch, **nicht angepasst** (s. u.) |
| Selection-/Checkbox-Akzent | `#1474C4` (B4), identisch zu Primary-Button | `#0082c9` (`Style.ncBlue`), fix, kein Dark-Wert | `SesCheckBox.qml:14/34/35` → `Theme::wizardHeaderBackgroundColor()`, theme.cpp:784, `Q_PROPERTY CONSTANT` | ⚠️ Bruch — dritte, unabhängige Blaufarbe |
| Links/Akzenttext | helles Blau | `#5FA8E0` (`folderWizardSubtitleColor`) bzw. `#5B60D6` (`settingsLinkColor`) | basetheme.h:324 bzw. stratotheme.h:38 | ❓ uneindeutig, **nicht angepasst** (s. u.) |
| Seiten-/Panel-Hintergrund | sehr dunkles Navy (C1, Hex unbekannt) | `#1F2024` | `dialogBackgroundColor()`/`trayBackgroundColor()`, stratotheme.h:18/34 | ❓ nicht verifizierbar, **nicht angepasst** |

**Vorgenommene Anpassungen:**
1. `buttonPrimaryColor()` Dark → `#1474C4`
2. `buttonPrimaryPressedColor()` Dark → `#095BB1`
3. `pillButtonPrimaryColor()` Dark → `#1474C4` (in Sync mit `buttonPrimaryColor()` gehalten, analog zur bestehenden Konvention bei `buttonSecondaryColor()`/`pillButtonSecondaryColor()`, siehe Kommentar dort)
4. `buttonPrimaryHoverColor()` Dark → `#1474C4` (identisch zum neuen Default; interim, da kein eigener Hover-Wert in der Vorgabe erfasst wurde — TODO-Kommentar entsprechend aktualisiert statt entfernt)
5. Neue `Style.sesCheckboxAccentColor`-Property (`Theme.darkMode ? "#1474C4" : ncBlue`) — `SesCheckBox.qml` nutzt jetzt diese statt direkt `Style.ncBlue`. Light-Mode-Wert bleibt dadurch unverändert (`#0082c9`), Dark Mode bekommt erstmals einen eigenen, mit der Vorgabe konsistenten Wert.

**Bewusst nicht angepasst** (fehlende oder unklare Vorgabe):
- Secondary-Button Pressed — Kontrastumkehr (Fläche + Textfarbe wechseln gemeinsam) wäre eine Strukturänderung am Button-Rendering, kein reiner Hex-Swap; hier nicht angefasst.
- `settingsLinkColor()`/`quotaProgressColor()` — teilen sich weiterhin die alte Indigo-Farbe (`#5B60D6`); kein bestätigter Link-Hex-Wert aus der Vorgabe, zwei Kandidaten-Getter mit unterschiedlicher Passung (s. Tabelle).
- `dialogBackgroundColor()`/`trayBackgroundColor()` (Seiten-/Panel-Hintergrund) — Vorgabe-Hex (`Cool Grey Ionos/C1`) unbekannt.

### Nachtrag (2026-08-27): MoreOptionsButtonStyle Icon-Hover-Kontrast

*Vom Nutzer per Live-Screenshot gemeldet, nicht Teil der ursprünglichen STRUXD-157-Vorgabe — reiner Kontrast-Bug im Settings-Ordnerlisten-„…"-Button.*

Beim Vergleich fiel auf, dass `MoreOptionsButtonStyle` (buttonstyle.h:244ff., Settings-Ordnerliste `folderstatusdelegate.cpp`) für sein Drei-Punkte-Icon als einziger der drei `ButtonStyle`-Typen den themenabhängigen `WLTheme.buttonIconColor()`/`buttonIconHoverColor()` verwendete (beide identisch `themedColor("#2f2f70","#C9CBEF")`), während `PrimaryButtonStyle`/`SecondaryButtonStyle` für **beide** Icon-Zustände fest `WLTheme.white()` nutzen (buttonstyle.h:139-147, 233-241). In der laufenden App erschienen die Punkte im Hover-Zustand (farbiger Kreis-Hintergrund) dadurch zu dunkel/schlecht lesbar.

| Property | Fundstelle | Vorher | Nachher | Status |
|---|---|---|---|---|
| `MoreOptionsButtonStyle::buttonIconHoverColor()` | buttonstyle.h:333 | `WLTheme.buttonIconHoverColor()` (themenabhängig, dark `#C9CBEF`) | `WLTheme.white()` (fix, wie bei Primary/Secondary) | ✅ angepasst |
| `MoreOptionsButtonStyle::buttonIconDefaultColor()` | buttonstyle.h:328 | `WLTheme.buttonIconColor()` (themenabhängig) | unverändert | ℹ️ nicht angefasst — Default-Zustand blendet laut Kommentar bewusst in den Zeilenhintergrund ein (`dialogBackgroundColor()`), dafür ist ein themenabhängiger Wert weiterhin sinnvoll |

### Nachtrag (2026-08-27): Native Checkbox-Häkchenfarbe (`sesStyle::drawCheckboxIndicator`)

*Ebenfalls per Live-Screenshot gemeldet (General Settings + Ordnerliste-Checkboxen). Betrifft die native QWidget-Checkbox (`sesstyle.cpp:93-153`, für alle `QCheckBox`-Widgets im Widget-Baum via `QProxyStyle`), nicht die QML-Komponente `SesCheckBox.qml`, die separat weiter oben in dieser Karte dokumentiert ist.*

`checkboxCheckmarkColor()` (basetheme.h:457) war bewusst gegenläufig zum Modus definiert: Weiß in Light Mode, **Schwarz in Dark Mode** — mit der Begründung, das sei "invertiert zum nativen Kontrast". Das ignoriert aber, dass die Checkbox-Füllfarbe (`WLTheme.buttonPrimaryColor()`, sesstyle.cpp:127) in beiden Modi eine gesättigte Akzentfarbe ist (Light `#272CB2`, Dark `#1474C4`) statt der Seiten-Hintergrundfarbe — ein schwarzes Häkchen auf Blau ist in beiden Modi schlecht lesbar.

| Property | Fundstelle | Vorher | Nachher | Status |
|---|---|---|---|---|
| `checkboxCheckmarkColor()` | basetheme.h:457-459 | Light `#FFFFFF` / Dark `#000000` | Light `#FFFFFF` / Dark `#FFFFFF` (fix) | ✅ angepasst |

**Hinweis:** Dieselbe Inkonsistenz (fixe Farbe vs. „invertiert nach Modus" ohne Rücksicht auf die tatsächliche Hintergrundfarbe am Einsatzort) ist strukturell verwandt mit dem oben dokumentierten `MoreOptionsButtonStyle`-Fund — in beiden Fällen wurde die Icon-/Glyphenfarbe an den Seiten-/Dialog-Hintergrund gekoppelt gedacht, obwohl der tatsächliche unmittelbare Hintergrund am Render-Ort (Akzentfarbe bzw. Hover-Kreis) ein anderer ist.

## Format je Komponente

```markdown
## <Komponente> (`<Datei(en)>`)

*Zuletzt geprüfter Commit: `<hash>` (<Datum>)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.titleColor()` | accountsettings.cpp:1922 | `#000000` | `#D6E4F5` | `themedColor()` in basetheme.h:318 | ✅ theme-aware |
| ... | ... | ... | ... | ... | ⚠️ Bruch: kein Dark-Wert |

**Brüche:** kurze Liste der ⚠️-Zeilen mit Einschätzung/Notiz, oder "keine".
```

Status-Werte: ✅ theme-aware · ⚠️ Bruch (kein Dark-Wert / Getter selbst ohne Dark-Wert) · ℹ️ bewusst fix (mit Begründung) · ℹ️ ambient (hängt von Palette-Propagation ab, ungeprüft/geprüft vermerken).
