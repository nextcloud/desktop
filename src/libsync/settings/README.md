<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Settings

This folder holds the client settings subsystems: managed settings, documented
below, and migration, documented in MIGRATION.md.

# Managed config: getConfig gateway design

## Context

Issue #5497 introduced a managed settings resolver so a setting can be resolved
across device enforced policy, server enforced policy, user config, server
defaults, device defaults and the builtin default. Today only skipUpdateCheck and
autoUpdateCheck go through it (via ConfigFile::getConfig). Every other
setting is still read with ConfigFile::getValue (OS default plus user config
only), ConfigFile::getPolicySetting (Windows policy overlay) or raw QSettings.

Those three read paths skip the enforcement hierarchy. Any setting read through
them cannot be enforced by an administrator. To make every setting manageable we
need one enforcement aware read path that all config access flows through.

## Principle

There is exactly one way to read a managed setting: getConfig. It walks the full
hierarchy and returns the effective value plus metadata (source, enforcement).
getValue and getPolicySetting become internals of the source adapters and are
removed from the public read surface. No managed key is read with raw QSettings.

We do not move away from ConfigFile. ConfigFile becomes the enforcement gateway;
getConfig is its single read entry point.

## API (on ConfigFile)

    // Resolved read: value plus source and enforcement metadata.
    ManagedValue getConfig(const QString &name, const QVariant &builtinDefault = {},
                           const QString &connectionGroupName = {}) const;

    // Typed convenience over getConfig().value.
    template<typename T>
    T getConfig(name, group = {}) const;

    // Write to the user config; returns false and writes nothing when enforced.
    bool setConfig(name, const QVariant &value, group = {});

    // UI helpers.
    bool isEnforced(name, group = {}) const;             // true means disable the control
    SettingSourceType sourceOf(name, group = {}) const;  // for the "Managed by ..." label

## Behavior

- getConfig finds the SettingSpec in ManagedSettingsSchema, or synthesizes one
  (builtinDefault, enforceable) for keys not yet in the schema, then builds the
  source stack (buildDeviceSources, UserConfigSource for the group,
  buildServerSources from the cached server settings) and resolves. The
  getConfig<T> template returns any type; resolveManagedBool is removed.
- setConfig resolves first: if the effective value is enforced it returns false
  and writes nothing, so a user can never override an enforced value. Otherwise it
  writes the user config.
- isEnforced and sourceOf read the resolved metadata. The settings UI disables a
  control when isEnforced is true and shows who set it from sourceOf. Enforced
  settings are disabled, never hidden; a server default stays editable.
- Update and proxy keys are default only from the server. Only device policy can
  enforce them, so a server cannot disable updates or reroute traffic.

## Scope

First slice (this change):
- Add getConfig, the typed getConfig<T> template, setConfig, isEnforced, sourceOf,
  with tests.
- Re express skipUpdateCheck and autoUpdateCheck on getConfig<T>; remove
  resolveManagedBool.

Incremental follow up (not in this slice):
- Migrate the remaining settings onto getConfig as they are onboarded into the
  schema, retiring their getValue and getPolicySetting use.
- A QML facade (Q_INVOKABLE getConfig/setConfig/isEnforced) for the settings UI,
  landing with the UI enforcement work.
- Disable enforced controls in the settings dialogs.
- A guard against new raw QSettings reads of managed keys.

## Out of scope

Secrets (proxyPass) and pure runtime state (geometry, lastSelectedAccount) are
not managed and keep plain setValue/getValue.

## Testing

Unit tests: getConfig across bool, int and string; setConfig writes when not
enforced and refuses when enforced; isEnforced and sourceOf metadata; the migrated
skipUpdateCheck and autoUpdateCheck resolve identically to before.

## Flow diagrams

### Resolution

How a managed setting is resolved (e.g. virtualFilesMode, which is server
enforceable; update and proxy keys never take the server enforced step):

```text
config.php (admin)                                     [server, optional]
  |
  support app Capabilities::getCapabilities
  |   allow list filter, enterprise subscription gate
  |
  OCS: support.desktopClient { defaults, enforced }
  |
Account::setCapabilities                               [client]
  |
  Account::updateServerManagedSettings
  |   Capabilities::desktopClientManagedSettings then parseServerManagedSettings
  |   sanitizeServerManagedSettings  (client allow list, drops non enforceable)
  |
  AccountManager::updateServerManagedSettings
  |   merge subscribed accounts (the subscribed account wins)
  |
  ConfigFile::setServerManagedSettings   (JSON in .cfg, offline cache)
  |
ConfigFile::getConfig                                  [read]
  |   add the sources for this key:
  |   buildDeviceSources()   Windows GP / macOS forced (enforced), OS default
  |   UserConfigSource       the user .cfg
  |   buildServerSources()   server enforced, server default
  |
  ManagedSettings::resolve(spec)
  |   the highest priority source that has a value wins:
  |
  device enforced (200) > server enforced (100) > user (50)
                      > server default (30) > device default (20) > builtin
  |
  ManagedValue { value, source, enforced/default }     [return]
```

### Server delivery

From the admin's config.php to a source the resolver can read:

```text
[server, support app]                     (separate repo, enterprise gated)
  config.php: desktopclient.defaults / .enforced
    |
  DesktopClientSettingsService   allow list, never secrets
    |
  Capabilities: support.desktopClient { schemaVersion, defaults, enforced }
    |
  OCS  /cloud/capabilities
    |
[client]
  Account::setCapabilities
    |
  Capabilities::desktopClientManagedSettings
    |   parseServerManagedSettings   (capability map into ServerManagedSettings)
    |
  sanitizeServerManagedSettings
    |   client allow list: drop unknown keys,
    |   keep only server enforceable keys in enforced
    |   (update and proxy keys are default only, never server enforced)
    |
  AccountManager::updateServerManagedSettings
    |   merge subscribed accounts, the subscribed account wins
    |
  ConfigFile::setServerManagedSettings   (JSON in .cfg, offline cache)
    |
  buildServerSources
    |   ServerSettingsSource  enforced   (ServerEnforced, priority 100)
    |   ServerSettingsSource  defaults   (ServerDefault, priority 30)
    |
  [added to the resolver by ConfigFile::getConfig]
```
