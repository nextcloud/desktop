# src/gui/socketapi

Implements the local IPC server that talks to the OS shell-integration extensions (Explorer overlay-icon handler on Windows, Finder Sync extension on macOS, Nautilus/file-manager extensions on Linux), providing per-file sync status and context-menu actions. This is core, largely upstream Nextcloud infrastructure.

## Key classes/components

- `SocketApi` (`socketapi.h/.cpp`) — `QObject` owning a `QLocalServer` (named pipe on Windows, App-Group socket on macOS, runtime-dir socket on Linux/BSD). Accepts client connections (`SocketListener` per socket), parses newline-delimited text commands (`COMMAND:argument`) and dispatches them via Qt meta-object reflection to `command_XXX` slots (e.g. `command_RETRIEVE_FILE_STATUS`, `command_SHARE`, `command_ENCRYPT`, `command_LOCK_FILE`, `command_MAKE_AVAILABLE_LOCALLY`, `command_GET_MENU_ITEMS`, plus `ASYNC_*` job-based commands for GUI testing). Also broadcasts `STATUS`/`UPDATE_VIEW`/`REGISTER_PATH` push messages to all connected listeners as folders sync.
- `socketapi_p.h` — private helper types: `BloomFilter` (compact probabilistic set used to avoid pushing STATUS updates for unmonitored directories), `SocketListener` (per-connection wrapper with `sendMessage`/`sendWarning`/`sendError` and directory-interest tracking), `ListenerClosure` (one-shot callback-as-slot helper), and `SocketApiJob` (represents one async `ASYNC_*` request/response with `resolve`/`reject`, used for GUI-testing commands and richer JSON-based commands).
- `socketapi.cpp` — the bulk of the logic: `FileData` helper (resolves a local path to its `Folder`, relative/server paths, journal record), share/leave-share/encrypt/activity/file-actions request handling, context-menu building (`sendSharingContextMenuOptions`, `sendFileActionsContextMenuOptions`, `sendEncryptFolderCommandMenuEntries`, `sendLockFile*MenuEntries`), conflict resolution, file lock/unlock, direct-editing (`command_EDIT`), and private-link copy/email/open.
- `socketapi_mac.mm` — macOS-only Objective-C++ providing `socketApiSocketUrl()`, which resolves the App Group container URL for the local socket path (needed because macOS sandboxing requires shared App Group storage instead of a plain runtime-dir path).
- `CMakeLists.txt` — adds `socketapi.h/.cpp` always, and `socketapi_mac.mm` only when building on Apple platforms.

## How it fits together

`SocketApi` is the single long-lived server object (created by `owncloudgui`), and each shell-extension process connects as a client and speaks the line-based `command_*` protocol; `FolderMan`/`Folder`/journal DB provide the data `SocketApi` needs to answer status queries and build context menus, while `SyncFileItem`/`SyncJournalFileRecord`/`ShareManager`/`EncryptFolderJob`/`ConflictDialog` are invoked to actually perform requested actions.

## Fork-specific notes

- Mostly upstream Nextcloud code; the one visible fork-specific touch is the `#ifndef IONOS_BUILD` guard around the "Activity" context-menu item in `command_GET_MENU_ITEMS`, which suppresses that menu entry specifically in the IONOS/whitelabel build.

*Quelle: src/gui/socketapi — Stand 2026-08-17, automatisch erstellt, bitte gegenlesen.*
