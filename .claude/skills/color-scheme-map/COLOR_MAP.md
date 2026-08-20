# Farbschema-Landkarte

Wird vom [color-scheme-map](SKILL.md)-Skill Komponente für Komponente befüllt — kein Vollscan, sondern nur die Komponenten, die tatsächlich geprüft wurden.

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

*Zuletzt geprüfter Commit: `d47ac72f5` (2026-08-20)*

| Property | Fundstelle | Light-Wert | Dark-Wert | Quelle | Status |
|---|---|---|---|---|---|
| `WLTheme.dialogBackgroundColor()` | accountsettings.cpp:605, 998, 1178, 1917, 1934, 1936 | `#F7F7F9` | `#1F2024` | `StratoTheme::dialogBackgroundColor()`, stratotheme.h:17 (überschreibt `themedColor("#FAFAFA","#1E2126")` in basetheme.h:441) | ✅ theme-aware |
| `WLTheme.titleColor()` | accountsettings.cpp:997, 1177, 1431, 1925, 1928, 1937, 1940 | `#000000` | `#D6E4F5` | `themedColor()` in basetheme.h:318 (keine Strato-Override) | ✅ theme-aware |
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
