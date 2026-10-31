<!--
SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Configuration migration

`OCC::Migration` (`migration.h` / `migration.cpp`) drives the one time upgrade of a
client's on disk configuration when the running binary no longer matches the config
that is already on the machine. It handles three situations:

* **Version change** the config was written by an older or newer client than the one
  running now (upgrade or downgrade).
* **Legacy import** an old ownCloud/Nextcloud config exists in one of the historical
  locations and its accounts should be pulled into the current config.
* **Unbranded to branded** an unbranded Nextcloud config is adopted by a branded build.

## Design

`Migration` is a static helper, not an object. All of its state is process global and
lives in static members, so the constructor is deleted (`Migration() = delete;`) and
every method is static. This mirrors how the migration actually behaves: there is one
config per process and one migration in flight at a time.

State held between phases:

| Member | Meaning |
| --- | --- |
| `_phase` | how far the migration has progressed |
| `_brandingType` | branding relationship between old and new config |
| `_upgradeType` | version relationship (upgrade, downgrade, no change) |
| `_discoveredLegacyConfigPath` | path of the legacy config, once found |

`resetForTesting()` clears all of it and exists only for unit tests.

## Phases

The migration is a monotonic state machine. `setPhase()` only ever moves forward, so a
later stage can never roll the phase back:

```cpp
void Migration::setPhase(const Phase phase)
{
    if (phase > _phase) {
        _phase = phase;
    }
}
```

```
NotStarted -> SetupConfigFile -> SetupUsers -> SetupFolders -> Done
```

`isInProgress()` is true for any phase between `SetupConfigFile` and `SetupFolders`
inclusive, i.e. not `NotStarted` and not `Done`.

## Version detection

Two versions are compared:

* `currentVersion()` the running binary, from `MIRALL_VERSION_STRING`.
* `configVersion()` the version recorded in the config file, from
  `ConfigFile().clientVersionString()`.

Derived predicates:

| Predicate | Definition |
| --- | --- |
| `isUpgrade()` | `currentVersion() > configVersion()` |
| `isDowngrade()` | `configVersion() > currentVersion()` |
| `versionChanged()` | `configVersion() != currentVersion()` |
| `shouldTryToMigrate()` | `versionChanged()` |

`shouldTryToMigrate()` is the single gate the application checks at startup before doing
any migration work.

## Legacy discovery

`legacyData()` is pure discovery: it looks for a readable legacy config and returns it,
without mutating any shared state.

* Return type is `std::unique_ptr<QSettings>` (`Migration::LegacyData`). Ownership moves
  to the caller; there is no stored copy.
* It probes the historical config locations in order (ownCloud 2.4 layout, 2.5+ layout,
  the current layout, and, for branded builds, the unbranded Nextcloud paths).
* The first location that exists and is readable wins; unreadable candidates are skipped
  with `continue`, not `break`, so a bad entry does not stop the search.
* A `nullptr` result means no legacy config was found.

Persisting what was discovered happens **after** the user confirms the import, in
`AccountManager::restoreFromLegacySettings`, not inside `legacyData()`. Only once an
account is accepted does the caller record the source:

```cpp
const QFileInfo legacyConfigInfo(oCSettings->fileName());
Migration::setDiscoveredLegacyConfigPath(legacyConfigInfo.canonicalPath());
ConfigFile().setClientPreviousVersionString(oCSettings->value(ConfigFile::clientVersionC).toString());
```

This keeps discovery side effect free: probing for a legacy file never changes the
config unless the user actually opts in.

## Branding

`brandingType()` records the relationship between the old and new config
(`UnbrandedToUnbranded`, `UnbrandedToBranded`, `LegacyToUnbranded`, `LegacyToBranded`).

Two predicates drive branded builds adopting an unbranded config:

* `shouldTryUnbrandedToBrandedMigration()` a forward looking check used during
  `SetupFolders`: we are at that phase, this is a branded build, and a legacy config path
  was discovered.
* `isUnbrandedToBrandedMigration()` true while a migration is in progress, a legacy path
  was discovered, and this is a branded build. `ConfigFile` uses it to read from the
  unbranded app group so branded builds pick up unbranded settings.

Both require a non empty `_discoveredLegacyConfigPath`, so neither fires on a plain
version upgrade with no legacy import.

## Key call sites

| Location | Role |
| --- | --- |
| `gui/application.cpp` `configVersionMigration` | entry gate; sets `SetupConfigFile`, backs up config, warns the user |
| `gui/application.cpp` `setupAccountsAndFolders` | advances `SetupUsers` / `SetupFolders` |
| `gui/accountmanager.cpp` `restoreFromLegacySettings` | consumes `legacyData()`, confirms import, persists discovered path |
| `gui/folderman.cpp` `setupFolders` | runs folder migration when a legacy path is known |
| `gui/accountstate.cpp` `slotCredentialsFetched` | marks the migration `Done` |
| `libsync/configfile.cpp` | reads from the unbranded group during unbranded to branded migration |
| `cmd/cmd.cpp` | drives the phases for the headless client |

## Testing

`test/testmigration.cpp` covers the state machine and predicates in isolation via
`resetForTesting()`: phase monotonicity, `isUpgrade` / `isDowngrade` / `versionChanged`,
`shouldTryToMigrate` across upgrade, downgrade and no change, `isInProgress` per phase,
and that `legacyData()` leaves `discoveredLegacyConfigPath()` empty (discovery has no side
effects).

## Flow diagrams

### Migration

The flow reads top down. A `|` is the main path, a `|--` is a branch that leaves it, and
the tag on the right marks the start and end.

```text
Application::configVersionMigration                        [start]
  |-- shouldTryToMigrate == false -------------------> Phase::Done
  |
  Phase::SetupConfigFile
  |   applyMigrationDefaults, backupConfigFiles
  |   remove incompatible keys, warn user
  |
Application::setupAccountsAndFolders
  |
  Application::restoreLegacyAccount -> AccountManager::restore
  |-- accounts already in current config ------------> load accounts
  |
  AccountManager::restoreFromLegacySettings
  |
  Migration::legacyData
  |   search legacy config locations
  |-- no legacy config found ------------------------> return
  |
  found and user confirms import
  |   setDiscoveredLegacyConfigPath
  |   setClientPreviousVersionString
  |
  Phase::SetupUsers
  |
  FolderMan::setupFolders
  |-- legacy path known -> setupFoldersMigration
  |
  load folder definitions
  |
AccountState::slotCredentialsFetched                       [end]
  |
  Phase::Done
```
