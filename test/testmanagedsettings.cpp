/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <memory>
#include <optional>

#include "settings/managedsettings.h"
#include "settings/managedsettingsschema.h"

using namespace OCC;

class MapSource : public SettingSource
{
public:
    MapSource(SettingSourceKind kind, LockState lockState, int priority, QVariantMap values)
        : _kind(kind)
        , _lockState(lockState)
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
    SettingSourceKind kind() const override { return _kind; }
    LockState lockState() const override { return _lockState; }
    int priority() const override { return _priority; }

private:
    SettingSourceKind _kind;
    LockState _lockState;
    int _priority;
    QVariantMap _values;
};

class TestManagedSettings : public QObject
{
    Q_OBJECT

    static SettingSpec skipSpec()
    {
        return {QStringLiteral("skipUpdateCheck"), false, true, SettingScope::User};
    }

private slots:
    void testBuiltinDefaultWhenNoSourceHasValue()
    {
        ManagedSettings resolver;

        const auto result = resolver.resolve(skipSpec());

        QCOMPARE(result.value.toBool(), false);
        QCOMPARE(result.present, false);
        QCOMPARE(result.source, SettingSourceKind::BuiltinDefault);
        QCOMPARE(result.lockState, LockState::Unlocked);
    }

    void testUserBeatsPlatformDefault()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::PlatformDefault, LockState::Unlocked, 20,
            QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::UserConfig, LockState::Unlocked, 50,
            QVariantMap{{QStringLiteral("skipUpdateCheck"), false}}));

        const auto r = resolver.resolve(skipSpec());

        QCOMPARE(r.value.toBool(), false);
        QCOMPARE(r.source, SettingSourceKind::UserConfig);
        QCOMPARE(r.lockState, LockState::Unlocked);
        QCOMPARE(r.present, true);
    }

    void testLockedPolicyBeatsUser()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::UserConfig, LockState::Unlocked, 50,
            QVariantMap{{QStringLiteral("skipUpdateCheck"), false}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::PlatformPolicy, LockState::Locked, 200,
            QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}));

        const auto r = resolver.resolve(skipSpec());

        QCOMPARE(r.value.toBool(), true);
        QCOMPARE(r.source, SettingSourceKind::PlatformPolicy);
        QVERIFY(r.isLocked());
    }

    void testHighestPriorityLockedWins()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::ServerLocked, LockState::Locked, 100,
            QVariantMap{{QStringLiteral("skipUpdateCheck"), false}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::PlatformPolicy, LockState::Locked, 200,
            QVariantMap{{QStringLiteral("skipUpdateCheck"), true}}));

        const auto r = resolver.resolve(skipSpec());

        QCOMPARE(r.value.toBool(), true); // device policy (200) beats server locked (100)
        QCOMPARE(r.source, SettingSourceKind::PlatformPolicy);
    }

    void testHighestPriorityDefaultWinsWhenNoUser()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::PlatformDefault, LockState::Unlocked, 20,
            QVariantMap{{QStringLiteral("updateChannel"), QStringLiteral("beta")}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::ServerDefault, LockState::Unlocked, 30,
            QVariantMap{{QStringLiteral("updateChannel"), QStringLiteral("stable")}}));

        const auto r = resolver.resolve({QStringLiteral("updateChannel"), QStringLiteral("stable"), false, SettingScope::User});

        QCOMPARE(r.value.toString(), QStringLiteral("stable")); // ServerDefault(30) beats PlatformDefault(20)
        QCOMPARE(r.source, SettingSourceKind::ServerDefault);
    }

    void testLockedIgnoredWhenSettingNotLockable()
    {
        ManagedSettings resolver;
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::UserConfig, LockState::Unlocked, 50,
            QVariantMap{{QStringLiteral("updateChannel"), QStringLiteral("beta")}}));
        resolver.addSource(std::make_unique<MapSource>(SettingSourceKind::PlatformPolicy, LockState::Locked, 200,
            QVariantMap{{QStringLiteral("updateChannel"), QStringLiteral("stable")}}));

        const auto r = resolver.resolve({QStringLiteral("updateChannel"), QStringLiteral("stable"), false, SettingScope::User});

        QCOMPARE(r.value.toString(), QStringLiteral("beta"));
        QCOMPARE(r.source, SettingSourceKind::UserConfig);
        QVERIFY(!r.isLocked());
    }

    void testSchemaHasUpdateSettings()
    {
        const auto skip = ManagedSettingsSchema::find(QStringLiteral("skipUpdateCheck"));
        QVERIFY(skip.has_value());
        QCOMPARE(skip->builtinDefault.toBool(), false);
        QVERIFY(skip->lockable);

        const auto autoCheck = ManagedSettingsSchema::find(QStringLiteral("autoUpdateCheck"));
        QVERIFY(autoCheck.has_value());
        QCOMPARE(autoCheck->builtinDefault.toBool(), true);

        QVERIFY(!ManagedSettingsSchema::find(QStringLiteral("nonexistent")).has_value());
    }
};

QTEST_GUILESS_MAIN(TestManagedSettings)
#include "testmanagedsettings.moc"
