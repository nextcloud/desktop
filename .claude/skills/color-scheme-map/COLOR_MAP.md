# Farbschema-Landkarte

Wird vom [color-scheme-map](SKILL.md)-Skill Komponente für Komponente befüllt — kein Vollscan, sondern nur die Komponenten, die tatsächlich geprüft wurden.

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
