<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Documentation asset capture

`DocAssetsCapture` is a standalone macOS developer executable. It uses the existing
client build's libraries and production windows without changing client startup or
shipping capture code in the application bundle. It writes PNG screenshots and icons into a
new `~/Downloads/Nextcloud-doc-assets-<UTC time>-<UUID>` folder. It does not write a
manifest. Qt's test-mode settings isolation does not redirect this output folder.

## Run from Xcode

Select **Nextcloud Documentation Assets**, build that scheme, then Run in a
logged-in desktop session. Windows appear briefly and close automatically. The
console prints each scenario and the final output directory, or a named failure.

If the local scheme has not been created, run this from the checkout:

```sh
python3 test/docassets/setup_xcode.py
```

Reopen the workspace if Xcode has not refreshed its scheme list. The generated
scheme uses an absolute executable path in ignored `xcuserdata`; regenerate it
if the checkout moves. The separate external target invokes
`test/docassets/compile.sh`, which builds the capture executable and any stale
shared libraries in the existing CMake build directory. It does not build, sign,
package, or install the normal client app or its extensions. It does not download
dependencies or create a second build tree.

The default build directory is
`build/macos-clang-<native architecture>/build/nextcloud-client/work/build`.
A configured client build and its dependencies must already exist. For a custom
build location, set `DOC_ASSETS_BUILD_DIR` on the **DocAssetsCapture** target and
select that directory's `bin/DocAssetsCapture` as the scheme's Run executable.
Use an actual absolute executable path, not a build-setting expression.

For shell use:

```sh
./test/docassets/compile.sh
build/macos-clang-arm64/build/nextcloud-client/work/build/bin/DocAssetsCapture
```

Optional arguments:

```sh
/path/to/build/bin/DocAssetsCapture --list
/path/to/build/bin/DocAssetsCapture --output /absolute/new/folder
```

An explicit destination must not exist, and its parent must exist. Every scenario
runs in a fresh worker process. All captures are staged in a temporary sibling
folder and decoded as PNGs before the folder is published. A required failure
does not stop subsequent scenarios: every screen is attempted, all failures are
reported together, and the run returns nonzero. Completed assets are retained in a visible folder whose name ends
with `-incomplete-<UUID>`, and the console reports its path. No incomplete folder is
created if no asset succeeded. Existing output is never replaced.
No documentation repository operations are performed.

## Catalogue

The exporter renders 18 bundled SVG resources as transparent 256×256 PNGs: sync status,
monochrome tray, and colored tray icons for ok, sync, pause, warning, error, and
offline. Filenames use the prefixes `icon-status-`, `icon-tray-`, and
`icon-tray-colored-`. Extension badges are not generated.

Scenario identifiers and filenames are centralized in `test/docassets/catalogue.cpp`.
Each filename is its scenario identifier followed by `.png`.

| Area | Scenarios |
| --- | --- |
| Wizard | `wizard-server`, `wizard-browser-auth`, `wizard-sync-classic`, `wizard-advanced-options`, `wizard-proxy-settings`, `wizard-client-certificate`, `wizard-secure-connection` |
| File Provider wizard | `wizard-sync-file-provider`, only when supported by the build and macOS version |
| Search | `search-results` |
| Sharing | `sharing-overview`, `sharing-details`, `sharing-advanced-settings` |
| Assistant | `assistant-chat` |
| User Status | `user-status` |
| Whole Settings dialog | `settings-account-classic`, `settings-general`, `settings-advanced`, `settings-info` |
| Advanced Settings dialog | `settings-ignored-files` |
| Activities | `activities`, including a conflict and warning status |

Settings captures the whole production dialog with its navigation. The classic
account scenario supplies Alex Morgan, one synced folder, and 12 GB of 100 GB used,
without registering the fixture account or starting folder/platform services. Small constructor injection points supply platform operations and an
unavailable updater; the normal client retains its production defaults. Activities
uses the production list model with an injected account lookup and sync summary.
The Ignored Files scenario captures the production editor opened from Advanced
Settings, with two sample patterns and no folder service startup.
Sharing captures the production sharing window with fixture shares for a project
document. The three states show its overview, recipient and permission details,
and advanced expiration and password settings without Finder integration or a server.

The console explicitly reports these omissions:

- File Provider Account Settings and activity file actions remain omitted.
- File Provider enable/disable confirmations are not yet catalogued.
- Selective folder selection, per-folder ignored files, and folder setup: these paths
  initialize `FolderMan` and shell integration.
- Assistant task selection: the production selector is deliberately hidden.
- Optional native tray window capture remains omitted.
- Browser-hosted login pages, macOS file pickers, and OS-owned File Provider
  indicators are outside the capture scope. No invented wizard completion screen
  is supplied for `CompletedStep`.


## Isolation and readiness

The executable does not construct `OCC::Application`, restore accounts, or start
sync, keychain operations, domain reconciliation, the updater, or shell integration.
It is excluded from the default build unless capture tests are explicitly enabled,
and has no install target. Its QML registrations
and image provider setup are local; production layouts and classes are reused.

Each worker uses temporary configuration and shader-cache paths, disabled QML disk
caching, English text, light appearance, and synthetic identities. Fixture account
objects are never registered in `AccountManager`. The existing `FakeAccountState`
and `FakeAssistantClient` seams are reused without linking Qt Test into the runner.
Wizard states use existing test friendships. Search responses come from a local
network manager that never forwards to the network; unknown requests fail locally.
The production tray image provider receives bundled images through a fixture adapter
that maps only the two named avatar URLs to a local resource. The QML network
manager rejects all requests.
Physical input and external HTTP/HTTPS URL opening are blocked while capturing.

Readiness checks controller state, loaded images, exposed windows, stable size, and
three subsequent rendered frames. Popup scenarios wait for the actual popup to
open. Overlays are captured with their parent; a popup rendered in a separate
window is captured through that window. QML uses `QQuickWindow::grabWindow()`;
Settings windows use `QWidget::grab()` after layout settles. Dimensions come from
production defaults and rendering uses native device scale. Workers have bounded
startup/capture timeouts, and diagnostics include the failed scenario.

The existing `WizardTextField.qml` and `EmojiPicker.qml` native-background
customization warnings are allowed narrowly. Other QML warnings remain fatal. Allowing this warning does not
establish visual correctness; inspect native controls and popup composition.

## Validation

On 2026-09-21, the capture target built and a complete export produced 20
screenshots and 18 PNG icons, including all three sharing states. The sharing
screenshots were visually inspected. The custom GET fixture regression passed.
The current catalogue contains 19 screenshots, or 20 when File Provider is available.
After the final review corrections, the capture suite passed with 27 passed,
0 failed, and 0 skipped, including the full catalogue export, multi-child teardown,
and warning-filter regressions.
`run-clang-tidy` is unavailable on the validation machine.

`DocAssetsCaptureTest` includes production-window capture for every catalogue entry
in separate workers, PNG decoding, flat output, icon rasterization, unique names, repeated
server capture dimensions, missing resources, cancellation, timeout, output
preservation, rejected requests, fixture response/cancellation handling, and Search
filter request checks. Existing native-warning and Xcode-argument regression
coverage remains. Added tests exercise whole Settings page selection, injected
autostart operations, production activity roles, supplied sync states, and lazy
creation of the default QML sync summary. Sharing fixture coverage checks all three
states, their production module, file identity, and local response data. Tests require a logged-in macOS desktop.

The test target requires `BUILD_DOC_ASSETS_TESTS=ON`, independently of
`BUILD_TESTING`. Enabling it includes the capture test and its dependencies in the
default build. Tests require a logged-in macOS desktop. Commands for repeating validation:

```sh
cmake -S . -B /path/to/existing/build -DBUILD_DOC_ASSETS_TESTS=ON
cmake --build /path/to/existing/build --target DocAssetsCaptureTest
ctest --test-dir /path/to/existing/build --output-on-failure -R '^DocAssetsCaptureTest$'
python3 -m unittest discover -s test/docassets -p 'test_*.py'
```

Inspect one complete export for correct states, clipping, native control rendering,
English wording, light appearance, popup composition, and absence of OS borders or
shadows. A successful source check is not runtime or visual acceptance. Discuss the
catalogue and isolation boundaries with maintainers before proposing this multi-area
feature for inclusion. Keep review slices focused: runtime and wizard, then Search
and Assistant, then whole Settings and Activities.
