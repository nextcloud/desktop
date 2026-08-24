# src/gui/filedetails

QML + C++ backend for the "file details" side panel/window: a per-file view showing file
info (icon, size, modified date, lock state, tags) and a "Sharing" tab for managing
Nextcloud shares (links, users/groups, permissions, passwords, expiry, notes). Opened from
the tray context menu or Explorer/Finder overlay context menu for a selected synced file.

## Key classes/components

- **`FileDetails`** (`filedetails.h/cpp`) – C++ model backing the header area: resolves a
  local path to its `Folder`/`SyncJournalFileRecord`, exposes name/size/last-changed/icon,
  file-lock status (polls every 6s while locked), and the file's `FileTagModel`
  (from `gui/filetagmodel.h`). Also exposes `sharingAvailable` (server capability check).
- **`ShareModel`** (`sharemodel.h/cpp`, large — 1502 lines in the .cpp, ~1770 combined) – the core sharing model:
  `QAbstractListModel` wrapping `ShareManager` to list/create/delete/modify shares
  (link, user/group, team, room, remote), handles password/expiry/note/permission toggles,
  placeholder rows for "create link share" / secure-file-drop, and "shared with me" info.
  Views: **`ShareView.qml`** (tab body: owner banner, `ShareeSearchField`, list of shares)
  and **`ShareDelegate.qml`** (compact per-share row) / **`ShareDetailsPage.qml`**
  (full-screen detail editor for one share, pushed onto the `StackView` when a share row
  is tapped).
- **`SortedShareModel`** (`sortedsharemodel.h/cpp`) – `QSortFilterProxyModel` ordering
  `ShareModel` rows (e.g. placeholder/internal links first) for `ShareView`'s `ListView`.
- **`ShareeModel`** (`shareemodel.h/cpp`) – autocomplete model that queries the server for
  users/groups/teams to share with (local + "search globally" modes). View:
  **`ShareeSearchField.qml`** (search `TextField` + popup) with **`ShareeDelegate.qml`**
  as the suggestion row.
- **`DateFieldBackend`** (`datefieldbackend.h/cpp`, `OCC::Quick` namespace) – small helper
  exposing a `QDate` with min/max bounds and locale string parsing for QML date entry;
  used by **`NCInputDateField.qml`** (expiry-date picker in `ShareDetailsPage`).
- **`NCInputTextField.qml` / `NCInputTextArea.qml` / `NCTabButton.qml`** – small generic
  styled input controls (password/note fields, permission-mode tabs) shared by
  `ShareDetailsPage.qml` and `ShareView.qml`'s password dialog.
- **`FileTag.qml`** – single colored tag chip rendered in the file-details header, driven
  by `FileDetails.fileTagModel`.
- **`FileActivityView.qml`** – wraps the generic `ActivityList` (from `../tray`) with a
  `FileActivityListModel` filtered to one file's activity; an "Activity" tab intended as a
  sibling of the sharing tab (see fork note below).
- **`FileDetailsPage.qml` → `FileDetailsView.qml` → `FileDetailsWindow.qml`** – the
  assembly chain: `FileDetailsPage` is a `Page` with the header (icon/name/size/lock/tags)
  and a `SwipeView` for the tab content; `FileDetailsView` wraps it in a `StackView` (so
  `ShareDetailsPage` can be pushed on top); `FileDetailsWindow` wraps that in a standalone
  `ApplicationWindow`.

## How it fits together

`Systray`/`ownCloudGui` (see `src/gui/systray.cpp`) instantiate `FileDetailsWindow.qml`
for a given `accountState`/`localPath` when the user picks "Share…" or "Activity" from a
file's tray/overlay context menu, and route `Systray::showFileDetailsPage` signals to
switch tabs on an already-open window. Inside, `FileDetailsPage` owns one `FileDetails`
instance (header data) and hosts tab content in a `SwipeView`; each `ShareDelegate` row
can push a `ShareDetailsPage` onto the shared `rootStackView` for full editing of that
share. `ShareModel` and `ShareeModel` are independent QAbstractListModels created per view
instance, parameterized by `accountState`/`localPath`.

## Fork-specific notes

- QML files import `com.strato.hidrivenext.desktopclient` (this build's whitelabel QML
  module name) rather than a generic Nextcloud module name — check the active branding
  target's module alias when porting code between forks.
- `FileDetailsPage.qml`'s `SwipeView` currently wires up only the `ShareView` tab; the
  `Connections` handler still branches on `Systray.FileDetailsPage.Activity` and
  references `fileActivityView.swipeIndex`, but no `FileActivityView` item exists in the
  `SwipeView` — the Activity tab appears to have been stripped from the UI in this fork
  while `FileActivityView.qml`/`FileActivityListModel` wiring in `systray.cpp` was left
  in place (likely dead code path — candidate for cleanup, verify usages before removing).
- Styling throughout (`Style.ses*` properties, `SesErrorBox`, `../SesComponents`) is
  fork-specific "SES" theming layered over otherwise-generic upstream Nextcloud sharing
  logic (`ShareModel`, `ShareeModel`, `ShareManager` itself live in `src/gui` more broadly).

*Quelle: src/gui/filedetails — Stand 2026-08-20, automatisch erstellt, bitte gegenlesen.*
