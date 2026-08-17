# src/gui/tray

This folder implements the Nextcloud Desktop **system tray popup**: the tray icon's dropdown window (account switcher, activity/notification feed, unified search, sync status, group-folder shortcuts, incoming-call/talk-reply dialogs). C++ classes expose Qt models/objects to QML (registered under the `com.strato.hidrivenext.desktopclient` module); the `.qml` files render them. The window is created/owned by `Systray` (src/gui/systray.cpp, outside this folder), which loads `MainWindow.qml`.

## Key C++ backing classes
- **`UserModel`/`User`** (usermodel.h/.cpp) — list model of connected accounts; `User` wraps one `AccountState` and exposes name/avatar/status/sync-status/group-folders/assistant (AI) state to QML. Central hub other tray objects hang off of (`getActivityModel()`, `getUnifiedSearchResultsListModel()`).
- **`UserAppsModel`** — list of the account's server "apps" shown in the apps menu.
- **`ActivityListModel`** (+ `SortedActivityListModel` proxy) — feed of notifications/sync results/errors/activities per account; backs `ActivityList.qml`/`ActivityItem*.qml`. Data rows are `Activity`/`ActivityLink`/`PreviewData` (activitydata.h).
- **`ServerNotificationHandler`** (notificationhandler.cpp) — fetches server notifications via OCS API and feeds them into `User`/`ActivityListModel`.
- **`SyncStatusSummary`** — aggregates folder sync progress/state across the current account; drives `SyncStatus.qml`.
- **`UnifiedSearchResultsListModel`** + `UnifiedSearchResult` — server-wide search results; backs `UnifiedSearchInputContainer.qml`/`UnifiedSearchResult*.qml` (currently hidden, `visible: false //SES-4 removed`).
- **`TalkReply`** — sends Talk chat replies from notification/activity items; used with `TalkReplyTextField.qml`.
- **`ImageProvider`/`AsyncImageResponse`, `TrayImageProvider`, `SvgImageProvider`** — async QML image providers for avatars, server icons, and recolored SVGs.

## Key QML components (MainWindow tree)
- **`MainWindow.qml`** — the tray popup `ApplicationWindow`; hosts header, sync-status warning banner, activity list, unified search, user-status/file-details drawers, and the AI assistant panel. Reacts to `UserModel`/`Systray` signals.
- **`HeaderLogo.qml`** + **`SesTrayHeader.qml`** (in `../SesComponents/`, fork-specific) — the actual active header bar, containing `TrayWindowAccountMenu`, the website `HeaderButton`, `TrayFoldersMenuButton`, and the apps menu.
- **`TrayWindowAccountMenu.qml`** + **`UserLine.qml`** — current-account button and its dropdown menu (per-account rows via `Instantiator` over `UserModel`, add-account/pause-sync/settings/exit entries).
- **`AccountMenuItem.qml`** — generic styled menu-item row reused inside the account menu and elsewhere (e.g. `ActivityItemContent.qml`).
- **`ActivityList.qml`** / **`ActivityItem.qml`** / **`ActivityItemContent.qml`** / **`ActivityItemActions.qml`** / **`ActivityItemContextMenu.qml`** — the notification/activity feed list and its per-row rendering, action buttons, and context menu.
- **`SyncStatus.qml`** (wraps `NC.SyncStatusSummary`) + **`NCBusyIndicator.qml`** / **`NCProgressBar.qml`** — sync spinner/progress shown in the header area.
- **`TrayFoldersMenuButton.qml`** + **`TrayFolderListItem.qml`** + **`AutoSizingMenu.qml`** — "open local/group folder" button and its folder-picker popup menu.
- **`UnifiedSearchInputContainer.qml`** + **`UnifiedSearchResult*.qml`** (`Item`, `ListItem`, `SectionItem`, `Skeleton*`, `NothingFound`, `FetchMoreTrigger`, `PlaceholderView`) — unified search box and results list (feature-flagged off, `SES-4`).
- **`CallNotificationDialog.qml`** — incoming Talk call popup; **`EncryptionTokenDiscoveryDialog.qml`** / **`EditFileLocallyLoadingDialog.qml`** — misc modal dialogs launched from activities.
- Small shared building blocks: `HeaderButton.qml`, `IconButton.qml`, `PrimaryPillButton.qml`/`SecondaryPillButton.qml`, `EnforcedPlainTextLabel.qml`, `ListItemLineAndSubline.qml`, `NCIconWithBackgroundImage.qml`.
- **Legacy/unused (superseded by the fork's `SesTrayHeader`)**: `TrayWindowHeader.qml` and `TrayWindowHeaderBar.qml` (upstream header variants) and `CurrentAccountHeaderButton.qml` (upstream account button, only referenced by `TrayWindowHeader.qml`) are still present but not instantiated from `MainWindow.qml`'s actual render tree — kept from upstream merges, replaced functionally by `TrayWindowAccountMenu.qml`/`UserLine.qml`.

## How it fits together
`Systray` creates the QML engine and loads `MainWindow.qml`, which is shown/hidden on tray-icon click. Inside, `HeaderLogo` + `SesTrayHeader` render the header; `SesTrayHeader` instantiates `TrayWindowAccountMenu`, which opens a `Menu` populated by an `Instantiator` over the global `UserModel` singleton, one `UserLine` per account, plus add-account/pause/settings/exit items. Below the header, `SyncStatus` shows aggregate sync progress, and the main body is an `ActivityList` bound to `UserModel.currentUser.getActivityModel()` (via `SortedActivityListModel`), rendering `ActivityItem`/`ActivityItemContent` rows whose actions/links come from `ActivityListModel`/`ActivityLink`. Switching the current account (`UserModel.currentUserId`) swaps which `User`'s activity/search/sync models the whole window is bound to. Drawers (`UserStatusSelectorPage`, `FileDetailsView`) and dialogs (`CallNotificationDialog`, `EncryptionTokenDiscoveryDialog`) are opened on demand from `Systray`/`UserModel` signals.

## Fork-specific vs upstream
- All `.qml` files import the fork's `com.strato.hidrivenext.desktopclient` QML module (HiDrive Next/Strato branding) rather than a generic Nextcloud one, and use `Style.ses*` properties for fork theming (colors, fonts, icon sizes) defined in `theme/Style/Style.qml`.
- `SesTrayHeader.qml` (in `../SesComponents/`) is a fork-only replacement for upstream's `TrayWindowHeaderBar.qml`/`TrayWindowHeader.qml`; `TrayWindowAccountMenu.qml`/`UserLine.qml` are fork-only replacements for upstream's `CurrentAccountHeaderButton.qml`, tied to tickets like `SES-459`/`SES-511`/`SES-589`.
- `AsyncImageResponse` (asyncimageresponse.h/.cpp) pulls in `SesFileIconProvider` (`sesFileIconProvider.h`) for fork-specific file-type icon rendering.
- Several upstream features are explicitly disabled in this fork via `visible: false //SES-4 removed` (Unified Search input, Talk/apps header buttons), while the underlying C++ models (`UnifiedSearchResultsListModel`, etc.) remain in the codebase unused by the active UI.
- Generic/unmodified upstream Nextcloud logic: `ActivityListModel`/`activitydata`, `ServerNotificationHandler`, `SyncStatusSummary`, `UserModel`/`User` core account logic, `TalkReply`, and the various list-model/image-provider plumbing.

## Merge-Risiko-Hinweis
Dieser Ordner enthält mehrere der historisch häufigsten Merge-Konflikt-Stellen des Forks (`MainWindow.qml`, `TrayWindowAccountMenu.qml`, `UserLine.qml`) — bei Änderungen hier den [stable-merge-check-Skill](../../../skills/stable-merge-check/SKILL.md) nutzen.

*Quelle: src/gui/tray — Stand 2026-08-17, automatisch erstellt, bitte gegenlesen.*
