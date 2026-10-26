/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QtTest>

#include <atomic>
#include <optional>
#include <thread>

#include "macOS/fileprovider.h"
#include "macOS/fileproviderxpc_mac_utils.h"

#import <FileProvider/FileProvider.h>

static_assert(__has_feature(objc_arc), "File Provider XPC utility tests require ARC.");

using namespace Qt::StringLiterals;

namespace
{

constexpr auto backgroundThreadTimeoutMs = 5000;

bool waitForBackgroundThread(std::atomic_bool &completed)
{
    QElapsedTimer timer;
    timer.start();

    while (!completed.load(std::memory_order_acquire) && timer.elapsed() < backgroundThreadTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }

    return completed.load(std::memory_order_acquire);
}

} // namespace

@interface TestFileProviderService : NSObject
@property (nonatomic, copy) NSString *name;
@end

@implementation TestFileProviderService
@end

@interface TestFileProviderManager : NSObject
@property (nonatomic, strong) NSFileProviderService *service;
@end

@implementation TestFileProviderManager

- (void)getServiceWithName:(NSFileProviderServiceName)serviceName
             itemIdentifier:(NSFileProviderItemIdentifier)itemIdentifier
          completionHandler:(void (^)(NSFileProviderService *service, NSError *error))completionHandler
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        completionHandler(self.service, nil);
    });
}

@end

@interface TestClientCommunicationService : NSObject <ClientCommunicationProtocol>
@property (nonatomic, copy) NSString *domainIdentifier;
@property (nonatomic, strong) NSError *error;
@property (nonatomic, assign) BOOL *deallocatedFlag;
@property (nonatomic, assign) BOOL suppressDomainIdentifierReply;
@end

@implementation TestClientCommunicationService

- (void)getFileProviderDomainIdentifierWithCompletionHandler:(void (^)(NSString *, NSError *))completionHandler
{
    if (self.suppressDomainIdentifierReply) {
        return;
    }

    completionHandler(self.domainIdentifier, self.error);
}

- (void)hasDirtyUserDataWithCompletionHandler:(void (^)(BOOL))completionHandler
{
    completionHandler(NO);
}

- (void)configureAccountWithUser:(NSString *)user
                          userId:(NSString *)userId
                       serverUrl:(NSString *)serverUrl
                        password:(NSString *)password
                       userAgent:(NSString *)userAgent
{
}

- (void)removeAccountConfig
{
}

- (void)setIgnoreList:(NSArray<NSString *> *)ignoreList
{
}

- (void)processFileIdsChanged:(NSArray<NSNumber *> *)fileIds completionHandler:(void (^)(BOOL))completionHandler
{
    completionHandler(NO);
}

- (void)dealloc
{
    if (self.deallocatedFlag != nullptr) {
        *self.deallocatedFlag = YES;
    }
}

@end

@interface TestXPCConnection : NSObject
@property (nonatomic, strong) NSObject<ClientCommunicationProtocol> *remoteService;
@property (nonatomic, assign) BOOL *deallocatedFlag;
@property (nonatomic, assign) BOOL invalidated;
@property (nonatomic, assign) BOOL resumed;
@end

@implementation TestXPCConnection

- (void)setExportedInterface:(NSXPCInterface *)exportedInterface
{
}

- (void)setExportedObject:(id)exportedObject
{
}

- (void)setRemoteObjectInterface:(NSXPCInterface *)remoteObjectInterface
{
}

- (void)setInterruptionHandler:(dispatch_block_t)interruptionHandler
{
}

- (void)setInvalidationHandler:(dispatch_block_t)invalidationHandler
{
}

- (void)resume
{
    self.resumed = YES;
}

- (void)invalidate
{
    self.invalidated = YES;
}

- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *))errorHandler
{
    return self.remoteService;
}

- (void)dealloc
{
    if (self.deallocatedFlag != nullptr) {
        *self.deallocatedFlag = YES;
    }
}

@end

class TestFileProviderXPCUtils : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void concurrentServiceLookupsCanBeCollected()
    {
        constexpr auto managerCount = 128;
        NSMutableArray<NSFileProviderManager *> * const managers = NSMutableArray.array;

        for (auto index = 0; index < managerCount; ++index) {
            const auto service = [TestFileProviderService new];
            service.name = [NSString stringWithFormat:@"service-%d", index];

            const auto manager = [TestFileProviderManager new];
            manager.service = (NSFileProviderService *)service;
            [managers addObject:(NSFileProviderManager *)manager];
        }

        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(managers);
        QCOMPARE(services.count, managerCount);
    }

    void serviceWithoutNameIsIgnored()
    {
        const auto service = [TestFileProviderService new];
        const auto manager = [TestFileProviderManager new];
        manager.service = (NSFileProviderService *)service;

        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(@[(NSFileProviderManager *)manager]);
        QCOMPARE(services.count, 0);
    }

    void clientCommunicationDomainIdentifierFollowsARCLifetime()
    {
        __weak NSString *weakDomainIdentifier = nil;
        @autoreleasepool {
            const auto service = [TestClientCommunicationService new];
            const auto expectedIdentifier = u"domain-identifier-long-enough-for-ARC-lifetime"_s;
            service.domainIdentifier = expectedIdentifier.toNSString();

            const auto domainIdentifier = OCC::Mac::FileProviderXPCUtils::getFileProviderDomainIdentifier((NSObject<ClientCommunicationProtocol> *)service);
            weakDomainIdentifier = domainIdentifier;

            QCOMPARE(QString::fromNSString(domainIdentifier), expectedIdentifier);
        }
        QVERIFY(weakDomainIdentifier == nil);
    }

    void domainIdentifierRequestTimesOut()
    {
        const auto service = [TestClientCommunicationService new];
        service.suppressDomainIdentifierReply = YES;

        const auto domainIdentifier = OCC::Mac::FileProviderXPCUtils::getFileProviderDomainIdentifier((NSObject<ClientCommunicationProtocol> *)service);

        QVERIFY(domainIdentifier == nil);
    }

    void clientCommunicationConnectionValuesHaveOneRetain()
    {
        BOOL serviceDeallocated = NO;
        BOOL connectionDeallocated = NO;
        @autoreleasepool {
            auto clientCommConnections = OCC::Mac::FileProviderXPCUtils::ClientCommunicationConnections{};
            @autoreleasepool {
                const auto service = [TestClientCommunicationService new];
                service.domainIdentifier = @"domain-id";
                service.deallocatedFlag = &serviceDeallocated;

                const auto connection = [TestXPCConnection new];
                connection.remoteService = service;
                connection.deallocatedFlag = &connectionDeallocated;
                const auto connections = [[NSMutableArray alloc] initWithObjects:(NSXPCConnection *)connection, nil];

                clientCommConnections = OCC::Mac::FileProviderXPCUtils::processClientCommunicationConnections(connections, nullptr);
            }
            QVERIFY(!serviceDeallocated);
            QVERIFY(!connectionDeallocated);
            QCOMPARE(clientCommConnections.size(), 1);
            const auto clientCommConnection = clientCommConnections.value(QStringLiteral("domain-id"));
            const auto returnedService = (__bridge_transfer NSObject *)clientCommConnection.clientCommunicationService;
            const auto returnedConnection = (__bridge_transfer TestXPCConnection *)clientCommConnection.xpcConnection;
            QVERIFY(returnedService != nil);
            QVERIFY(returnedConnection.resumed);
            [returnedConnection invalidate];
        }
        QVERIFY(serviceDeallocated);
        QVERIFY(connectionDeallocated);
    }

    void nilDomainIdentifierInvalidatesConnectionAndReleasesService()
    {
        BOOL serviceDeallocated = NO;
        BOOL connectionDeallocated = NO;
        @autoreleasepool {
            const auto service = [TestClientCommunicationService new];
            service.domainIdentifier = nil;
            service.deallocatedFlag = &serviceDeallocated;

            const auto connection = [TestXPCConnection new];
            connection.remoteService = service;
            connection.deallocatedFlag = &connectionDeallocated;
            const auto connections = [[NSMutableArray alloc] initWithObjects:(NSXPCConnection *)connection, nil];

            const auto clientCommConnections = OCC::Mac::FileProviderXPCUtils::processClientCommunicationConnections(connections, nullptr);

            QCOMPARE(clientCommConnections.size(), 0);
            QVERIFY(connection.invalidated);
        }
        QVERIFY(serviceDeallocated);
        QVERIFY(connectionDeallocated);
    }

    void duplicateDomainIdentifierInvalidatesDuplicateConnectionAndReleasesService()
    {
        BOOL firstServiceDeallocated = NO;
        BOOL firstConnectionDeallocated = NO;
        BOOL duplicateServiceDeallocated = NO;
        BOOL duplicateConnectionDeallocated = NO;
        @autoreleasepool {
            const auto firstService = [TestClientCommunicationService new];
            firstService.domainIdentifier = @"domain-id";
            firstService.deallocatedFlag = &firstServiceDeallocated;

            const auto firstConnection = [TestXPCConnection new];
            firstConnection.remoteService = firstService;
            firstConnection.deallocatedFlag = &firstConnectionDeallocated;

            const auto duplicateService = [TestClientCommunicationService new];
            duplicateService.domainIdentifier = @"domain-id";
            duplicateService.deallocatedFlag = &duplicateServiceDeallocated;

            const auto duplicateConnection = [TestXPCConnection new];
            duplicateConnection.remoteService = duplicateService;
            duplicateConnection.deallocatedFlag = &duplicateConnectionDeallocated;
            const auto connections = [[NSMutableArray alloc] initWithObjects:(NSXPCConnection *)firstConnection, (NSXPCConnection *)duplicateConnection, nil];

            const auto clientCommConnections = OCC::Mac::FileProviderXPCUtils::processClientCommunicationConnections(connections, nullptr);

            QCOMPARE(clientCommConnections.size(), 1);
            QVERIFY(duplicateConnection.invalidated);
            const auto clientCommConnection = clientCommConnections.value(QStringLiteral("domain-id"));
            const auto returnedService = (__bridge_transfer NSObject *)clientCommConnection.clientCommunicationService;
            const auto returnedConnection = (__bridge_transfer TestXPCConnection *)clientCommConnection.xpcConnection;
            QVERIFY(returnedService != nil);
            [returnedConnection invalidate];
        }
        QVERIFY(firstServiceDeallocated);
        QVERIFY(firstConnectionDeallocated);
        QVERIFY(duplicateServiceDeallocated);
        QVERIFY(duplicateConnectionDeallocated);
    }

    void configureXPCFromBackgroundThreadUsesFileProviderThread()
    {
        if (!OCC::Mac::FileProvider::available()) {
            QSKIP("File Provider is unavailable on this macOS version.");
        }

        auto *const fileProvider = OCC::Mac::FileProvider::instance();
        std::atomic_bool configured = false;

        std::thread backgroundThread([fileProvider, &configured] {
            fileProvider->configureXPC();
            configured.store(true, std::memory_order_release);
        });

        const auto configuredInTime = waitForBackgroundThread(configured);
        backgroundThread.join();

        QVERIFY(configuredInTime);
        QVERIFY(fileProvider->xpc());
        QCOMPARE(fileProvider->xpc()->thread(), QThread::currentThread());
    }

    void dirtyUserDataCheckFromBackgroundThreadReturnsUnknownWhenDomainIsMissing()
    {
        if (!OCC::Mac::FileProvider::available()) {
            QSKIP("File Provider is unavailable on this macOS version.");
        }

        auto *const fileProvider = OCC::Mac::FileProvider::instance();
        std::optional<bool> hasDirtyUserData;
        std::atomic_bool completed = false;

        std::thread backgroundThread([fileProvider, &hasDirtyUserData, &completed] {
            hasDirtyUserData = fileProvider->fileProviderDomainHasDirtyUserData("missing-domain"_L1);
            completed.store(true, std::memory_order_release);
        });

        const auto completedInTime = waitForBackgroundThread(completed);
        backgroundThread.join();

        QVERIFY(completedInTime);
        QVERIFY(!hasDirtyUserData.has_value());
    }
};

QTEST_GUILESS_MAIN(TestFileProviderXPCUtils)
#include "testfileproviderxpcutils.moc"
