# src/gui/integration

Implements the "server-defined file actions" feature: lets the Nextcloud server describe custom context-menu actions for specific file types (via capabilities), and shows/executes them in a small popup window from the systray/shell-integration context menu. This is fairly new, generic Nextcloud functionality (2025 SPDX headers, `namespace OCC`), not obviously fork-specific.

## Key classes/components

- `FileActionsModel` (`fileactionsmodel.h/.cpp`) — `QAbstractListModel` (namespace `OCC`) driven by `accountState`, `localPath`, `fileId`, `remoteItemPath` properties. On `parseEndpoints()` it reads `account->capabilities().fileActionsByMimeType(mimeType)` to build a list of `FileAction` (icon/name/url/method/params), issues the chosen action as a `JsonApiJob` on `createRequest(row)`, and parses the server's JSON reply in `processRequest()` into a displayable `Response` (label + URL) shown as a clickable result button.
- `FileActionsWindow.qml` — the popup `ApplicationWindow` (frameless or normal window depending on `Systray.useNormalWindow`) that binds a `FileActionsModel`, lists available actions in a `ListView`, and shows the response link/button after an action completes; closes and opens the response URL in the browser via `Systray.openUrlInBrowser`.

## How it fits together

`systray.cpp`/`owncloudgui.cpp` create/show `FileActionsWindow.qml` (registered as a QML type) for a given file, which instantiates `FileActionsModel` bound to the current `AccountState`/file path/`fileId`; the model fetches server-capability-defined actions, executes the selected one via a network job, and reports results back into the QML window. `folderman.h` is referenced for locating the sync folder/journal record for a given local path (to resolve file IDs and remote paths).

## Fork-specific notes

- Nothing here appears fork-specific except cosmetic reuse of shared `Style`/whitelabel QML singletons (`import Style`, `com.nextcloud.desktopclient`); the feature itself, its model, and its network protocol look like generic (recent) upstream-style Nextcloud functionality carried into the fork.

*Quelle: src/gui/integration — Stand 2026-08-17, automatisch erstellt, bitte gegenlesen.*
