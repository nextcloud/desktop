/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSet>
#include <QNetworkProxy>
#include <algorithm>
#include <memory>
#include <optional>

#include "account.h"
#include "capabilities.h"
#include "clientproxy.h"
#include "configfile.h"
#include "managedsettingstestutils.h"
#include "settings/devicesources.h"
#include "settings/managedconfig.h"
#include "settings/managedsettings.h"
#include "settings/managedsettingsschema.h"
#include "settings/servermanagedsettings.h"
#include "settings/settingsources.h"
#include "syncenginetestutils.h"

using namespace OCC;
using namespace Qt::StringLiterals;

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
    [[nodiscard]] bool isForced(const QString &key) const override
    {
        return _forcedKeys.contains(key);
    }
    [[nodiscard]] std::optional<QVariant> copyForcedValue(const QString &key) const override
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

    static SettingDefinition skipSpec()
    {
        return {QStringLiteral("skipUpdateCheck"), false, true};
    }

    static AccountPtr createAccountWithNetworkAccessManager()
    {
        const auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        return account;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // Never read the device policy of the machine running the tests.
    void init()
    {
        ConfigFile::setDeviceSourcesFactory([] {
            return std::vector<std::unique_ptr<SettingSource>>{};
        });
    }

    void cleanup()
    {
        ConfigFile::setDeviceSourcesFactory([] {
            return std::vector<std::unique_ptr<SettingSource>>{};
        });
    }

    void testBuiltinDefaultWhenNoSourceHasValue()
    {
        ManagedSettings resolver;

        const auto result = resolver.resolve(skipSpec());

        QCOMPARE(result.value.toBool(), false);
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

        const auto r = resolver.resolve({QStringLiteral("updateChannel"), QStringLiteral("stable"), false});

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

        const auto r = resolver.resolve({QStringLiteral("updateChannel"), QStringLiteral("stable"), false});

        QCOMPARE(r.value.toString(), QStringLiteral("beta"));
        QCOMPARE(r.source, SettingSourceType::UserConfig);
        QVERIFY(!r.isEnforced());
    }

    void testInvalidValueIsSkippedNotEnforced()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy,
                                                       EnforcementState::Enforced,
                                                       200,
                                                       QVariantMap{{QStringLiteral("timeout"), QStringLiteral("not-a-number")}}));
        resolver.addSource(
            std::make_unique<MapSource>(SettingSourceType::ServerDefault, EnforcementState::NotEnforced, 30, QVariantMap{{QStringLiteral("timeout"), 7}}));

        const auto r = resolver.resolve({QStringLiteral("timeout"), 42, true});

        QCOMPARE(r.value.toInt(), 7);
        QCOMPARE(r.source, SettingSourceType::ServerDefault);
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

    // Enforcement is opt in: only keys declared in the schema may be enforced. Proxy and the runtime
    // default folder limits must therefore be declared, and an undeclared key is not enforceable.
    void testEnforceableKeysAreDeclaredInSchema()
    {
        for (const auto &key : {QStringLiteral("proxyType"),
                                QStringLiteral("proxyHost"),
                                QStringLiteral("proxyPort"),
                                QStringLiteral("newBigFolderSizeLimit"),
                                QStringLiteral("stopSyncingExistingFoldersOverLimit")}) {
            const auto spec = ManagedSettingsSchema::find(key);
            QVERIFY2(spec.has_value(), qPrintable(key));
            QVERIFY2(spec->enforceable, qPrintable(key));
        }
        QVERIFY(!ManagedSettingsSchema::find(QStringLiteral("someUndeclaredKey")).has_value());
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

    void testParseServerManagedSettingsReadsSchemaAndMaps()
    {
        const QVariantMap cap{
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("defaults"), QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("wincfapi")}}},
            {QStringLiteral("enforced"), QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}},
        };

        const auto parsed = parseServerManagedSettings(cap);

        QCOMPARE(parsed.schemaVersion, 1);
        QCOMPARE(parsed.defaults.value(QStringLiteral("virtualFilesMode")).toString(), QStringLiteral("wincfapi"));
        QCOMPARE(parsed.enforced.value(QStringLiteral("skipUpdateCheck")).toBool(), true);
    }

    void testSanitizeKeepsAcceptedKeysDropsUnknown()
    {
        ServerManagedSettings raw;
        raw.defaults = QVariantMap{{QStringLiteral("skipUpdateCheck"), false}, {QStringLiteral("bogus"), 1}};
        raw.enforced = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("wincfapi")},
            {QStringLiteral("secretKey"), QStringLiteral("x")}};

        const auto clean = sanitizeServerManagedSettings(raw);

        QVERIFY(clean.defaults.contains(QStringLiteral("skipUpdateCheck")));
        QVERIFY(!clean.defaults.contains(QStringLiteral("bogus")));
        QVERIFY(clean.enforced.contains(QStringLiteral("virtualFilesMode")));
        QVERIFY(!clean.enforced.contains(QStringLiteral("secretKey")));
    }

    void testSanitizeDropsInvalidVirtualFilesMode()
    {
        ServerManagedSettings raw;
        raw.defaults = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("onlineOnly")}};
        raw.enforced = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("wincfapi")}};

        const auto clean = sanitizeServerManagedSettings(raw);

        QVERIFY(!clean.defaults.contains(QStringLiteral("virtualFilesMode")));
        QCOMPARE(clean.enforced.value(QStringLiteral("virtualFilesMode")).toString(), QStringLiteral("wincfapi"));
    }

    void testSanitizeDropsInvalidProxyValues()
    {
        ServerManagedSettings raw;
        raw.defaults = QVariantMap{{QStringLiteral("proxyType"), 6}, {QStringLiteral("proxyPort"), 0}};
        raw.enforced = QVariantMap{{QStringLiteral("proxyType"), 4}, {QStringLiteral("proxyPort"), 65535}};

        const auto clean = sanitizeServerManagedSettings(raw);

        QVERIFY(!clean.defaults.contains(QStringLiteral("proxyType")));
        QVERIFY(!clean.defaults.contains(QStringLiteral("proxyPort")));
        QVERIFY(!clean.enforced.contains(QStringLiteral("proxyType")));
        QVERIFY(!clean.enforced.contains(QStringLiteral("proxyPort")));
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
        raw.enforced = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("wincfapi")}};
        const auto clean = sanitizeServerManagedSettings(raw);

        const SettingDefinition spec{QStringLiteral("virtualFilesMode"), QStringLiteral(""), true};

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
        QCOMPARE(withoutDevice.value.toString(), QStringLiteral("wincfapi"));
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
        settings.defaults = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("wincfapi")}};
        settings.enforced = QVariantMap{{QStringLiteral("confirmExternalStorage"), false}, {QStringLiteral("skipUpdateCheck"), true}};
        config.setServerManagedSettings(settings);

        const auto read = config.serverManagedSettings();
        QCOMPARE(read.schemaVersion, 1);
        QCOMPARE(read.defaults.value(QStringLiteral("virtualFilesMode")).toString(), QStringLiteral("wincfapi"));
        QCOMPARE(read.enforced.value(QStringLiteral("confirmExternalStorage")).toBool(), false);
        // The cache is sanitized, so a key the server may not enforce never reaches it.
        QVERIFY(!read.enforced.contains(QStringLiteral("skipUpdateCheck")));
    }

    void testServerCannotEnforceSkipUpdateCheckOverUser()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        config.setSkipUpdateCheck(false, QString());

        ServerManagedSettings settings;
        settings.enforced = QVariantMap{{QStringLiteral("skipUpdateCheck"), true}};
        config.setServerManagedSettings(settings);

        // Only device policy may enforce an update key, so the user value stands.
        QCOMPARE(config.skipUpdateCheck(), false);
        QVERIFY(!config.isEnforced(QStringLiteral("skipUpdateCheck")));
    }

    void testResolverRefreshesOnServerSettingsChange()
    {
        QTemporaryDir dir;
        ConfigFile reader;
        reader.setConfDir(dir.path());
        ManagedConfig::instance().invalidate();

        // Prime the reader's cached resolver while nothing is enforced.
        QVERIFY(!reader.isEnforced(QStringLiteral("virtualFilesMode")));

        // A different ConfigFile writes enforced server settings.
        ServerManagedSettings settings;
        settings.enforced = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("wincfapi")}};
        ConfigFile writer;
        writer.setConfDir(dir.path());
        writer.setServerManagedSettings(settings);

        // The reader picks up the change without being recreated.
        QVERIFY(reader.isEnforced(QStringLiteral("virtualFilesMode")));
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
        serverSettings.enforced = QVariantMap{{QStringLiteral("confirmExternalStorage"), false}};
        config.setServerManagedSettings(serverSettings);

        // A user cannot override an enforced value.
        QCOMPARE(config.setConfig(QStringLiteral("confirmExternalStorage"), true), false);
        QCOMPARE(config.getConfig<bool>(QStringLiteral("confirmExternalStorage")), false);
        QVERIFY(config.isEnforced(QStringLiteral("confirmExternalStorage")));
        QCOMPARE(config.sourceOf(QStringLiteral("confirmExternalStorage")), SettingSourceType::ServerEnforced);
    }

    void testOutOfRangeProxyPolicyValueFallsBackToUserValue_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<int>("policyValue");
        QTest::addColumn<int>("userValue");

        QTest::newRow("port above range") << u"proxyPort"_s << 70000 << 3128;
        QTest::newRow("port zero") << u"proxyPort"_s << 0 << 3128;
        QTest::newRow("unknown proxy type") << u"proxyType"_s << 7 << int(QNetworkProxy::HttpProxy);
        QTest::newRow("negative proxy type") << u"proxyType"_s << -1 << int(QNetworkProxy::HttpProxy);
    }

    // Port 70000 would wrap to 4464.
    void testOutOfRangeProxyPolicyValueFallsBackToUserValue()
    {
        QFETCH(QString, key);
        QFETCH(int, policyValue);
        QFETCH(int, userValue);

        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy, EnforcementState::Enforced, 200, QVariantMap{{key, policyValue}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::UserConfig, EnforcementState::NotEnforced, 50, QVariantMap{{key, userValue}}));

        const auto definition = ManagedSettingsSchema::find(key);
        QVERIFY(definition.has_value());
        const auto resolved = resolver.resolve(*definition);

        QCOMPARE(resolved.value.toInt(), userValue);
        QCOMPARE(resolved.source, SettingSourceType::UserConfig);
        QVERIFY(!resolved.isEnforced());
    }

    void testInRangeProxyPolicyValueIsEnforced_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<int>("policyValue");

        QTest::newRow("lowest port") << u"proxyPort"_s << 1;
        QTest::newRow("highest port") << u"proxyPort"_s << 65535;
        QTest::newRow("system proxy type") << u"proxyType"_s << int(QNetworkProxy::DefaultProxy);
        QTest::newRow("http proxy type") << u"proxyType"_s << int(QNetworkProxy::HttpProxy);
    }

    void testInRangeProxyPolicyValueIsEnforced()
    {
        QFETCH(QString, key);
        QFETCH(int, policyValue);

        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy, EnforcementState::Enforced, 200, QVariantMap{{key, policyValue}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceType::UserConfig, EnforcementState::NotEnforced, 50, QVariantMap{{key, 42}}));

        const auto definition = ManagedSettingsSchema::find(key);
        QVERIFY(definition.has_value());
        const auto resolved = resolver.resolve(*definition);

        QCOMPARE(resolved.value.toInt(), policyValue);
        QVERIFY(resolved.isEnforced());
    }

    void testDeviceProxyPolicyReachesManagedProxySettings()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());
        ConfigFile::setDeviceSourcesFactory([] {
            std::vector<std::unique_ptr<SettingSource>> sources;
            sources.push_back(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy,
                                                          EnforcementState::Enforced,
                                                          200,
                                                          QVariantMap{{u"proxyHost"_s, u"proxy.example.com"_s}, {u"proxyPort"_s, 70000}}));
            return sources;
        });

        const auto managedProxy = config.managedProxySettings();

        QVERIFY(managedProxy.hostEnforced);
        QCOMPARE(managedProxy.proxyHostName, u"proxy.example.com"_s);
        QVERIFY(!managedProxy.typeManaged);
        QVERIFY(!managedProxy.portManaged);
        QVERIFY(!managedProxy.portEnforced);
    }

    // A server default must not reach the application proxy, which every account shares.
    // A value written by an earlier version at the top level must not outrank a later server default.
    void testSetConfigClearsTheLegacyTopLevelValue()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        {
            QSettings legacy(config.configFile(), QSettings::IniFormat);
            legacy.setValue(u"confirmExternalStorage"_s, true);
            legacy.sync();
        }
        QCOMPARE(config.sourceOf(u"confirmExternalStorage"_s), SettingSourceType::UserConfig);

        QVERIFY(config.setConfig(u"confirmExternalStorage"_s, false));

        QSettings written(config.configFile(), QSettings::IniFormat);
        QVERIFY(!written.contains(u"confirmExternalStorage"_s));
        QCOMPARE(config.getConfig<bool>(u"confirmExternalStorage"_s), false);

        // Clearing the choice of the user leaves nothing behind to shadow a server default.
        {
            QSettings groupValue(config.configFile(), QSettings::IniFormat);
            groupValue.beginGroup(config.defaultConnectionGroupName());
            groupValue.remove(u"confirmExternalStorage"_s);
            groupValue.sync();
        }
        ServerManagedSettings settings;
        settings.defaults = QVariantMap{{u"confirmExternalStorage"_s, true}};
        config.setServerManagedSettings(settings);

        QCOMPARE(config.sourceOf(u"confirmExternalStorage"_s), SettingSourceType::ServerDefault);
        QCOMPARE(config.getConfig<bool>(u"confirmExternalStorage"_s), true);
    }

    void testServerProxyDefaultAppliesToItsAccountButNotTheApplicationProxy()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        ServerManagedSettings accountServerSettings;
        accountServerSettings.defaults = QVariantMap{{u"proxyHost"_s, u"server.example.com"_s}, {u"proxyPort"_s, 8080}};

        const auto scopedToAccount = config.managedProxySettings(accountServerSettings);
        QVERIFY(scopedToAccount.hostManaged);
        QCOMPARE(scopedToAccount.proxyHostName, u"server.example.com"_s);
        QVERIFY(!scopedToAccount.isEnforced);

        const auto applicationProxy = config.managedProxySettings();
        QVERIFY(!applicationProxy.hostManaged);
        QVERIFY(!applicationProxy.portManaged);
    }

    // The File Provider is the macOS virtual files implementation, so an enforced off must disable it.
    void testEnforcedVirtualFilesOffDisablesFileProviderMode()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());
        config.setMacFileProviderModeEnabled(true);
        QVERIFY(config.macFileProviderModeEnabled());

        ConfigFile::setDeviceSourcesFactory([] {
            std::vector<std::unique_ptr<SettingSource>> sources;
            sources.push_back(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy,
                                                          EnforcementState::Enforced,
                                                          200,
                                                          QVariantMap{{u"virtualFilesMode"_s, u"off"_s}}));
            return sources;
        });

        QVERIFY(!config.macFileProviderModeEnabled());
    }

    void testEnforcedVirtualFilesModeKeepsFileProviderModeWhenEnabled()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());
        config.setMacFileProviderModeEnabled(true);

        ConfigFile::setDeviceSourcesFactory([] {
            std::vector<std::unique_ptr<SettingSource>> sources;
            sources.push_back(std::make_unique<MapSource>(SettingSourceType::PlatformPolicy,
                                                          EnforcementState::Enforced,
                                                          200,
                                                          QVariantMap{{u"virtualFilesMode"_s, u"wincfapi"_s}}));
            return sources;
        });

        QVERIFY(config.macFileProviderModeEnabled());
    }

    void testEnforcedProxyFieldReplacesOnlyThatField()
    {
        const auto account = createAccountWithNetworkAccessManager();
        account->setProxyType(QNetworkProxy::HttpProxy);
        account->setProxyHostName(u"account.example.com"_s);
        account->setProxyPort(1111);

        account->applyManagedProxySettings(managedProxyFields(true, std::nullopt, std::nullopt, 8080));

        QCOMPARE(account->proxyPort(), 8080);
        QCOMPARE(account->networkAccessManager()->proxy().port(), 8080);
        QCOMPARE(account->proxyHostName(), u"account.example.com"_s);
        QCOMPARE(account->proxyType(), QNetworkProxy::HttpProxy);
        QVERIFY(account->proxySettingsAreManaged());
        QCOMPARE(account->accountProxyPort(), 1111);
    }

    void testManagedProxyDefaultAppliesOnlyWhileFollowingSystemProxy_data()
    {
        QTest::addColumn<int>("accountProxyType");
        QTest::addColumn<int>("expectedProxyType");

        QTest::newRow("follows system proxy") << int(QNetworkProxy::DefaultProxy) << int(QNetworkProxy::HttpProxy);
        QTest::newRow("manual proxy") << int(QNetworkProxy::Socks5Proxy) << int(QNetworkProxy::Socks5Proxy);
        QTest::newRow("no proxy") << int(QNetworkProxy::NoProxy) << int(QNetworkProxy::NoProxy);
    }

    void testManagedProxyDefaultAppliesOnlyWhileFollowingSystemProxy()
    {
        QFETCH(int, accountProxyType);
        QFETCH(int, expectedProxyType);

        const auto account = createAccountWithNetworkAccessManager();
        account->setProxyType(static_cast<QNetworkProxy::ProxyType>(accountProxyType));

        account->applyManagedProxySettings(managedProxyFields(false, int(QNetworkProxy::HttpProxy), u"proxy.example.com"_s, 8080));

        QCOMPARE(int(account->proxyType()), expectedProxyType);
        QCOMPARE(int(account->accountProxyType()), accountProxyType);
        QVERIFY(!account->proxySettingsAreManaged());
    }

    void testReapplyingManagedProxyKeepsAccountValue()
    {
        const auto account = createAccountWithNetworkAccessManager();
        account->setProxyPort(1111);
        const auto managedProxy = managedProxyFields(true, std::nullopt, std::nullopt, 8080);

        account->applyManagedProxySettings(managedProxy);
        account->applyManagedProxySettings(managedProxy);

        QCOMPARE(account->accountProxyPort(), 1111);
    }

    void testRemovedProxyPolicyRestoresAccountValue()
    {
        const auto account = createAccountWithNetworkAccessManager();
        account->setProxyType(QNetworkProxy::HttpProxy);
        account->setProxyPort(1111);
        account->applyManagedProxySettings(managedProxyFields(true, int(QNetworkProxy::NoProxy), std::nullopt, 8080));

        account->applyManagedProxySettings({});

        QCOMPARE(account->proxyType(), QNetworkProxy::HttpProxy);
        QCOMPARE(account->proxyPort(), 1111);
        QVERIFY(!account->proxySettingsAreManaged());
    }

    void testEnforcedProxyFieldsIgnoreWritesButKeepCredentialsWritable()
    {
        const auto account = createAccountWithNetworkAccessManager();
        account->setProxyType(QNetworkProxy::HttpProxy);
        account->setProxyHostName(u"account.example.com"_s);
        account->setProxyPort(1111);
        account->applyManagedProxySettings(managedProxyFields(true, std::nullopt, u"proxy.example.com"_s, 8080));

        account->setProxySettings(QNetworkProxy::Socks5Proxy, u"user.example.com"_s, 9999, true, u"alice"_s, u"secret"_s);

        QCOMPARE(account->proxyHostName(), u"proxy.example.com"_s);
        QCOMPARE(account->proxyPort(), 8080);
        QCOMPARE(account->proxyType(), QNetworkProxy::Socks5Proxy);
        QVERIFY(account->proxyNeedsAuth());
        QCOMPARE(account->proxyUser(), u"alice"_s);
        QCOMPARE(account->proxyPassword(), u"secret"_s);
        QCOMPARE(account->accountProxyHostName(), u"account.example.com"_s);
        QCOMPARE(account->accountProxyPort(), 1111);
    }

    void testAccountProxyModeKeepsEnforcedProxy_data()
    {
        QTest::addColumn<int>("enforcedProxyType");
        QTest::addColumn<ClientProxy::AccountProxyMode>("expectedMode");

        QTest::newRow("enforced no proxy") << int(QNetworkProxy::NoProxy) << ClientProxy::AccountProxyMode::AccountProxy;
        QTest::newRow("enforced http proxy") << int(QNetworkProxy::HttpProxy) << ClientProxy::AccountProxyMode::AccountProxy;
        QTest::newRow("enforced system proxy") << int(QNetworkProxy::DefaultProxy) << ClientProxy::AccountProxyMode::SystemProxy;
    }

    void testAccountProxyModeKeepsEnforcedProxy()
    {
        QFETCH(int, enforcedProxyType);
        QFETCH(ClientProxy::AccountProxyMode, expectedMode);

        const auto account = createAccountWithNetworkAccessManager();
        account->setProxyType(QNetworkProxy::DefaultProxy);
        account->applyManagedProxySettings(managedProxyFields(true, enforcedProxyType, std::nullopt, std::nullopt));

        QCOMPARE(ClientProxy::accountProxyMode(*account), expectedMode);
    }

    void testAccountProxyModeLooksUpSystemProxyForUnmanagedAccount()
    {
        const auto account = createAccountWithNetworkAccessManager();
        account->setProxyType(QNetworkProxy::DefaultProxy);

        QCOMPARE(ClientProxy::accountProxyMode(*account), ClientProxy::AccountProxyMode::SystemProxy);
    }

    void testSourceLabelUsesRequestedSetting()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        ServerManagedSettings settings;
        settings.enforced = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("off")}};
        config.setServerManagedSettings(settings);

        QCOMPARE(config.sourceLabel(QStringLiteral("virtualFilesMode")),
                 QStringLiteral("Managed by your organization"));
    }

    void testManagedVirtualFilesModeReportsDefaultAndEnforcement()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        ServerManagedSettings settings;
        settings.defaults = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("wincfapi")}};
        config.setServerManagedSettings(settings);

        const auto defaultValue = config.managedVirtualFilesMode();
        QVERIFY(defaultValue.isManaged);
        QVERIFY(!defaultValue.isEnforced);
        QVERIFY(defaultValue.enabled);

        settings.defaults.clear();
        settings.enforced = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("off")}};
        config.setServerManagedSettings(settings);

        const auto enforcedValue = config.managedVirtualFilesMode();
        QVERIFY(enforcedValue.isManaged);
        QVERIFY(enforcedValue.isEnforced);
        QVERIFY(!enforcedValue.enabled);
    }

    void testServerSettingsAreCachedAndPersisted()
    {
        QTemporaryDir dir;
        ConfigFile config;
        config.setConfDir(dir.path());

        ServerManagedSettings settings;
        settings.schemaVersion = 1;
        settings.enforced = QVariantMap{{QStringLiteral("virtualFilesMode"), QStringLiteral("wincfapi")}};
        config.setServerManagedSettings(settings);

        QCOMPARE(config.serverManagedSettings().enforced.value(QStringLiteral("virtualFilesMode")).toString(), QStringLiteral("wincfapi"));

        // Dropping the cache reparses from the config file, proving persistence.
        ManagedConfig::instance().invalidate();
        QCOMPARE(config.serverManagedSettings().enforced.value(QStringLiteral("virtualFilesMode")).toString(), QStringLiteral("wincfapi"));
    }

    void testSanitizeDropsServerEnforcedUpdateAndProxyKeys()
    {
        ServerManagedSettings raw;
        raw.enforced = QVariantMap{{QStringLiteral("skipUpdateCheck"), true},
            {QStringLiteral("proxyHost"), QStringLiteral("evil.example.com")},
            {QStringLiteral("virtualFilesMode"), QStringLiteral("wincfapi")}};

        const auto clean = sanitizeServerManagedSettings(raw);

        // A server cannot enforce updates or proxy; only device policy can.
        QVERIFY(!clean.enforced.contains(QStringLiteral("skipUpdateCheck")));
        QVERIFY(!clean.enforced.contains(QStringLiteral("proxyHost")));
        QVERIFY(clean.enforced.contains(QStringLiteral("virtualFilesMode")));
    }
};

QTEST_GUILESS_MAIN(TestManagedSettings)
#include "testmanagedsettings.moc"
