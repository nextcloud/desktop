# src/gui/SesComponents

This folder holds small, fork-specific UI/logic components used across the GUI, kept separate from upstream Nextcloud code so they survive upstream merges with minimal conflicts. "Ses" is this team's internal ticket/component prefix (BRICKMAKERS work on the IONOS/HiDrive Next/STRATO whitelabel fork). All four files here are fork-specific — there is no upstream equivalent folder.

## Key classes/components

- `SesErrorBox.qml` — reusable inline error banner (icon + bold "Error" title + wrapped message text), styled via `Style.ses*` tokens; used e.g. in `filedetails/ShareDetailsPage.qml` to show password errors.
- `SesTrayHeader.qml` — custom tray-popup header bar (account menu button, "Website" link, local/team-folder button, optional "featured app"/assistant button); embedded into `tray/MainWindow.qml` as `trayWindowHeaderBackground`, replacing/restyling the upstream Nextcloud tray header row for the whitelabel look.
- `syncdirvalidation.h` / `syncdirvalidation.cpp` — `SyncDirValidator` class: on Windows, checks that a chosen sync directory is not the app's own `%AppData%` roaming path (to avoid users pointing sync at the app's own data folder); on other OSes it's a no-op always returning valid. Used by `folderwizard.cpp` and `wizard/owncloudadvancedsetuppage.cpp` during local-folder selection.

## How it fits together

The two QML components are drop-in replacements/extensions for tray and sharing UI, pulled in via relative QML imports (`import "../tray/"` etc.) and the shared `Style` singleton for whitelabel theming; `SyncDirValidator` is an unrelated plain C++ helper reused by folder-setup wizards to validate a picked path. All three are additive fork logic, not modifications of upstream files.

## Fork-specific notes

- Entire folder is fork-only (BRICKMAKERS/IONOS-HiDrive Next fork), no upstream Nextcloud equivalent directory exists.
- `SesTrayHeader.qml` specifically substitutes for/duplicates parts of what would otherwise be inline header markup in upstream `tray/MainWindow.qml`.

*Quelle: src/gui/SesComponents — Stand 2026-08-17, automatisch erstellt, bitte gegenlesen.*
