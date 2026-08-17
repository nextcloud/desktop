# src/gui/cloudproviders

Linux-only desktop integration with the freedesktop.org `libcloudproviders`/GNOME Online Accounts style "cloud providers" D-Bus interface, letting file managers (e.g. GNOME Files/Nautilus) show native sync status and a context menu for each configured Nextcloud account/folder. Compiled only when CMake finds libcloudproviders (`WITH_LIBCLOUDPROVIDERS`, see `src/gui/CMakeLists.txt`).

## Key classes/components

- `CloudProviderManager` (`cloudprovidermanager.h/.cpp`) — owns the D-Bus name/provider exporter (`g_bus_own_name`), keeps a `QMap<alias, CloudProviderWrapper*>` in sync with `FolderMan`'s folder list (`slotFolderListChanged`), creating/destroying one wrapper per sync folder.
- `CloudProviderWrapper` (`cloudproviderwrapper.h/.cpp`) — wraps one `OCC::Folder` as a `CloudProvidersAccountExporter`: builds its GMenu (open desktop app, open in browser, recent files, pause sync, help/settings/logout/quit), an action group handling those GActions, and pushes live status text from `ProgressDispatcher`/`Folder` signals (sync started/finished/paused, progress).
- `cloudproviderconfig.h.in` — CMake-configured header defining the D-Bus bus name/object path constants (`LIBCLOUDPROVIDERS_DBUS_BUS_NAME/OBJECT_PATH`) used by the manager.

## How it fits together

`CloudProviderManager` is instantiated once (from `owncloudgui.cpp`) and acts as the top-level bridge between OCC's `FolderMan`/`Folder` model and glib/GIO's cloud-providers D-Bus API; it creates a `CloudProviderWrapper` per folder, and each wrapper independently listens to its `Folder`'s Qt signals and translates them into GObject/GVariant calls understood by GNOME's file manager integration.

## Fork-specific notes

- Entirely generic/upstream-style Nextcloud Linux integration (SPDX headers say "Nextcloud GmbH and contributors", 2017) — not fork-specific, though it does read `APPLICATION_NAME`/`APPLICATION_ICON_NAME` theme macros so the exported label/icon follow whitelabel branding.

*Quelle: src/gui/cloudproviders — Stand 2026-08-17, automatisch erstellt, bitte gegenlesen.*
