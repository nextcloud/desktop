/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "config.h"
#include "configfile.h"
#include "macOS/fileproviderservice.h"
#include "macOS/findersyncservice.h"
#include "macOS/findersyncxpc.h"
#include "socketapi/socketapi.h"
#include "xpcservicetestpeer.h"

#import "../../shell_integration/MacOSX/NextcloudFileProviderKit/Sources/NextcloudFileProviderXPC/include/NextcloudFileProviderXPC.h"
#import "../../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncAppProtocol.h"

static_assert(__has_feature(objc_arc), "XPC service lifecycle tests require ARC.");

using namespace Qt::StringLiterals;

class TestXPCServiceLifecycle : public QObject
{
    Q_OBJECT

    QTemporaryDir _configDir;

    static NSObject *listenerDelegateFor(OCC::Mac::FinderSyncXPC *owner, OCC::Mac::FinderSyncService *service)
    {
        NSObject *delegate = [NSClassFromString(@"FinderSyncXPCListenerDelegate") new];
        if (!delegate) {
            return nil;
        }
        const auto ownerSelector = NSSelectorFromString(@"setFinderSyncXPC:");
        const auto serviceSelector = NSSelectorFromString(@"setService:");
        using OwnerSetter = void (*)(id, SEL, OCC::Mac::FinderSyncXPC *);
        using ServiceSetter = void (*)(id, SEL, OCC::Mac::FinderSyncService *);
        reinterpret_cast<OwnerSetter>([delegate methodForSelector:ownerSelector])(delegate, ownerSelector, owner);
        reinterpret_cast<ServiceSetter>([delegate methodForSelector:serviceSelector])(delegate, serviceSelector, service);
        return delegate;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(_configDir.isValid());
        QVERIFY(OCC::ConfigFile::setConfDir(_configDir.path()));
    }

    void fileProviderDelegateQueuesSignalsAndFollowsOwnerLifetime()
    {
        __weak id weakDelegate = nil;
        @autoreleasepool {
            auto service = std::make_unique<OCC::Mac::FileProviderService>();
            const QSignalSpy received(service.get(), &OCC::Mac::FileProviderService::openItemInBrowserRequested);
            weakDelegate = (__bridge id)service->delegate();
            QVERIFY(weakDelegate != nil);
            [(id<AppProtocol>)weakDelegate openItemInBrowser:@"42" remoteItemPath:@"Documents/test.txt" forDomainIdentifier:@"domain-id"];
            QCOMPARE(received.count(), 0);
            QTRY_COMPARE(received.count(), 1);
            QCOMPARE(received.first().at(0).toString(), u"42"_s);
            QCOMPARE(received.first().at(1).toString(), u"Documents/test.txt"_s);
            QCOMPARE(received.first().at(2).toString(), u"domain-id"_s);
            service.reset();
        }
        QVERIFY(weakDelegate == nil);
    }

    void retainedFinderDelegateReportsUnavailableAfterOwnerDestruction()
    {
        __weak id weakDelegate = nil;
        @autoreleasepool {
            auto service = std::make_unique<OCC::Mac::FinderSyncService>();
            const auto delegate = (__bridge id<FinderSyncAppProtocol>)service->delegate();
            weakDelegate = delegate;
            service.reset();
            __block auto completed = false;
            [delegate retrieveFileStatusForPath:@"/outside-sync-root"
                              completionHandler:^(NSString *status, NSError *error) {
                                  completed = true;
                                  QVERIFY(status == nil);
                                  QCOMPARE(error.code, 1);
                                  QCOMPARE(QString::fromNSString(error.domain), QString::fromLatin1(APPLICATION_REV_DOMAIN ".FinderSyncService"));
                              }];
            QVERIFY(completed);
        }
        QVERIFY(weakDelegate == nil);
    }

    void finderRepliesSurviveTheSubmittingAutoreleasePool_data()
    {
        QTest::addColumn<int>("request");
        QTest::newRow("file status") << 0;
        QTest::newRow("folder status") << 1;
        QTest::newRow("menu items") << 2;
        QTest::newRow("menu command") << 3;
    }

    void finderRepliesSurviveTheSubmittingAutoreleasePool()
    {
        QFETCH(int, request);
        OCC::SocketApi socketApi;
        OCC::Mac::FinderSyncService service;
        service.setSocketApi(&socketApi);
        __weak id weakToken = nil;
        __block auto completed = false;
        @autoreleasepool {
            const auto token = [NSObject new];
            weakToken = token;
            const auto delegate = (__bridge id<FinderSyncAppProtocol>)service.delegate();
            if (request < 2) {
                void (^reply)(NSString *, NSError *) = ^(NSString *status, NSError *error) {
                    QVERIFY(token != nil);
                    QVERIFY(error == nil);
                    QCOMPARE(QString::fromNSString(status), u"NOP"_s);
                    completed = true;
                };
                if (request == 0) {
                    [delegate retrieveFileStatusForPath:@"/outside-sync-root" completionHandler:reply];
                } else {
                    [delegate retrieveFolderStatusForPath:@"/outside-sync-root" completionHandler:reply];
                }
            } else if (request == 2) {
                [delegate getMenuItemsForPaths:@[ @"/outside-sync-root" ]
                             completionHandler:^(NSArray<NSDictionary *> *items, NSError *error) {
                                 QVERIFY(token != nil);
                                 QVERIFY(error == nil);
                                 QCOMPARE(items.count, 0);
                                 completed = true;
                             }];
            } else {
                [delegate executeMenuCommand:@"ACTIVITY"
                                    forPaths:@[ @"/outside-sync-root" ]
                           completionHandler:^(NSError *error) {
                               QVERIFY(token != nil);
                               QVERIFY(error == nil);
                               completed = true;
                           }];
            }
        }
        QVERIFY(!completed);
        QVERIFY(weakToken != nil);
        @autoreleasepool {
            QTRY_VERIFY(completed);
        }
        QVERIFY(weakToken == nil);
    }

    void queuedFinderReplyReportsOwnerDestruction()
    {
        OCC::SocketApi socketApi;
        auto service = std::make_unique<OCC::Mac::FinderSyncService>();
        service->setSocketApi(&socketApi);
        __block auto completed = false;
        const auto delegate = (__bridge id<FinderSyncAppProtocol>)service->delegate();
        [delegate getMenuItemsForPaths:@[ @"/outside-sync-root" ]
                     completionHandler:^(NSArray<NSDictionary *> *items, NSError *error) {
                         QVERIFY(items == nil);
                         QCOMPARE(error.code, 1);
                         QCOMPARE(QString::fromNSString(error.domain), QString::fromLatin1(APPLICATION_REV_DOMAIN ".FinderSyncService"));
                         completed = true;
                     }];
        service.reset();
        QTRY_VERIFY(completed);
    }

    void destroyingQueuedReplyContextReleasesItsCapture()
    {
        __weak id weakToken = nil;
        __block auto completed = false;
        @autoreleasepool {
            auto socketApi = std::make_unique<OCC::SocketApi>();
            auto service = std::make_unique<OCC::Mac::FinderSyncService>();
            service->setSocketApi(socketApi.get());
            const auto token = [NSObject new];
            weakToken = token;
            const auto delegate = (__bridge id<FinderSyncAppProtocol>)service->delegate();
            [delegate retrieveFileStatusForPath:@"/outside-sync-root"
                              completionHandler:^(NSString *, NSError *) {
                                  QVERIFY(token != nil);
                                  completed = true;
                              }];
            service.reset();
            socketApi.reset();
        }
        QVERIFY(!completed);
        QVERIFY(weakToken == nil);
    }

    void finderStoredReferencesAreReleasedAndConnectionsInvalidatedOnTeardown()
    {
        __weak XPCServiceTestPeer *weakProxy = nil;
        __weak XPCServiceTestPeer *weakConnection = nil;
        __block auto invalidated = false;
        __block auto callbackRan = false;
        @autoreleasepool {
            auto finderSync = std::make_unique<OCC::Mac::FinderSyncXPC>();
            @autoreleasepool {
                const auto proxy = [XPCServiceTestPeer new];
                const auto connection = [XPCServiceTestPeer new];
                weakProxy = proxy;
                weakConnection = connection;
                connection.didInvalidate = ^{ invalidated = true; };
                connection.invalidationHandler = ^{ callbackRan = true; };
                finderSync->storeExtensionProxy(u"connection"_s, (__bridge_retained void *)proxy, (__bridge_retained void *)connection);
            }
            QVERIFY(weakProxy != nil);
            QVERIFY(weakConnection != nil);
            finderSync->registerPath(u"/sync-root"_s);
            QCOMPARE(QString::fromNSString(weakProxy.registeredPath), u"/sync-root"_s);
            finderSync.reset();
        }
        QVERIFY(invalidated);
        QVERIFY(!callbackRan);
        QVERIFY(weakProxy == nil);
        QVERIFY(weakConnection == nil);
    }

    void finderNativeConnectionHandlersDoNotKeepConnectionAlive()
    {
        __weak XPCServiceTestPeer *weakConnection = nil;
        __weak XPCServiceTestPeer *weakProxy = nil;
        @autoreleasepool {
            OCC::Mac::FinderSyncXPC finderSync;
            OCC::Mac::FinderSyncService service;
            const auto delegate = listenerDelegateFor(&finderSync, &service);
            QVERIFY(delegate != nil);
            const auto listener = [NSXPCListener anonymousListener];
            @autoreleasepool {
                const auto proxy = [XPCServiceTestPeer new];
                const auto connection = [XPCServiceTestPeer new];
                connection.remoteObject = proxy;
                weakProxy = proxy;
                weakConnection = connection;
                QVERIFY([(id<NSXPCListenerDelegate>)delegate listener:listener shouldAcceptNewConnection:(NSXPCConnection *)connection]);
            }
            QVERIFY(finderSync.hasActiveConnections());
            @autoreleasepool {
                const auto interrupted = weakConnection.interruptionHandler;
                QVERIFY(interrupted != nil);
                interrupted();
            }
            QVERIFY(!finderSync.hasActiveConnections());
        }
        QVERIFY(weakConnection == nil);
        QVERIFY(weakProxy == nil);
    }

    void finderListenerRejectsConnectionsAfterServiceDestruction()
    {
        OCC::Mac::FinderSyncXPC finderSync;
        auto service = std::make_unique<OCC::Mac::FinderSyncService>();
        const auto delegate = listenerDelegateFor(&finderSync, service.get());
        QVERIFY(delegate != nil);
        service.reset();
        const auto listener = [NSXPCListener anonymousListener];
        const auto connection = [XPCServiceTestPeer new];
        QVERIFY(![(id<NSXPCListenerDelegate>)delegate listener:listener shouldAcceptNewConnection:(NSXPCConnection *)connection]);
        QVERIFY(!finderSync.hasActiveConnections());
    }

    void finderInvalidationConsumesCallbackAndStoredReferences()
    {
        __weak XPCServiceTestPeer *weakProxy = nil;
        __weak XPCServiceTestPeer *weakConnection = nil;
        @autoreleasepool {
            OCC::Mac::FinderSyncXPC finderSync;
            const auto proxy = [XPCServiceTestPeer new];
            const auto connection = [XPCServiceTestPeer new];
            weakProxy = proxy;
            weakConnection = connection;
            finderSync.storeExtensionProxy(u"connection"_s, (__bridge_retained void *)proxy, (__bridge_retained void *)connection);
            QVERIFY(finderSync.hasActiveConnections());
            finderSync.removeExtensionProxy((__bridge_retained void *)connection);
            QVERIFY(!finderSync.hasActiveConnections());
            finderSync.removeExtensionProxy((__bridge_retained void *)connection);
        }
        QVERIFY(weakProxy == nil);
        QVERIFY(weakConnection == nil);
    }

    void finderProxySnapshotSurvivesInvalidationDuringCall()
    {
        __weak XPCServiceTestPeer *weakProxy = nil;
        __weak XPCServiceTestPeer *weakConnection = nil;
        __block auto completed = false;
        @autoreleasepool {
            OCC::Mac::FinderSyncXPC finderSync;
            auto *const owner = &finderSync;
            @autoreleasepool {
                const auto proxy = [XPCServiceTestPeer new];
                const auto connection = [XPCServiceTestPeer new];
                weakProxy = proxy;
                weakConnection = connection;
                proxy.registerPathHandler = ^(NSString *) {
                    const auto connection = weakConnection;
                    owner->removeExtensionProxy((__bridge_retained void *)connection);
                    QVERIFY(weakProxy != nil);
                    completed = true;
                };
                finderSync.storeExtensionProxy(u"connection"_s, (__bridge_retained void *)proxy, (__bridge_retained void *)connection);
            }
            @autoreleasepool {
                finderSync.registerPath(u"/sync-root"_s);
            }
            QVERIFY(completed);
            QVERIFY(!finderSync.hasActiveConnections());
        }
        QVERIFY(weakProxy == nil);
        QVERIFY(weakConnection == nil);
    }
};

QTEST_GUILESS_MAIN(TestXPCServiceLifecycle)
#include "testxpcservicelifecycle.moc"
