/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSet>
#include <algorithm>
#include <memory>
#include <optional>

#include "configfile.h"
#include "capabilities.h"
#include "settings/managedsettings.h"
#include "settings/managedsettingsschema.h"
#include "settings/settingsources.h"
#include "settings/servermanagedsettings.h"
#include "settings/managedconfig.h"

using namespace OCC;

class MapSource : public SettingSource
{
public:
    MapSource(SettingSourceType kind, EnforcementState enforcement, int priority, QVariantMap values)
        : _kind(kind)
        , _enforcement(enforcement)
        , _priority(priority)
        , _values(std::move(values))
    {
    }

    std::optional<QVariant> read(const QString &key, const QString &) const override
    {
        if (!_values.contains(key)) {
            return std::nullopt;
        }
        return _values.value(key);
    }
    SettingSourceType type() const override
    {
        return _kind;
    }
    EnforcementState enforcement() const override { return _enforcement; }
    int priority() const override { return _priority; }

private:
    SettingSourceType _kind;
    EnforcementState _enforcement;
    int _priority;
    QVariantMap _values;
};

// Fake ForcedPreferenceSource with test controlled forced keys and values, no
// CoreFoundation dependency.
class FakeForcedSource : public ForcedPreferenceSource
{
public:
    FakeForcedSource(int priority, QSet<QString> forcedKeys, QVariantMap values)
        : ForcedPreferenceSource(priority)
        , _forcedKeys(std::move(forcedKeys))
        , _values(std::move(values))
    {
    }

protected:
    bool isForced(const QString &key) const override { return _forcedKeys.contains(key); }
    std::optional<QVariant> copyForcedValue(const QString &key) const override
    {
        if (!_values.contains(key)) {
            return std::nullopt;
        }
        return _values.value(key);
    }

private:
    QSet<QString> _forcedKeys;
    QVariantMap _values;
};

class TestManagedSettings : public QObject
{
    Q_OBJECT

    static SettingSpec skipSpec()
    {
        return {QStringLiteral("skipUpdateCheck"), false, true, SettingScope::User};
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testBuiltinDefaultWhenNoSourceHasValue()
    {
        ManagedSettings resolver;

        const auto result = resolver.resolve(skipSpec());

        QCOMPARE(result.value.toBool(), false);
        QCOMPARE(result.present, false);
        QCOMPARE(result.source, SettingSourceType::BuiltinDefault);
        QCOMPARE(result.enforcement, EnforcementState::NotEnforced);
    }

    void testUserBeatsPlatformDefault()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformDefault,
                                                       EnforcementState::NotEnforced,
                                                       20,
                                                       QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::UserConfig,
                                                       EnforcementState::NotEnforced,
                                                       50,
                                                       QVariantMap{{QStringLiteral("skipUpdateCheck"), false}}));

        const auto r = resolver.resolve(skipSpec());

        QCOMPARE(r.value.toBool(), false);
        QCOMPARE(r.source, SettingSourceType::UserConfig);
        QCOMPARE(r.enforcement, EnforcementState::NotEnforced);
        QCOMPARE(r.present, true);
    }

    void testEnforcedPolicyBeatsUser()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::UserConfig,
                                                       EnforcementState::NotEnforced,
                                                       50,
                                                       QVariantMap{{QStringLiteral("skipUpdateCheck"), false}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy,
                                                       EnforcementState::Enforced,
                                                       200,
                                                       QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}));

        const auto r = resolver.resolve(skipSpec());

        QCOMPARE(r.value.toBool(), true);
        QCOMPARE(r.source, SettingSourceType::PlatformPolicy);
        QVERIFY(r.isEnforced());
    }

    void testHighestPriorityEnforcedWins()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::ServerEnforced,
                                                       EnforcementState::Enforced,
                                                       100,
                                                       QVariantMap{{QStringLiteral("skipUpdateCheck"), false}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy,
                                                       EnforcementState::Enforced,
                                                       200,
                                                       QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}));

        const auto r = resolver.resolve(skipSpec());

        QCOMPARE(r.value.toBool(), true); // device policy (200) beats server enforced (100)
        QCOMPARE(r.source, SettingSourceType::PlatformPolicy);
    }

    void testHighestPriorityDefaultWinsWhenNoUser()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformDefault,
                                                       EnforcementState::NotEnforced,
                                                       20,
                                                       QVariantMap{{QStringLiteral("updateChannel"), QStringLiteral("beta")}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::ServerDefault,
                                                       EnforcementState::NotEnforced,
                                                       30,
                                                       QVariantMap{{QStringLiteral("updateChannel"), QStringLiteral("stable")}}));

        const auto r = resolver.resolve({QStringLiteral("updateChannel"), QStringLiteral("stable"), false, SettingScope::User});

        QCOMPARE(r.value.toString(), QStringLiteral("stable")); // ServerDefault(30) beats PlatformDefault(20)
        QCOMPARE(r.source, SettingSourceType::ServerDefault);
    }

    void testEnforcedIgnoredWhenSettingNotEnforceable()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::UserConfig,
                                                       EnforcementState::NotEnforced,
                                                       50,
                                                       QVariantMap{{QStringLiteral("updateChannel"), QStringLiteral("beta")}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy,
                                                       EnforcementState::Enforced,
                                                       200,
                                                       QVariantMap{{QStringLiteral("updateChannel"), QStringLiteral("stable")}}));

        const auto r = resolver.resolve({QStringLiteral("updateChannel"), QStringLiteral("stable"), false, SettingScope::User});

        QCOMPARE(r.value.toString(), QStringLiteral("beta"));
        QCOMPARE(r.source, SettingSourceType::UserConfig);
        QVERIFY(!r.isEnforced());
    }

    // A forced source contributes only forced keys; a present but non forced
    // value is ignored.
    void testForcedSourceContributesOnlyForcedKeys()
    {
        FakeForcedSource source(200,
            {QStringLiteral("skipUpdateCheck")},
            QVariantMap{{QStringLiteral("skipUpdateCheck"), true},
                {QStringLiteral("autoUpdateCheck"), false}});

        QCOMPARE(source.type(), SettingSourceType::PlatformPolicy);
        QCOMPARE(source.enforcement(), EnforcementState::Enforced);
        QCOMPARE(source.priority(), 200);

        const auto forced = source.read(QStringLiteral("skipUpdateCheck"), QString());
        QVERIFY(forced.has_value());
        QCOMPARE(forced->toBool(), true);

        // Present in the domain but not forced, so it must not be enforced.
        QVERIFY(!source.read(QStringLiteral("autoUpdateCheck"), QString()).has_value());
    }

    void testSchemaHasUpdateSettings()
    {
        const auto skip = ManagedSettingsSchema::find(QStringLiteral("skipUpdateCheck"));
        QVERIFY(skip.has_value());
        QCOMPARE(skip->builtinDefault.toBool(), false);
        QVERIFY(skip->enforceable);

        const auto autoCheck = ManagedSettingsSchema::find(QStringLiteral("autoUpdateCheck"));
        QVERIFY(autoCheck.has_value());
        QCOMPARE(autoCheck->builtinDefault.toBool(), true);

        QVERIFY(!ManagedSettingsSchema::find(QStringLiteral("nonexistent")).has_value());
    }

    void testUserConfigSourceReadsIniValueAndGroup()
    {
        QTemporaryDir dir;
        const auto path = dir.path() + QStringLiteral("/user.cfg");
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("skipUpdateCheck"), true);
            settings.beginGroup(QStringLiteral("Accounts"));
            settings.setValue(QStringLiteral("autoUpdateCheck"), false);
            settings.endGroup();
            settings.sync();
        }
        const UserConfigSource source(path);

        QCOMPARE(source.type(), SettingSourceType::UserConfig);
        QCOMPARE(source.enforcement(), EnforcementState::NotEnforced);
        QCOMPARE(source.read(QStringLiteral("skipUpdateCheck"), QString())->toBool(), true);
        QCOMPARE(source.read(QStringLiteral("autoUpdateCheck"), QStringLiteral("Accounts"))->toBool(), false);
        QVERIFY(!source.read(QStringLiteral("missing"), QString()).has_value());
    }

    // The platform adapters read real OS stores, so they cannot be exercised on
    // the Linux test build. This only checks the factory compiles and runs.
    void testBuildDeviceSourcesReturnsSources()
    {
        const auto sources = buildDeviceSources();
        QVERIFY(!sources.empty());
    }

    void testUserConfigSourceBakedGroupOverridesReadGroup()
    {
        QTemporaryDir dir;
        const auto path = dir.path() + QStringLiteral("/user.cfg");
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.beginGroup(QStringLiteral("Nextcloud"));
            settings.setValue(QStringLiteral("skipUpdateCheck"), true);
            settings.endGroup();
            settings.sync();
        }
        const UserConfigSource source(path, QStringLiteral("Nextcloud"));

        QCOMPARE(source.read(QStringLiteral("skipUpdateCheck"), QStringLiteral("Other"))->toBool(), true);
    }

    void testConfigFileRoutesSkipUpdateCheckThroughResolver()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        QCOMPARE(config.skipUpdateCheck(), false);
        config.setSkipUpdateCheck(true, QString());
        QCOMPARE(config.skipUpdateCheck(), true);
    }

    void testResolveAllReturnsMetadataPerSpec()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy,
                                                       EnforcementState::Enforced,
                                                       200,
                                                       QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}));

        const auto all = resolver.resolveAll(ManagedSettingsSchema::all());
        QCOMPARE(all.size(), ManagedSettingsSchema::all().size());

        const auto skip = std::find_if(all.cbegin(), all.cend(),
            [](const ManagedValue &value) { return value.key == QStringLiteral("skipUpdateCheck"); });
        QVERIFY(skip != all.cend());
        QVERIFY(skip->isEnforced());
        QCOMPARE(skip->source, SettingSourceType::PlatformPolicy);
    }

    void testParseServerManagedSettingsReadsSchemaAndMaps()
    {
        const QVariantMap cap{
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("defaults"), QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("enabled")}}},
            {QStringLiteral("enforced"), QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}},
        };

        const auto parsed = parseServerManagedSettings(cap);

        QCOMPARE(parsed.schemaVersion, 1);
        QCOMPARE(parsed.defaults.value(QStringLiteral("virtualFilesMode")).toString(), QStringLiteral("enabled"));
        QCOMPARE(parsed.enforced.value(QStringLiteral("skipUpdateCheck")).toBool(), true);
    }

    void testSanitizeKeepsAcceptedKeysDropsUnknown()
    {
        ServerManagedSettings raw;
        raw.defaults = QVariantMap{{QStringLiteral("skipUpdateCheck"), false}, {QStringLiteral("bogus"), 1}};
        raw.enforced = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("onlineOnly")},
            {QStringLiteral("secretKey"), QStringLiteral("x")}};

        const auto clean = sanitizeServerManagedSettings(raw);

        QVERIFY(clean.defaults.contains(QStringLiteral("skipUpdateCheck")));
        QVERIFY(!clean.defaults.contains(QStringLiteral("bogus")));
        QVERIFY(clean.enforced.contains(QStringLiteral("virtualFilesMode")));
        QVERIFY(!clean.enforced.contains(QStringLiteral("secretKey")));
    }

    void testServerSettingsSourceExposesMapWithKindEnforcementPriority()
    {
        const ServerSettingsSource source(QVariantMap{{QStringLiteral("skipUpdateCheck"), true}},
                                          SettingSourceType::ServerEnforced,
                                          EnforcementState::Enforced,
                                          100);

        QCOMPARE(source.type(), SettingSourceType::ServerEnforced);
        QCOMPARE(source.enforcement(), EnforcementState::Enforced);
        QCOMPARE(source.priority(), 100);
        QCOMPARE(source.read(QStringLiteral("skipUpdateCheck"), QString())->toBool(), true);
        QVERIFY(!source.read(QStringLiteral("missing"), QString()).has_value());
    }

    void testServerEnforcedEnforcedOverUserButDevicePolicyWins()
    {
        // virtualFilesMode stays server enforceable, so it survives sanitize.
        ServerManagedSettings raw;
        raw.enforced = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("onlineOnly")}};
        const auto clean = sanitizeServerManagedSettings(raw);

        const SettingSpec spec{QStringLiteral("virtualFilesMode"), QStringLiteral(""), true, SettingScope::User};

        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::UserConfig,
                                                       EnforcementState::NotEnforced,
                                                       50,
                                                       QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("off")}}));
        for (auto &source : buildServerSources(clean)) {
            resolver.addSource(std::move(source));
        }

        // Server enforced beats the user config.
        const auto withoutDevice = resolver.resolve(spec);
        QCOMPARE(withoutDevice.value.toString(), QStringLiteral("onlineOnly"));
        QCOMPARE(withoutDevice.source, SettingSourceType::ServerEnforced);

        // Device policy still beats server enforced.
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy,
                                                       EnforcementState::Enforced,
                                                       200,
                                                       QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("off")}}));
        const auto withDevice = resolver.resolve(spec);
        QCOMPARE(withDevice.value.toString(), QStringLiteral("off"));
        QCOMPARE(withDevice.source, SettingSourceType::PlatformPolicy);
    }

    void testConfigFileServerManagedSettingsRoundtrip()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        ServerManagedSettings settings;
        settings.schemaVersion = 1;
        settings.defaults = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("enabled")}};
        settings.enforced = QVariantMap{{QStringLiteral("skipUpdateCheck"), true}};
        config.setServerManagedSettings(settings);

        const auto read = config.serverManagedSettings();
        QCOMPARE(read.schemaVersion, 1);
        QCOMPARE(read.defaults.value(QStringLiteral("virtualFilesMode")).toString(), QStringLiteral("enabled"));
        QCOMPARE(read.enforced.value(QStringLiteral("skipUpdateCheck")).toBool(), true);
    }

    void testSkipUpdateCheckHonorsServerEnforcedOverUser()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        config.setSkipUpdateCheck(false, QString());
        QCOMPARE(config.skipUpdateCheck(), false);

        ServerManagedSettings settings;
        settings.enforced = QVariantMap{{QStringLiteral("skipUpdateCheck"), true}};
        config.setServerManagedSettings(settings);

        // Server enforced overrides the user value.
        QCOMPARE(config.skipUpdateCheck(), true);
    }

    void testCapabilitiesParsesDesktopClientManagedSettings()
    {
        const QVariantMap caps{
            {QStringLiteral("support"), QVariantMap{
                {QStringLiteral("desktopClient"), QVariantMap{
                    {QStringLiteral("schemaVersion"), 1},
                    {QStringLiteral("enforced"), QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}},
                }},
            }},
        };
        const Capabilities capabilities(caps);
        const auto parsed = capabilities.desktopClientManagedSettings();

        QCOMPARE(parsed.schemaVersion, 1);
        QCOMPARE(parsed.enforced.value(QStringLiteral("skipUpdateCheck")).toBool(), true);
    }

    void testGetConfigResolvesUserValueWithMetadata()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        QVERIFY(config.setConfig(QStringLiteral("skipUpdateCheck"), true));
        QCOMPARE(config.getConfig<bool>(QStringLiteral("skipUpdateCheck")), true);

        const auto resolved = config.getConfig(QStringLiteral("skipUpdateCheck"));
        QCOMPARE(resolved.source, SettingSourceType::UserConfig);
        QVERIFY(!resolved.isEnforced());
    }

    void testGetConfigStringUnmanagedKeyFallsBackToUser()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        QVERIFY(config.setConfig(QStringLiteral("someText"), QStringLiteral("hello")));
        QCOMPARE(config.getConfig<QString>(QStringLiteral("someText")), QStringLiteral("hello"));
    }

    void testSetConfigRefusesWhenEnforced()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        ServerManagedSettings serverSettings;
        serverSettings.enforced = QVariantMap{{QStringLiteral("skipUpdateCheck"), true}};
        config.setServerManagedSettings(serverSettings);

        // A user cannot override an enforced value.
        QCOMPARE(config.setConfig(QStringLiteral("skipUpdateCheck"), false), false);
        QCOMPARE(config.getConfig<bool>(QStringLiteral("skipUpdateCheck")), true);
        QVERIFY(config.isEnforced(QStringLiteral("skipUpdateCheck")));
        QCOMPARE(config.sourceOf(QStringLiteral("skipUpdateCheck")), SettingSourceType::ServerEnforced);
    }

    void testServerSettingsAreCachedAndPersisted()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        ServerManagedSettings settings;
        settings.schemaVersion = 1;
        settings.enforced = QVariantMap{{QStringLiteral("skipUpdateCheck"), true}};
        config.setServerManagedSettings(settings);

        QCOMPARE(config.serverManagedSettings().enforced.value(QStringLiteral("skipUpdateCheck")).toBool(), true);

        // Dropping the cache reparses from the config file, proving persistence.
        ManagedConfig::instance().invalidate();
        QCOMPARE(config.serverManagedSettings().enforced.value(QStringLiteral("skipUpdateCheck")).toBool(), true);
    }

    void testSanitizeDropsServerEnforcedUpdateAndProxyKeys()
    {
        ServerManagedSettings raw;
        raw.enforced = QVariantMap{{QStringLiteral("skipUpdateCheck"), true},
            {QStringLiteral("proxyHost"), QStringLiteral("evil.example.com")},
            {QStringLiteral("virtualFilesMode"), QStringLiteral("onlineOnly")}};

        const auto clean = sanitizeServerManagedSettings(raw);

        // A server cannot enforce updates or proxy; only device policy can.
        QVERIFY(!clean.enforced.contains(QStringLiteral("skipUpdateCheck")));
        QVERIFY(!clean.enforced.contains(QStringLiteral("proxyHost")));
        QVERIFY(clean.enforced.contains(QStringLiteral("virtualFilesMode")));
    }
};

QTEST_GUILESS_MAIN(TestManagedSettings)
#include "testmanagedsettings.moc"
