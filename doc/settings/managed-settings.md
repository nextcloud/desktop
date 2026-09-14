<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Managed settings
Managed settings let a setting be resolved across device enforced policy, server
enforced policy, user config, server defaults, device defaults and the builtin
default.

Auto update, proxy, folder limit and virtual files keys now resolve through
`ConfigFile::getConfig`. It is the only public enforcement aware function.

## Principle
`ConfigFile::getConfig` walks the full hierarchy and returns the effective value and 
metadata - the source of the setting and if it is enforced.

## API (on ConfigFile)
    // Resolved read: value plus source and enforcement metadata.
    ResolvedSetting getConfig(const QString &name, const QVariant &builtinDefault = {},
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
- `ConfigFile::getConfig` looks up the settings definition, collects the value each source offers
  for it - device policy, server, user config, defaults, and returns the highest
  priority one with its source and enforcement state. The `getConfig<T>`
  template returns any type.
- `ConfigFile::setConfig` resolves first: if the effective value is enforced it returns false
  and writes nothing, so a user can never override an enforced value. Otherwise it
  writes the user config.
- `ConfigFile::isEnforced` and `ConfigFile::sourceOf` read the resolved metadata. 
  The settings UI disables a control when `ConfigFile::isEnforced` is true and 
  shows who set it from `ConfigFile::sourceOf`. Enforced settings are disabled, 
  never hidden - a server default stays editable.
- Update and proxy keys are default only from the server. Only device policy can
  enforce them, so a server cannot disable updates or reroute traffic.

## The schema
The schema is the declared contract for managed settings. Each entry is one
`SettingDefinition`:
```cpp
struct SettingDefinition {
    QString key;              // the setting name
    QVariant builtinDefault;  // value when no source has one, and it fixes the type
    bool enforceable = false; // may a policy force it?
    SettingScope scope = SettingScope::User; // Device, User, Account, Folder
};
```

- `ManagedSettingsSchema::all()` lists them and find(key) looks one up.
- *a non enforceable key ignores every enforced source*, so no registry key, managed plist or server can enforce it.
- `builtinDefault` is the fallback value and it also fixes the type, so a registry or
plist string is coerced to a real bool or int.
- *A key that is not declared in the schema still resolves*, but it is treated as
*not enforceable*. Declaring a key in the schema is how you allow it to be 
enforced (enforceable = true), give it a fixed default and type, or have 
it appear in the `ManagedSettings::resolveAll` diagnostics.
- *There are two separate enforceable flags*. The schema enforceable above decides
whether any enforced source (device or server) is honored. A second flag,
serverEnforceable in servermanagedsettings, decides whether the server specifically
may enforce a key. That is why update and proxy keys are serverEnforceable false (a
server may only default them) yet still enforceable by device policy, whose schema
enforceable is true.

## Sources and the resolver
- **source**: it is the place a value can come from: a registry key, a managed plist, 
the server settings, the user .cfg. It reads the value live and tags it with its kind 
(where it came from), an enforcement state and a priority.  
- **resolver**: it gathers the sources for a key and keeps the highest priority
one that has a value, subject to the enforceable option.

### Device sources by platform
| Platform | Source | Kind | Enforcement | Priority |
|---|---|---|---|---|
| Windows | `HKCU\Software\Policies\<vendor>\<app>` | PlatformPolicy | Enforced | 210 |
| Windows | `HKLM\Software\Policies\<vendor>\<app>` | PlatformPolicy | Enforced | 200 |
| Windows | `HKLM\Software\<vendor>\<app>` | PlatformDefault | NotEnforced | 20 |
| macOS | managed preferences (MDM) | PlatformPolicy | Enforced | 200 |
| macOS | `/Library/Preferences/<domain>.plist` | PlatformDefault | NotEnforced | 20 |
| Linux | `<sysconfdir>/<app>/policies.conf` | PlatformPolicy | Enforced | 200 |
| Linux | `<sysconfdir>/<app>/<app>.conf` | PlatformDefault | NotEnforced | 20 |

The user config contributes at priority 50 (with the legacy top level at 49), and
the server settings contribute a server enforced source at 100 and a server default
at 30. The full precedence is shown in the resolution flow diagram below.

## Managed keys
Every managed key, its type, whether the server may enforce it (not only default
it), and whether it is declared in the schema:
| Key | Type | Server may enforce | In schema |
|---|---|---|---|
| `skipUpdateCheck` | bool | no, default only | yes |
| `autoUpdateCheck` | bool | no, default only | yes |
| `confirmExternalStorage` | bool | yes | yes |
| `useNewBigFolderSizeLimit` | bool | yes | yes |
| `notifyExistingFoldersOverLimit` | bool | yes | yes |
| `newBigFolderSizeLimit` | int, MB | yes | yes |
| `stopSyncingExistingFoldersOverLimit` | bool | yes | yes |
| `virtualFilesMode` | string | yes | yes |
| `proxyType` | int | no, default only | yes |
| `proxyHost` | string | no, default only | yes |
| `proxyPort` | int | no, default only | yes |

- **Out of scope**: Secrets (`proxyPass`) and pure runtime state (`geometry`, `lastSelectedAccount`) are
not managed and kept only in the config file.
- **Device policy can set any key name**, but only these keys actually take effect.
`newBigFolderSizeLimit` and `stopSyncingExistingFoldersOverLimit` carry a runtime or
theme default. They are still declared in the schema so they can be enforced, and
the caller supplies the default value.
- **Enforcement is opt in**: a key can only be enforced if it is declared in the
schema, so a policy can not enforce a setting the client did not declare.
- **Proxy resolves through getConfig as well**, wrapped in
`ConfigFile::managedProxySettings`, which reads proxyType, proxyHost and proxyPort
together so the type, host and port are always managed as one tuple.
- **`virtualFilesMode` resolves through `ConfigFile::managedVirtualFilesMode`**. It is per
folder - `FolderDefinition`, so the value is applied where a new folder is created:
the add folder wizard preselects and, when enforced, disables the virtual files
checkbox, and the account setup wizard forces or hides the virtual files sync mode
the same way. The server may enforce it, so an enforced value can come from the
server or from device policy.
- **Legacy keys are stored in the general group in the config file** while managed writes use the
account group, so getConfig reads both: the account group at priority 50 and the
general group at 49, the group value winning when both exist.
- **Server delivered values are validated** in `sanitizeServerManagedSettings` (the folder size 
limit and `virtualFilesMode`) - invalid values are dropped.
- **Setters refuse an enforced write**: the folder limit setters go through `ConfigFile::setConfig` and
`Account::setProxySettings` refuses a managed proxy write. 
- **UI options are disabled and display a message** explaining the setting is enforced by a system 
administrator or by the organization (for auto update, folder limit and proxy settings).

### Proxy unifies the two layers
- The network dialog edits per account state (`Account`) while the resolver reads the 
managed proxy keys, so `ConfigFile::managedProxySettings` resolves type, host and port as one 
tuple and `AccountManager` applies it at account load: an enforced value always wins, 
a default only when the account follows the system proxy. 
- `Account::proxySettingsAreManaged` is set when the proxy is enforced,
`Account::setProxySettings` refuses a write while managed, and NetworkSettings
disables the editor and shows the managed label. Any enforced field disables the
whole editor, but only the managed fields replace values, so an account keeps its
own value for the rest.
- `ConfigFile::managedProxySettings` is a policy overlay:  it reads the policy keys 
`proxyType`, `proxyHost` and `proxyPort`, while the user's  own proxy stays in the 
account or the legacy `Proxy/type` storage, so the two never collide. The server can 
only default the proxy, never enforce it, so an enforced proxy always comes from
device policy.
- A managed proxy applies at account load, so a *policy change takes effect on reconnect
or restart* - there is no live reapply on capability refresh.

## Nextcloud instances without an enterprise subscription
Server managed settings are an enterprise feature. *The support app returns no
desktopClient capability for an account without a valid subscription*, and *the client
drops any account that is not subscribed*: `AccountManager::updateServerManagedSettings`
merges only subscribed accounts, so with none subscribed the server managed cache is
cleared.

For such instance's users the server layer is removed: no server defaults, no server enforced
values, no UI options disabled. Everything else will work the same and in the same order 
of priority: `ConfigFile::getConfig` still resolves through *device policy, user config and 
the builtin default*, so device policy (Windows registry, macOS managed preferences)
still enforces settings, since that is local OS policy and independent of the subscription. 

If a user loses the subscription, the next refresh drops the cached server settings.

## Testing
`test/testmanagedsettings.cpp` covers the resolver priority order, `ConfigFile::getConfig` and
`ConfigFile::setConfig`, sanitize including the `virtualFilesMode` validation, the schema, 
the proxy per field merge and the global proxy honoring a managed default, and 
`virtualFilesMode` default versus enforced. 

The `Account::setProxySettings` write guard is not covered there because `Account::create` 
self references and trips the leak checker, it is owed in a non ASAN test.

## Resolution
How a managed setting is resolved - e.g. `virtualFilesMode`, which is server
enforceable - update and proxy keys never take the server enforced step:

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
  ManagedSettings::resolve(definition)
  |   the highest priority source that has a value wins:
  |
  device enforced (200) > server enforced (100) > user (50)
                      > server default (30) > device default (20) > builtin
  |
  ResolvedSetting { value, source, enforced/default }     [return]
```

## Server managed settings
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
