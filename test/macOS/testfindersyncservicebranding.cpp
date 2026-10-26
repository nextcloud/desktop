/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#undef APPLICATION_REV_DOMAIN
#define APPLICATION_REV_DOMAIN "org.example.brandedclient"

#include "../../src/gui/macOS/findersyncservice.mm"

#include "configfile.h"

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace Qt::StringLiterals;

class TestFinderSyncServiceBranding : public QObject
{
    Q_OBJECT

    QTemporaryDir configDirectory;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(configDirectory.isValid());
        QVERIFY(OCC::ConfigFile::setConfDir(configDirectory.path()));
    }

    void errorsUseConfiguredApplicationDomain_data()
    {
        QTest::addColumn<bool>("queuedOwnerDestroyed");
        QTest::addColumn<bool>("menuRequest");
        QTest::newRow("immediate status") << false << false;
        QTest::newRow("immediate menu") << false << true;
        QTest::newRow("queued status") << true << false;
        QTest::newRow("queued menu") << true << true;
    }

    void errorsUseConfiguredApplicationDomain()
    {
        QFETCH(bool, queuedOwnerDestroyed);
        QFETCH(bool, menuRequest);
        OCC::SocketApi socketApi;
        auto service = std::make_unique<OCC::Mac::FinderSyncService>();
        if (queuedOwnerDestroyed) {
            service->setSocketApi(&socketApi);
        }
        const auto delegate = (__bridge id<FinderSyncAppProtocol>)service->delegate();
        __block auto callbackCount = 0;
        void (^reply)(id, NSError *) = ^(id result, NSError *error) {
            ++callbackCount;
            QVERIFY(result == nil);
            QVERIFY(error != nil);
            QCOMPARE(error.code, 1);
            QCOMPARE(QString::fromNSString(error.domain), u"org.example.brandedclient.FinderSyncService"_s);
        };
        if (menuRequest) {
            [delegate getMenuItemsForPaths:@[ @"/outside-sync-root" ]
                         completionHandler:^(NSArray<NSDictionary *> *items, NSError *error) { reply(items, error); }];
        } else {
            [delegate retrieveFileStatusForPath:@"/outside-sync-root" completionHandler:^(NSString *status, NSError *error) { reply(status, error); }];
        }
        QCOMPARE(callbackCount, queuedOwnerDestroyed ? 0 : 1);
        if (queuedOwnerDestroyed) {
            service.reset();
            QTRY_COMPARE(callbackCount, 1);
        }
        QCoreApplication::processEvents();
        QCOMPARE(callbackCount, 1);
    }
};

QTEST_GUILESS_MAIN(TestFinderSyncServiceBranding)
#include "testfindersyncservicebranding.moc"
