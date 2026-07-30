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

For the integration to work, four things must happen:

1. **Registration** — macOS (LaunchServices / PlugInKit, `pluginkit`) must
   discover and elect the extension for the logged-in user.
2. **Enablement** — the user must have it enabled (this is the state a `+` in
   `pluginkit -m` reflects).
3. **Broker availability** — the `FinderSyncBroker` login item must be registered
   with Service Management *and* not switched off by the user or an MDM profile,
   and its Mach service must actually be reachable.
4. **Connection** — the extension, launched by Finder, must collect the client's
   listener endpoint from the broker and complete a handshake over it.

### Why there is a broker

The client cannot vend a named XPC endpoint of its own. Only a launchd job may
advertise a Mach service name, and a plain application is not one — the
`xpc_connection_create(3)` manual page states that names not declared in a
`launchd.plist` may not be registered dynamically, and that the allowances XPC
makes for this "in debug scenarios" will "absolutely NOT be made in the
production scenario".

A Service Management login item *is* a launchd job, so `FinderSyncBroker` vends
the name instead. The client creates an **anonymous** listener and publishes its
endpoint to the broker; the extension collects that endpoint and connects
directly. The broker relays nothing afterwards — no badge or menu traffic passes
through it. Its Mach service name is its own bundle identifier
(`<team>.<reverse domain>.FinderSyncBroker`), which is both team-prefixed, as
login items require, and inside the shared App Group prefix, which is what lets
both sandboxed peers look it up **without any `temporary-exception`
entitlement**.

Earlier, the integration appeared "not loaded" (missing badges/menus after a
reboot) even when steps 1 and 2 were fine, because the connection could silently
fail — see [#10032](https://github.com/nextcloud/desktop/issues/10032),
[#8471](https://github.com/nextcloud/desktop/issues/8471) and
[#8363](https://github.com/nextcloud/desktop/issues/8363). The extension only
reports itself connected after a real **handshake** round-trip succeeds, and
every outcome is logged, so a failure is recognizable from logs alone. This
document explains how to read them.

> **Never validate any of this from Xcode.** Because of the debugging support
> mentioned above, a broken channel can appear to work under the debugger and
> fail for every user. Install one copy in `/Applications`, launch it from
> Finder, and attach to the running process if you need a debugger.

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

### Is the broker registered and serving?

This is the check whose absence made the 34.0.0 failure invisible. `pluginkit`
can look perfectly healthy while the broker is missing.

```console
UID_=$(id -u)
L=$(defaults read /Applications/Nextcloud.app/Contents/Info CFBundleIdentifier)
BROKER="$(codesign -d --entitlements :- /Applications/Nextcloud.app 2>/dev/null \
  | plutil -extract com.apple.security.application-groups.0 raw -).FinderSyncBroker"

launchctl print "gui/$UID_/$BROKER"        # the job: Mach endpoints, PID, last exit status
launchctl print-disabled "gui/$UID_" | grep -i findersync
sfltool dumpbtm | grep -A6 -i findersyncbroker    # Background Task Management view
```

`launchctl print` reporting `Could not find service …` means the login item is
not registered for this user — the client registers it on launch, so this points
at that having failed. If it *is* registered but disabled, `print-disabled` and
`sfltool dumpbtm` will say so; the user or an MDM profile turned it off in
**System Settings ▸ General ▸ Login Items & Extensions**, and no amount of
retrying from the client can override that.

The broker also checks itself on startup and says so in the system log:

```console
log show --last 10m --info --predicate 'subsystem CONTAINS "FinderSyncBroker"'
```

| Message | Level | Meaning |
| --- | --- | --- |
| `Self-check passed: … is registered and serving` | info | **The broker is healthy.** |
| `Self-check failed: the Mach service … is not reachable` | fault | The name did not register. Nothing can reach the client. |
| `Self-check timed out after 5s` | fault | The name exists but the broker did not answer its own ping. |
| `Info.plist is missing NCDevelopmentTeam …` | fault | Packaging fault; not retried. |

For enterprise fleets, the login item can be pre-approved with a
`com.apple.servicemanagement` configuration profile (macOS 13+, requires
user-approved MDM), which avoids relying on each user to allow it.

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
| `Connecting to FinderSync broker: …` | info | Opening the broker link. |
| `Broker pushed endpoint generation N` | info | The client published an endpoint; connecting to it now. |
| `Broker has no client endpoint yet; waiting for it to publish` | debug | Normal at login — Finder starts the extension before the client runs. Not retried; the broker pushes when one exists. |
| `Cannot reach the FinderSync broker: …` | error | The login item is missing or switched off. Check the broker section above. |
| `Broker reports no client endpoint; dropping peer connection` | info | The client quit. Its endpoint died with it, so it is discarded rather than reused. |
| `Performing XPC handshake with app (generation N)` | info | A connection attempt is in progress. |
| `FinderSync XPC handshake timed out after 5 s` | error | The endpoint was reachable but the client did not answer. A reconnect is scheduled. |
| `FinderSync XPC connection lost (<reason>); scheduling reconnect` | error | An established connection dropped. |
| `Scheduling reconnect in N seconds` | info | Backoff between retries (1 → 2 → 4 → 8 s). |
| `Info.plist is missing NCApplicationGroupIdentifier or NCDevelopmentTeam` | error | Packaging fault; not retried. |

Transient states immediately after login are normal: the extension is typically
launched by Finder before the client finishes starting, so expect
`Broker has no client endpoint yet` followed later by `handshake succeeded`. What
indicates a real problem is `handshake succeeded` **never** appearing, or a
previously-live connection producing repeated `connection lost` entries.

The client side is logged in the **desktop client log** (not the system log), at
info level under `nextcloud.gui.macos.findersync.xpc`,
`nextcloud.gui.macos.findersync.broker` and
`nextcloud.gui.macfindersyncservice`. Positive confirmations there:

```
FinderSync broker login item is enabled
Broker accepted our endpoint, generation 1
FinderSync broker is reachable; the extension can connect
FinderSync extension handshake received; connection to app is live
```

And the line that means Finder integration is dead, whatever else looks healthy:

```
FinderSync broker is not reachable; Finder badges and the context menu will not
work until it is
```

When the extension cannot reach the client, its context menu shows a single
disabled **“Nextcloud — not connected”** item rather than nothing at all, so the
failure is visible in a screenshot instead of being indistinguishable from the
extension not being installed.

## Remediation

If the extension is registered and enabled (`+` above) but never completes the
handshake, and the client is definitely running:

1. **Check the broker first** — `launchctl print gui/$(id -u)/<broker id>`. If the
   login item is switched off, re-enable it in **System Settings ▸ General ▸
   Login Items & Extensions**; the client cannot re-enable it for you, and
   repeated registration attempts fail with `kSMErrorLaunchDeniedByUser`.
2. Disable and re-enable **Nextcloud** in **System Settings** under **Login Items
   & Extensions** (on older macOS: **Privacy & Security ▸ Extensions**).
3. Relaunch Finder (`killall Finder`).

Steps 2 and 3 are workarounds for a stuck state, not fixes. When reporting a
problem, attach the output of the `grep`, `pluginkit -m`, `launchctl print` and
`log show` commands above.

## Release checklist

Neither the broker's registration nor the Mach service can be exercised by PR CI:
[`macos-build-and-test.yml`](../.github/workflows/macos-build-and-test.yml) builds
and runs `ctest`, with no signing identity, no GUI session, and without ever
launching the app. `ctest` covers only the identifier contract
([`test/testfindersyncbrokeridentity.cpp`](../test/testfindersyncbrokeridentity.cpp)),
which pins the strings the three bundles must agree on. Everything else is manual:

On a build installed from the real `.pkg` and launched by double-clicking it in
`/Applications` — never from Xcode:

1. `codesign -d --entitlements :-` on the app, the appex **and** the login item.
   The login item must show only `app-sandbox` and `application-groups`. Neither
   app nor appex may show any `temporary-exception` entitlement other than
   Sparkle's `-spks`/`-spki`, which are present only when the updater is built.
2. `launchctl print gui/$(id -u)/<broker id>` exits 0.
3. The system log contains the broker's `Self-check passed` line and **no**
   `listener failed to activate` for that name.
4. Badges and the Nextcloud context menu appear on a classic sync folder. Click
   **File actions** and **Apply labels** specifically — both used to render and
   then fail silently.
5. Repeat over: fresh install; upgrade over the previous version (exercises the
   broker version check and re-registration); login item disabled in System
   Settings (expect the disabled “not connected” menu item and a clear log line,
   not silence); client quit and relaunched; `killall Finder`; full reboot; and
   two user accounts on one machine.

## Implementation references

- Broker login item (Swift), including the startup self-check:
  [`shell_integration/MacOSX/NextcloudIntegration/FinderSyncBroker/`](../shell_integration/MacOSX/NextcloudIntegration/FinderSyncBroker/)
- Broker XPC contract, and why a broker is needed at all:
  [`shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncBrokerProtocol.h`](../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncBrokerProtocol.h),
  [`…/FinderSyncBrokerClientProtocol.h`](../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncBrokerClientProtocol.h)
- Extension XPC client, endpoint collection and reconnect logic:
  [`shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/FinderSyncXPCManager.m`](../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/FinderSyncXPCManager.m)
- Extension principal object (badges, menus, bounded menu wait):
  [`shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/FinderSync.m`](../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/FinderSync.m)
- Shared FinderSync protocol (`performHandshakeWithReply:`):
  [`shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncAppProtocol.h`](../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncAppProtocol.h)
- Client-side anonymous listener, endpoint publishing and service:
  [`src/gui/macOS/findersyncxpc_mac.mm`](../src/gui/macOS/findersyncxpc_mac.mm),
  [`src/gui/macOS/findersyncservice.mm`](../src/gui/macOS/findersyncservice.mm)
- Login item registration and status reporting:
  [`src/gui/macOS/findersyncbrokerregistrar_mac.mm`](../src/gui/macOS/findersyncbrokerregistrar_mac.mm)
- Identifiers all three bundles must agree on, and their unit test:
  [`src/gui/macOS/findersyncbrokeridentity.cpp`](../src/gui/macOS/findersyncbrokeridentity.cpp),
  [`test/testfindersyncbrokeridentity.cpp`](../test/testfindersyncbrokeridentity.cpp)
- Build, install and signing of the login item:
  [`shell_integration/MacOSX/CMakeLists.txt`](../shell_integration/MacOSX/CMakeLists.txt),
  [`admin/osx/mac-crafter/Sources/Utils/Signer.swift`](../admin/osx/mac-crafter/Sources/Utils/Signer.swift)
- Installer registration of the extension:
  [`admin/osx/post_install.sh.cmake`](../admin/osx/post_install.sh.cmake)
