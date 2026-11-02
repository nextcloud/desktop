/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "common/utility.h"
#include "macutilitymock.h"

#include <QtTest>

using namespace Qt::StringLiterals;

class TestMacUtility : public QObject
{
    Q_OBJECT

private:
    MacUtilityMock *__strong _native = nil;

private Q_SLOTS:
    void init()
    {
        @autoreleasepool {
            _native = [[MacUtilityMock alloc] init];
            QVERIFY([_native install]);
        }
    }

    void cleanup()
    {
        @autoreleasepool {
            [_native uninstall];
            _native = nil;
        }
    }

    void reportsLoginItemStatus_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<bool>("requiresApproval");
        QTest::newRow("not-registered") << int(SMAppServiceStatusNotRegistered) << false << false;
        QTest::newRow("enabled") << int(SMAppServiceStatusEnabled) << true << false;
        QTest::newRow("requires-approval") << int(SMAppServiceStatusRequiresApproval) << true << true;
        QTest::newRow("not-found") << int(SMAppServiceStatusNotFound) << false << false;
    }

    void reportsLoginItemStatus()
    {
        QFETCH(int, status);
        QFETCH(bool, enabled);
        QFETCH(bool, requiresApproval);
        _native.configuredStatus = static_cast<SMAppServiceStatus>(status);

        QCOMPARE(OCC::Utility::hasLaunchOnStartup(u"ignored app name"_s), enabled);
        QCOMPARE(OCC::Utility::launchOnStartupRequiresApproval(), requiresApproval);

        QCOMPARE(_native.factoryCount, 2);
        QCOMPARE(_native.statusReadCount, 2);
        QCOMPARE(_native.registerCount, 0);
        QCOMPARE(_native.unregisterCount, 0);
        QCOMPARE(_native.liveServiceCount, 0);
    }

    void updatesLoginItemAndChecksResultingStatus_data()
    {
        QTest::addColumn<bool>("enable");
        QTest::addColumn<int>("status");
        QTest::newRow("register-enabled") << true << int(SMAppServiceStatusEnabled);
        QTest::newRow("register-pending") << true << int(SMAppServiceStatusRequiresApproval);
        QTest::newRow("register-not-registered") << true << int(SMAppServiceStatusNotRegistered);
        QTest::newRow("register-not-found") << true << int(SMAppServiceStatusNotFound);
        QTest::newRow("unregister-not-registered") << false << int(SMAppServiceStatusNotRegistered);
        QTest::newRow("unregister-not-found") << false << int(SMAppServiceStatusNotFound);
        QTest::newRow("unregister-enabled") << false << int(SMAppServiceStatusEnabled);
        QTest::newRow("unregister-pending") << false << int(SMAppServiceStatusRequiresApproval);
    }

    void updatesLoginItemAndChecksResultingStatus()
    {
        QFETCH(bool, enable);
        QFETCH(int, status);
        _native.updatedStatus = static_cast<SMAppServiceStatus>(status);

        OCC::Utility::setLaunchOnStartup(u"ignored app name"_s, u"ignored display name"_s, enable);

        QCOMPARE(_native.factoryCount, 1);
        QCOMPARE(_native.registerCount, enable ? 1 : 0);
        QCOMPARE(_native.unregisterCount, enable ? 0 : 1);
        QCOMPARE(_native.configuredStatus, _native.updatedStatus);
        QCOMPARE(_native.statusReadCount, 1);
        QVERIFY(_native.errorOutputProvided);
        QCOMPARE(_native.liveServiceCount, 0);
        QCOMPARE(_native.liveErrorCount, 0);
    }

    void failedLoginItemUpdateReleasesObjectsAndDoesNotReadStatus_data()
    {
        QTest::addColumn<bool>("enable");
        QTest::addColumn<bool>("includesError");
        QTest::newRow("register-error") << true << true;
        QTest::newRow("register-without-error") << true << false;
        QTest::newRow("unregister-error") << false << true;
        QTest::newRow("unregister-without-error") << false << false;
    }

    void failedLoginItemUpdateReleasesObjectsAndDoesNotReadStatus()
    {
        QFETCH(bool, enable);
        QFETCH(bool, includesError);
        _native.configuredStatus = enable ? SMAppServiceStatusNotRegistered : SMAppServiceStatusEnabled;
        _native.updatedStatus = enable ? SMAppServiceStatusEnabled : SMAppServiceStatusNotRegistered;
        const auto originalStatus = _native.configuredStatus;
        _native.operationSucceeds = NO;
        _native.includesError = includesError;

        OCC::Utility::setLaunchOnStartup({}, {}, enable);

        QCOMPARE(_native.factoryCount, 1);
        QCOMPARE(_native.registerCount, enable ? 1 : 0);
        QCOMPARE(_native.unregisterCount, enable ? 0 : 1);
        QCOMPARE(_native.statusReadCount, 0);
        QCOMPARE(_native.configuredStatus, originalStatus);
        QVERIFY(_native.errorOutputProvided);
        QCOMPARE(_native.liveServiceCount, 0);
        QCOMPARE(_native.liveErrorCount, 0);
    }

    void registrationAndRemovalAreReflectedInSubsequentQueries()
    {
        _native.updatedStatus = SMAppServiceStatusRequiresApproval;
        OCC::Utility::setLaunchOnStartup({}, {}, true);
        QVERIFY(OCC::Utility::hasLaunchOnStartup({}));
        QVERIFY(OCC::Utility::launchOnStartupRequiresApproval());

        _native.updatedStatus = SMAppServiceStatusNotRegistered;
        OCC::Utility::setLaunchOnStartup({}, {}, false);
        QVERIFY(!OCC::Utility::hasLaunchOnStartup({}));
        QVERIFY(!OCC::Utility::launchOnStartupRequiresApproval());

        QCOMPARE(_native.registerCount, 1);
        QCOMPARE(_native.unregisterCount, 1);
        QCOMPARE(_native.liveServiceCount, 0);
        QCOMPARE(_native.liveErrorCount, 0);
    }

#ifndef TOKEN_AUTH_ONLY
    void reportsDarkTrayPreference_data()
    {
        QTest::addColumn<QString>("style");
        QTest::addColumn<bool>("dark");
        QTest::newRow("missing") << QString() << false;
        QTest::newRow("dark") << u"Dark"_s << true;
        QTest::newRow("light") << u"Light"_s << false;
        QTest::newRow("lowercase") << u"dark"_s << false;
    }

    void reportsDarkTrayPreference()
    {
        QFETCH(QString, style);
        QFETCH(bool, dark);
        _native.style = style.isNull() ? nil : style.toNSString();

        QCOMPARE(OCC::Utility::hasDarkSystray(), dark);

        QCOMPARE(_native.preferenceReadCount, 1);
        QCOMPARE(_native.liveStyleCount, 0);
        QCOMPARE(_native.factoryCount, 0);
    }
#endif
};

QTEST_APPLESS_MAIN(TestMacUtility)

#include "testmacutility.moc"
