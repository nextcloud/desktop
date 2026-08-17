# Kontext-Karte: src/gui

Diese Struktur unter `.claude/context/gui/` spiegelt `src/gui/` und liefert für jeden Bereich eine kompakte, KI-lesbare Beschreibung: Zweck, Kernklassen, wie die Teile zusammenspielen, und was fork-spezifisch (BRICKMAKERS/IONOS/HiDrive Next/Strato-Whitelabel) statt generischer Nextcloud-Upstream-Code ist. Bewusst **getrennt vom Quellcode** gehalten, damit sie nicht mit echten Source-Dateien verwechselt oder versehentlich mitgebaut wird.

`nc-desktop` ist ein Whitelabel-Fork des Nextcloud-Desktop-Clients für IONOS/HiDrive Next/STRATO. `src/gui/` enthält ~450 Dateien: 229 davon liegen flach direkt im Wurzelverzeichnis, der Rest verteilt sich auf 11 Unterordner.

## Navigation

**Unterordner** (jeweils eigenes `CLAUDE.md`, Struktur spiegelt `src/gui/<name>/`):

| Ordner | Inhalt |
|---|---|
| [tray/](tray/CLAUDE.md) | Systray-Popup: Header, Account-Menü, Activity-Feed, Sync-Status, Unified Search *(größter Unterordner, bekannter Merge-Hotspot)* |
| [wizard/](wizard/CLAUDE.md) | Account-Setup-Wizard (Server-URL, Login, Datenschutz-Consent, Ordner-Konfiguration) |
| [macOS/](macOS/CLAUDE.md) | macOS-spezifische Integration (FileProvider/VFS, XPC, Single-Instance) |
| [filedetails/](filedetails/CLAUDE.md) | Datei-Detailansicht mit Sharing-Tab |
| [creds/](creds/CLAUDE.md) | Authentifizierung & Credential-Speicherung (Keychain, Login Flow v2) |
| [updater/](updater/CLAUDE.md) | Selbst-Update-Mechanismus (NSIS/Sparkle/Passive) |
| [SesComponents/](SesComponents/CLAUDE.md) | Fork-eigene, wiederverwendete UI-/Logik-Bausteine ("Ses"-Präfix) |
| [cloudproviders/](cloudproviders/CLAUDE.md) | Linux `libcloudproviders`/GNOME-Integration |
| [ga4/](ga4/CLAUDE.md) | Google-Analytics-4-Telemetrie (fork-eigen) |
| [integration/](integration/CLAUDE.md) | Server-definierte Datei-Aktionen (Kontextmenü-Popup) |
| [socketapi/](socketapi/CLAUDE.md) | IPC-Server für Explorer/Finder-Shell-Erweiterungen |

**Wurzelverzeichnis** (229 Dateien ohne Unterordner, keine 1:1-Ordnerstruktur möglich):

- [COMPONENTS.md](COMPONENTS.md) — Tabelle aller Dateien, gruppiert in acht fachliche Cluster: Account & Connection, Folder & Sync Management, **Settings Dialog & App Settings (⚠️ bekannter Merge-Hotspot)**, Sharing & OCS API Jobs, User Status & Emoji, App Shell/Theming & Window Chrome, File Activity/Tags & External Editing, Platform Glue (macOS/Cocoa) & Misc.

## Fork-Erkennungsmerkmale

Wiederkehrende Marker, an denen sich fork-eigener Code von generischem Nextcloud-Upstream-Code unterscheiden lässt:
- Namenspräfix **"ses"** (BRICKMAKERS-Ticketpräfix, z. B. `sesStyle`, `SesComponents`, `sesFileIconProvider`) — praktisch immer fork-eigen.
- Compile-Guards **`IONOS_BUILD`, `IONOS_WL_BUILD`, `STRATO_WL_BUILD`** — schalten Code zwischen IONOS- und Strato/HiDrive-Next-Branding um; `StratoTheme` ist der `#else`-Default-Zweig.
- QML-Modul-Import **`com.strato.hidrivenext.desktopclient`** statt eines generischen `com.nextcloud.desktopclient` — praktisch jede `.qml`-Datei im Fork nutzt das.
- Klassen **`WLTheme`/`BaseTheme`/`IonosTheme`/`StratoTheme`** als zentrale Theming-Schicht über dem generischen `Theme`.
- Referenzen auf **"hidrivenext"** in Log-Kategorien/Dateinamen (z. B. `hidrivenext.log`) als Branding-Leck in sonst generischem Code.

## Bekannter Merge-Hotspot

Die **Settings-Dialog-Dateien im Wurzelverzeichnis** (`settingsdialog.*`, `generalsettings.*`, `networksettings.*`) sowie Teile von `tray/` (`MainWindow.qml`, `TrayWindowAccountMenu.qml`, `UserLine.qml`) sind historisch die häufigsten Stellen für Merge-Konflikte mit dem upstream `stable-x.y`-Branch. Bei Arbeiten dort den [`stable-merge-check`-Skill](../../skills/stable-merge-check/SKILL.md) nutzen (Referenz-Branch siehe dort in `reference-branch.txt`).

## Pflegehinweis

Diese Dokumente wurden am 2026-08-17 automatisiert per Code-Scan erstellt (nicht Zeile-für-Zeile von Boris verifiziert) und sollten vor blindem Vertrauen gegengelesen werden — insbesondere Aussagen zu "dead code"/unbenutzten Pfaden. Sie beschreiben Struktur/Zweck, nicht Implementierungsdetails, und veralten daher langsamer als der Code selbst — trotzdem bei größeren Umbauten in einem Bereich das jeweilige `CLAUDE.md` mit aktualisieren.
