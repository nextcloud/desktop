<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# macOS Finder Integration (FinderSync) Extension

## Overview

The Finder integration — the Nextcloud context-menu entries and the sync-status
icons (badges) shown for folders under Finder's **Favorites** — is provided by a
separate macOS app extension, `FinderSyncExt` (bundle identifier
`com.nextcloud.desktopclient.FinderSyncExt` for the official build). It is
distinct from the File Provider extension (`FileProviderExt`), which drives the
**Locations** integration for virtual files. macOS allows only one of the two to
be active at a time, by design.

For the integration to work, three things must happen:

1. **Registration** — macOS (LaunchServices / PlugInKit, `pluginkit`) must
   discover and elect the extension for the logged-in user.
2. **Enablement** — the user must have it enabled (this is the state a `+` in
   `pluginkit -m` reflects).
3. **Connection** — the extension process, launched by Finder, must connect back
   to the running desktop client over XPC and complete a handshake.

Historically the integration appeared "not loaded" (missing badges/menus after a
reboot) even when steps 1 and 2 were fine, because step 3 could silently fail —
see [#10032](https://github.com/nextcloud/desktop/issues/10032),
[#8471](https://github.com/nextcloud/desktop/issues/8471) and
[#8363](https://github.com/nextcloud/desktop/issues/8363). The extension now only
reports itself connected after a real XPC **handshake** round-trip with the
client succeeds, and both the installer and the runtime log every outcome so a
failure is recognizable from logs alone. This document explains how to read them.

## Verifying installation

The installer's postinstall script ([`admin/osx/post_install.sh.cmake`](../admin/osx/post_install.sh.cmake))
registers and enables the extension **for the logged-in console user**
(`pluginkit` state is per-user, and the script itself runs as `root`, so it hops
into the user's context with `launchctl asuser` + `sudo -u`). Every step is
logged to `/var/log/install.log` with the prefix `Nextcloud FinderSync:`.

```console
grep "Nextcloud FinderSync" /var/log/install.log
```

A healthy installation ends with:

```
Nextcloud FinderSync: pluginkit -e use succeeded for com.nextcloud.desktopclient.FinderSyncExt
Nextcloud FinderSync: extension present in pluginkit database after registration (see line above for election state)
```

Signals that registration did **not** happen:

| Log line | Meaning |
| --- | --- |
| `WARNING pluginkit -a failed …` / `WARNING pluginkit -e use failed …` | The registration/election command returned non-zero. |
| `WARNING extension NOT present in pluginkit database after registration …` | The extension is still not registered after the attempt. |
| `no console user logged in; skipping pluginkit registration …` | Installed with no GUI user (e.g. MDM/remote). The extension is expected to register on the user's first launch of the app instead. |
| `pluginkit not found; cannot register extension …` | `pluginkit` is unavailable on this system. |

## Verifying at runtime

### Is the extension registered and enabled?

```console
pluginkit -mvvv -i com.nextcloud.desktopclient.FinderSyncExt
```

A leading `+` means enabled, `-` means installed but disabled, `?` means
registered without an explicit election, and empty output means it is not
registered at all.

### Does the extension reach the client? (the handshake)

The extension logs its connection lifecycle to the unified system log under its
own bundle identifier as subsystem. Watch it live while the client starts (or
right after logging in):

```console
log stream --level info --predicate 'subsystem == "com.nextcloud.desktopclient.FinderSyncExt"'
```

Or inspect what already happened (info-level entries are not persisted forever,
so keep the window short):

```console
log show --last 10m --info --predicate 'subsystem == "com.nextcloud.desktopclient.FinderSyncExt"'
```

Key messages and what they mean:

| Message | Level | Meaning |
| --- | --- | --- |
| `FinderSync XPC handshake succeeded; connection to app is live` | info | **Working.** The extension confirmed the client is on the other end. |
| `Performing XPC handshake with app (generation N)` | info | A connection attempt is in progress. |
| `FinderSync XPC handshake timed out after 5 s` | error | The client was reachable but did not answer the handshake. A reconnect is scheduled. |
| `FinderSync XPC message to app failed: …` | error | The client is not listening yet (e.g. extension started before the app). A reconnect is scheduled. |
| `FinderSync XPC connection lost (<reason>); scheduling reconnect` | error | An established connection dropped (client quit, interrupted, …). |
| `Scheduling reconnect in N seconds` | info | Backoff between retries (1 → 2 → 4 → 8 s). |

Transient failures immediately after login are normal: the extension is
typically launched by Finder before the client finishes starting, so you may see
a `message to app failed` / reconnect or two followed by `handshake succeeded`.
What indicates a real problem is `handshake succeeded` **never** appearing, or a
previously-live connection producing repeated `connection lost` entries.

The client side of the same handshake is logged in the **desktop client log**
(not the system log), at info level under the categories
`nextcloud.gui.macos.findersync.xpc` and `nextcloud.gui.macfindersyncservice`.
The positive confirmation there is:

```
FinderSync extension handshake received; connection to app is live
```

## Remediation

If the extension is registered and enabled (`+` above) but never completes the
handshake, and the client is definitely running:

1. Disable and re-enable **Nextcloud** in **System Settings** under **Login Items
   & Extensions** (on older macOS: **Privacy & Security ▸ Extensions**).
2. Relaunch Finder (`killall Finder`).

These are workarounds for a stuck state, not a fix. When reporting a problem,
attach the output of the `grep`, `pluginkit -m` and `log show` commands above.

## Implementation references

- Extension XPC client and handshake / reconnect logic:
  [`shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/FinderSyncXPCManager.m`](../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/FinderSyncXPCManager.m)
- Extension principal object (badges, menus, bounded menu wait):
  [`shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/FinderSync.m`](../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/FinderSync.m)
- Shared XPC protocol (`performHandshakeWithReply:`):
  [`shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncAppProtocol.h`](../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncAppProtocol.h)
- Client-side XPC listener and service:
  [`src/gui/macOS/findersyncxpc_mac.mm`](../src/gui/macOS/findersyncxpc_mac.mm),
  [`src/gui/macOS/findersyncservice.mm`](../src/gui/macOS/findersyncservice.mm)
- Installer registration:
  [`admin/osx/post_install.sh.cmake`](../admin/osx/post_install.sh.cmake)
