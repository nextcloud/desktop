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
defaults, device defaults and the builtin default. Update, proxy, folder limit and
virtual files keys now resolve through ConfigFile::getConfig. The remaining settings are still
read with ConfigFile::getValue (OS default plus user config only),
ConfigFile::getPolicySetting (Windows policy overlay) or raw QSettings, and are
migrated onto getConfig as they are onboarded into the schema.

Those read paths skip the enforcement hierarchy, so a setting read through them
cannot be enforced by an administrator. getConfig is the single enforcement aware
read path.

## Principle

There is exactly one way to read a managed setting: getConfig. It walks the full
hierarchy and returns the effective value plus metadata (source, enforcement).
getValue and getPolicySetting stay only for keys not yet onboarded, and a wired
managed key is never read with raw QSettings.

We do not move away from ConfigFile. ConfigFile is the enforcement gateway;
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

## The schema

The schema is the declared contract for managed settings. Each entry is one
SettingSpec:

```cpp
struct SettingSpec {
    QString key;              // the setting name
    QVariant builtinDefault;  // value when no source has one, and it fixes the type
    bool enforceable = false; // may a policy force it?
    SettingScope scope = SettingScope::User; // Device, User, Account, Folder
};
```

ManagedSettingsSchema::all() lists them and find(key) looks one up. Three fields do
the work at resolution time.

enforceable is the security gate. A non enforceable key ignores every enforced
source, so no registry key, managed plist or server can lock it:

```cpp
// ManagedSettings::resolve
for (const auto &source : _sources) {
    if (source->enforcement() == EnforcementState::Enforced && !spec.enforceable) {
        continue; // enforced sources do not apply to a non enforceable key
    }
    ...
}
```

builtinDefault is the fallback value and it fixes the type. The resolver coerces the
resolved value to that type, so a registry or plist string becomes a real bool or
int:

```cpp
if (!settingSource) {
    return {definition.key, definition.builtinDefault, SettingSourceType::BuiltinDefault, ...};
}
if (definition.builtinDefault.isValid()) {
    settingValue.convert(definition.builtinDefault.metaType());
}
```

Being in the schema is not required to resolve a key. getConfig synthesizes a
definition for anything not declared, defaulting enforceable to true:

```cpp
const auto spec = ManagedSettingsSchema::find(name)
    .value_or(SettingSpec{name, builtinDefault, /*enforceable*/ true, SettingScope::User});
```

So you add a key to the schema to override those defaults: to make it non
enforceable, to give it a fixed default and type, or to have it appear in the
resolveAll diagnostics.

Note two separate enforceable flags. The schema enforceable above decides whether
any enforced source (device or server) is honored. A second flag, serverEnforceable
in servermanagedsettings, decides whether the server specifically may enforce a key:

```cpp
// sanitizeServerManagedSettings keeps a server enforced value only when allowed
if (policy != validKeys.cend() && policy->serverEnforceable && isServerKeyValueValid(key, value)) {
    serverSettings.enforced.insert(key, value);
}
```

That is why update and proxy keys are serverEnforceable false (a server may only
default them) yet still lockable by device policy, whose schema enforceable is true.

## Sources and the resolver

A source is a small adapter over one place a value can come from. It holds no
values; it reads them live and tags them with a provenance, an enforcement state and
a priority:

```cpp
class SettingSource {
public:
    virtual std::optional<QVariant> read(const QString &key, const QString &group) const = 0;
    virtual SettingSourceType type() const = 0;         // PlatformPolicy, ServerDefault, ...
    virtual EnforcementState enforcement() const = 0;   // Enforced or NotEnforced
    virtual int priority() const = 0;                   // who wins on a conflict
};
```

buildDeviceSources and buildServerSources are factory functions that construct these
adapters for the current environment. The device set depends on the platform and
carries the priorities:

```cpp
// buildDeviceSources, Windows
HKCU\Software\Policies\<vendor>\<app>   PlatformPolicy,  Enforced,    210
HKLM\Software\Policies\<vendor>\<app>   PlatformPolicy,  Enforced,    200
HKLM\Software\<vendor>\<app>            PlatformDefault, NotEnforced,  20
// macOS: MacForcedPreferenceSource(200) + /Library/Preferences/<domain>.plist (20)
// Linux: <sysconfdir>/<app>/<app>.conf (20)
```

```cpp
// buildServerSources, from the cached server settings
if (!sanitized.enforced.isEmpty()) {
    sources.push_back(std::make_unique<ServerSettingsSource>(
        sanitized.enforced, SettingSourceType::ServerEnforced, EnforcementState::Enforced, 100));
}
if (!sanitized.defaults.isEmpty()) {
    sources.push_back(std::make_unique<ServerSettingsSource>(
        sanitized.defaults, SettingSourceType::ServerDefault, EnforcementState::NotEnforced, 30));
}
```

getConfig builds the whole stack fresh on each call, so resolution reflects the
current registry, plist and cfg (which is why a device policy change is seen after
the dialog reopens):

```cpp
ManagedSettings resolver;
for (auto &deviceSource : buildDeviceSources()) {
    resolver.addSource(std::move(deviceSource));
}
resolver.addSource(std::make_unique<UserConfigSource>(configFile(), groupName));      // 50
resolver.addSource(std::make_unique<UserConfigSource>(configFile(), QString(), 49));  // legacy top level
for (auto &serverSource : buildServerSources(serverManagedSettings())) {
    resolver.addSource(std::move(serverSource));
}
return resolver.resolve(definition);
```

resolve then asks every source and keeps the highest priority one that has a value,
subject to the enforceable gate:

```cpp
if (source->priority() > settingPriority) {
    settingSource = source.get();
    settingValue = *value;
    settingPriority = source->priority();
}
```

## Managed keys

Every managed key, its type, whether the server may enforce it (not only default
it), and whether it is declared in the schema:

| Key | Type | Server may enforce | In schema |
|---|---|---|---|
| skipUpdateCheck | bool | no, default only | yes |
| autoUpdateCheck | bool | no, default only | yes |
| confirmExternalStorage | bool | yes | yes |
| useNewBigFolderSizeLimit | bool | yes | yes |
| notifyExistingFoldersOverLimit | bool | yes | yes |
| newBigFolderSizeLimit | int, MB | yes | no, runtime default |
| stopSyncingExistingFoldersOverLimit | bool | yes | no, runtime default |
| virtualFilesMode | string | yes | yes |
| proxyType | int | no, default only | no |
| proxyHost | string | no, default only | no |
| proxyPort | int | no, default only | no |

Device policy can set any key name, but only these are wired to change behaviour.
newBigFolderSizeLimit and stopSyncingExistingFoldersOverLimit carry a runtime or
theme default, so they resolve with that default at the call site and are not in the
schema.

The "In schema" column refers to The schema above: a listed key has a declared
SettingDefinition, an unlisted key resolves through a synthesized definition.

Proxy resolves through getConfig as well, wrapped in
ConfigFile::managedProxySettings, which reads proxyType, proxyHost and proxyPort
together so the type, host and port are always managed as one tuple.

virtualFilesMode resolves through ConfigFile::managedVirtualFilesMode. It is per
folder (FolderDefinition), so the value is applied where a new folder is created:
the add folder wizard preselects and, when enforced, disables the virtual files
checkbox, and the account setup wizard forces or hides the virtual files sync mode
the same way. The server may enforce it, so an enforced value can come from the
server or from device policy.

Legacy keys are stored at the top level of the .cfg while managed writes use the
account group, so getConfig reads both: the account group at priority 50 and the
top level at 49, the group value winning when both exist.

Server delivered values are validated in sanitizeServerManagedSettings (the
folder size limit and virtualFilesMode); invalid values are dropped.

Setters refuse an enforced write: the folder limit setters go through setConfig and
Account::setProxySettings refuses a managed proxy write. UI enforcement (disable and
label) covers the update control, the folder limit controls (advancedsettings) and
the proxy editor (networksettings).

Proxy unifies the two layers. The network dialog edits per account state (Account)
while the resolver reads the managed proxy keys, so managedProxySettings resolves
type, host and port as one tuple and AccountManager applies it at account load: an
enforced value always wins, a default only when the account follows the system
proxy. Account::proxySettingsAreManaged is set when the proxy is enforced,
Account::setProxySettings refuses a write while managed, and NetworkSettings
disables the editor and shows the managed label. Any enforced field disables the
whole editor, but only the managed fields replace values, so an account keeps its
own value for the rest. managedProxySettings is a policy overlay: it reads the policy
keys proxyType, proxyHost and proxyPort, while the user's own proxy stays in the
account or the legacy Proxy/type storage, so the two never collide. The server can
only default the proxy, never enforce it, so an enforced proxy always comes from
device policy.

## Without an enterprise subscription

Server delivery is gated on the enterprise subscription. The support app returns no
desktopClient capability for an account without a valid subscription, and the client
drops any account that is not subscribed: AccountManager::updateServerManagedSettings
merges only subscribed accounts, so with none subscribed the server managed cache is
cleared.

For such a user the server layer is inert: no server defaults, no server enforced
values, no "Managed by your organization" label. Everything else is unchanged.
getConfig still resolves through device policy, user config and the builtin default,
so device policy (Windows registry, macOS managed preferences) still enforces
settings, since that is local OS policy and independent of the subscription. If a
user loses the subscription, the next refresh drops the cached server settings.

## Scope

The feature is complete for the wired keys below. The remaining items are optional
and not required for it.

Done:
- getConfig, the typed getConfig<T> template, setConfig, isEnforced and sourceOf on
  ConfigFile; resolveManagedBool removed.
- Wired keys: skipUpdateCheck, autoUpdateCheck, the folder limit keys, proxy and
  virtualFilesMode.
- Enforced controls disabled with a managed label in the settings dialogs (folder
  limits in advancedsettings, proxy in networksettings) and in the folder wizards
  (virtual files).
- Server delivered values sanitized against the client allow list and validated.
- Unit tests for the resolver, sanitize, the schema, the proxy per field merge and
  virtualFilesMode resolution (test/testmanagedsettings.cpp).

Optional, not required:
- Migrate other settings onto getConfig as they are onboarded into the schema,
  retiring their getValue and getPolicySetting use.
- A QML facade (Q_INVOKABLE getConfig/setConfig/isEnforced) if the settings UI moves
  to QML.
- A guard against new raw QSettings reads of managed keys.

A managed proxy applies at account load, so a policy change takes effect on reconnect
or restart; there is no live reapply on capability refresh.

## Out of scope

Secrets (proxyPass) and pure runtime state (geometry, lastSelectedAccount) are
not managed and keep plain setValue/getValue.

## Testing

test/testmanagedsettings.cpp covers the resolver priority order, getConfig and
setConfig, sanitize including the virtualFilesMode validation, the schema, the proxy
per field merge and the global proxy honoring a managed default, and virtualFilesMode
default versus enforced. The Account::setProxySettings write guard is not covered
there because Account::create self references and trips the leak checker; it is owed
in a non ASAN test.

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
  ManagedSettings::resolve(definition)
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
