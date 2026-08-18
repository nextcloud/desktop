<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Local Network Permission Handling

## Purpose

On macOS 15 and later, the user can deny an application access to devices on
the local network. A connection attempt affected by this setting otherwise
looks much like an unreachable server to the desktop client. The local network
permission check lets the client replace a generic connection error with an
actionable message:

> Local Network access is disabled. Enable it in System Settings → Privacy &
> Security → Local Network.

This check is diagnostic rather than proactive. It runs after a server
connection has already failed or timed out. It does not request permission and
does not report a global permission state. Apple does not provide a general API
for querying that state; instead, the check observes the path of a connection
to the server the user entered. See
[TN3179: Understanding local network privacy](https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy).

The implementation was introduced in response to
[nextcloud/desktop#10452](https://github.com/nextcloud/desktop/issues/10452).

## Architecture

### Platform boundary

`LocalNetworkPermission` exposes two functions from
`src/gui/localnetworkpermission.h`:

- `checkDeniedForConnection()` reports through a callback whether local network
  permission denied a specific failed connection.
- `deniedError()` returns the platform-appropriate error shown to the user.

CMake selects the implementation:

- On macOS, `src/gui/macOS/localnetworkpermission.mm` uses Network.framework.
- On other platforms, `src/gui/localnetworkpermission.cpp` reports `false`,
  preserving the existing connection error.

This keeps platform conditionals out of `ConnectionValidator` and
`AccountWizardController`. A `false` result means that local network denial was
not established; it does not prove that permission is enabled.

### macOS connection probe

The macOS implementation is available on macOS 15 and later. It performs the
following steps:

1. Extract the host and port from the failed URL. The default port is `443` for
   HTTPS and `80` otherwise.
2. Create a Network.framework TCP connection to that endpoint. The probe does
   not perform an HTTP request or a TLS handshake.
3. Set `prefer_no_proxy` on the connection parameters. This makes
   Network.framework try the direct path first, so a VPN-provided local proxy
   cannot immediately hide the local-network denial. Network.framework may
   still try a configured proxy if the direct attempt fails.
4. Observe connection path and state updates on the main dispatch queue.
5. Finish with `true` when an unsatisfied path reports
   `nw_path_unsatisfied_reason_local_network_denied`.
6. Finish with `false` when the connection becomes ready.
7. On a `waiting` or `failed` state, finish only if the current path explicitly
   reports local-network denial. A path can be temporarily inconclusive, so
   completing with `false` at this point would introduce a race with a later
   path update.
8. After two seconds, inspect the path once more and finish. This bounds the
   diagnostic delay when the server is merely absent or unreachable.

`ConnectionProbe` owns the Network.framework connection and callback. Its
`completed` flag ensures exactly-once completion. Finishing cancels and
releases the connection before dispatching the result back through Qt.

The callback context is held as a `QPointer<QObject>`. The result is queued onto
that context and is discarded if the context has been destroyed, preventing a
callback into a deleted controller or validator.

### Consumers

`ConnectionValidator` invokes the check when:

- the status request fails; or
- its connection job times out.

When denial is established, the permission message replaces the generic
network error. The validator's status value is unchanged.

`AccountWizardController` invokes the check after its server connection fails.
When denial is established, it displays the permission message and does not
offer secure-connection recovery, such as retrying without TLS. Otherwise, the
existing recovery flow continues.

The wizard also verifies that the account URL still matches the URL whose
probe completed. This prevents a delayed result from an earlier attempt from
changing the state of a newer attempt.

### Test seam

Both consumers store the permission check in a private `std::function`,
initialized to `LocalNetworkPermission::checkDeniedForConnection`. Their test
access classes are friends and replace that callable with a synchronous
deterministic result.

This keeps the production constructors and public API unchanged. It also tests
the behavior of each consumer without subclassing production classes or
making one-line methods virtual solely for tests.

## Test plan

### Automated coverage

`AccountWizardControllerTest` covers:

- An invalid URL produces a non-denied result.
- A denied result displays `deniedError()` and suppresses secure-connection
  recovery.
- A non-denied result preserves the secure-connection recovery flow.

`ConnectionValidatorTest` covers:

- A denied result replaces the generic timeout text with `deniedError()`.

Build and run the focused tests from the repository root:

```sh
cmake -S . -B build-testing
cmake --build build-testing \
    --target AccountWizardControllerTest ConnectionValidatorTest
ctest --test-dir build-testing --output-on-failure \
    -R '^(ConnectionValidator|AccountWizardController)Test$'
```

### Manual coverage

The automated tests do not cover:

- macOS Local Network privacy enforcement or its System Settings toggle;
- Network.framework path-update ordering and unsatisfied reasons;
- behavior with a real VPN or system proxy;
- the direct-path preference and proxy fallback;
- code-signing identity and executable UUID tracking;
- differences between launching from Finder, Terminal, Xcode, or another
  parent process;
- the complete two-second probe against a real network.

These behaviors depend on operating-system privacy state, routing, signing,
and the active network environment. Checking only that a Network.framework
parameter was set would test an implementation detail, not the intended VPN
behavior. The native path therefore requires the manual regression test below.

## Reproducing local-network denial

### Requirements

- macOS 15 or later.
- A validly signed application with a stable Apple-issued identity.
- A unique UUID in the main executable.
- `NSLocalNetworkUsageDescription` in the application `Info.plist`.
- A target address on a network directly attached through Wi-Fi or Ethernet.
  A private address routed elsewhere is not necessarily a local-network
  address for this privacy feature.

The target does not need to run a Nextcloud server. Using an unused address on
the directly attached subnet is useful because it isolates privacy diagnosis
from server behavior.

Verify the application before testing:

```sh
codesign --verify --deep --strict --verbose=2 /path/to/Nextcloud.app
/usr/bin/dwarfdump --uuid /path/to/Nextcloud.app/Contents/MacOS/Nextcloud
```

Both commands must succeed, and the UUID output must not be empty.

### Launch the application correctly

Quit all running instances and launch the tested application by
double-clicking its bundle in Finder.

Do not start the executable directly from Terminal or SSH. macOS automatically
allows local-network access for command-line tools launched from those
environments and for their child processes. In that situation,
Network.framework can report the connection as ineligible for privacy
enforcement even though the application's Local Network toggle is disabled.
This produces a false-negative test.

For the same reason, Finder launch is preferred for this regression test over
developer launch mechanisms whose responsible process may affect privacy
attribution.

### Test procedure

1. Determine the Mac's Wi-Fi or Ethernet address and subnet.
2. Choose an unused address on that same directly attached subnet.
3. Open **System Settings → Privacy & Security → Local Network**.
4. Disable Local Network access for Nextcloud.
5. Quit Nextcloud, then launch the tested app bundle from Finder.
6. In the account wizard, enter the unused address, for example
   `https://192.168.0.64`.
7. Start the connection.

Expected result:

- The wizard reports that Local Network access is disabled.
- It does not show the secure-connection recovery dialog.

Repeat with a VPN active. The expected result is the same. The permission probe
should try the direct local route before a VPN-provided proxy can handle the
connection.

As a comparison, enable Local Network access and repeat. Because the chosen
address has no server, the result should now be an ordinary timeout or
connection failure rather than the permission message.

## Troubleshooting

### The result is a timeout or TLS recovery dialog

Confirm all of the following:

- The application was launched from Finder, not by executing its binary in a
  shell.
- The tested bundle has a valid signature.
- The running process belongs to the bundle just verified.
- The target address is on a directly attached Wi-Fi or Ethernet subnet.
- The Local Network toggle for the tested application is disabled.

### Inspect Network.framework activity

The following command reads the relevant unified logs for a bounded test
interval:

```sh
/usr/bin/log show \
    --start '2026-07-27 22:20:45' \
    --end '2026-07-27 22:22:35' \
    --style compact --info --debug \
    --predicate 'process == "Nextcloud" AND subsystem BEGINSWITH "com.apple.network"'
```

Replace the timestamps with the actual test interval. Useful evidence includes:

- `prefer no proxy`, confirming that the direct path preference is active;
- `local network denied` or an unsatisfied local-network-denial reason;
- a proxy endpoint such as `127.0.0.1`, showing proxy fallback;
- `Privacy Stance: Not Eligible`, which indicates that the operation was not
  subject to normal Local Network privacy enforcement and commonly points to
  the launch or identity conditions described above.

### VPN interpretation

A VPN can install a system proxy even when the local target remains routed over
Wi-Fi. `prefer_no_proxy` means “try direct first,” not “prohibit all proxies.”
Seeing a later proxy attempt is therefore expected when the direct attempt
fails. What matters for this feature is that macOS has an opportunity to
evaluate the direct local path and report denial before proxy fallback masks
the original condition.
