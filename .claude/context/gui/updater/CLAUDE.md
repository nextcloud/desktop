# src/gui/updater

This folder implements the client's self-update mechanism: periodic checks against an update server, platform-specific handling of the result (silent notification, NSIS/MSI installer download+run on Windows, Sparkle framework on macOS), and parsing of the legacy ownCloud XML update-info format.

## Key classes/components

- `Updater` (`updater.h/.cpp`) — abstract base class/singleton (`Updater::instance()`); builds the update-check URL (`updateUrl()`, from `APPLICATION_UPDATE_URL` plus query params like version/platform/OS/channel/oem), and factory-selects the concrete updater per platform in `create()`. Also has version-integer helpers (`Helper::versionToInt`, `currentVersionToInt`, `stringVersionToInt`).
- `OCUpdater` (`ocupdater.h/.cpp`) — base class using ownCloud's proprietary XML update-info format; performs the network GET, tracks a `DownloadState` state machine (CheckingServer/UpToDate/Downloading/DownloadComplete/DownloadFailed/DownloadTimedOut/UpdateOnlyAvailableThroughSystem), and exposes `statusString()` for UI display.
- `NSISUpdater` (in `ocupdater.h/.cpp`) — Windows-only `OCUpdater` subclass; downloads the `.exe`/`.msi` installer, stores it, and on next startup (`handleStartup`) launches it (via `QProcess`/PowerShell wrapper for MSI) and shows "new version"/"update failed" dialogs.
- `PassiveUpdateNotifier` (in `ocupdater.h/.cpp`) — Linux/fallback `OCUpdater` subclass that only notifies the user of an available update (relies on the OS package manager for actual installation); also detects if the on-disk binary changed under a running process and requests an app restart.
- `SparkleUpdater` (`sparkleupdater.h`, `sparkleupdater_mac.mm`) — macOS `Updater` implementation wrapping the Sparkle framework (`SPUStandardUpdaterController`); `SparkleInterface`/`NCSparkleUpdaterDelegate` bridge Sparkle's Objective-C delegate callbacks to Qt signals (`statusChanged`) and an internal `State` enum (Idle/Working/AwaitingUserInput).
- `UpdaterScheduler` (in `ocupdater.h/.cpp`) — `QObject` that owns the recurring `QTimer`, checks `ConfigFile` for skip/auto-update settings, and calls `Updater::instance()->backgroundCheckForUpdate()`; forwards `newUpdateAvailable`/`requestRestart` signals to the app.
- `UpdateInfo` (`updateinfo.h/.cpp`) — generated (kxml_compiler-style) XML data class parsing `<owncloudclient>` responses into version/versionString/web/downloadUrl fields; used by `OCUpdater`.

## How it fits together

`Updater::instance()` (created via `Updater::create()`) picks `NSISUpdater` (Windows), `SparkleUpdater` or `PassiveUpdateNotifier` (macOS, depending on Sparkle availability/entitlement), or `PassiveUpdateNotifier` (Linux/other). `UpdaterScheduler` drives periodic `backgroundCheckForUpdate()` calls and relays announcements to the GUI. The non-Sparkle updaters all funnel through `OCUpdater`, which fetches XML from the update URL, parses it via `UpdateInfo`, and lets the subclass decide what to do (`versionInfoArrived`).

## Fork-specific notes

- The update server URL is brand-specific and set at CMake configure time — see `IONOS.cmake` (`APPLICATION_UPDATE_URL = https://customerupdates.nextcloud.com/client/`, set identically for both "strato" and "ionos" `WHITELABEL_NAME`, and only inside the `LOCALBUILD` branch — production builds presumably get this from an external CMake cache variable) vs. `NEXTCLOUD.cmake` (`https://updates.nextcloud.org/client/`, set unconditionally), compiled into `config.h` as `APPLICATION_UPDATE_URL` and read in `Updater::updateUrl()`.
- The updater C++ logic itself (`OCUpdater`, `NSISUpdater`, `PassiveUpdateNotifier`, `SparkleUpdater`, `UpdateInfo`) is largely unmodified upstream Nextcloud/ownCloud code; only user-facing strings reference `Theme::instance()->appNameGUI()` for branding.

*Quelle: src/gui/updater — Stand 2026-08-17, automatisch erstellt, bitte gegenlesen.*
