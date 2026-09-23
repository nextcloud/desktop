/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QThread>
#include <QtTest>

#include <atomic>
#include <thread>

#include "common/utility.h"
#include "macOS/fileprovider.h"
#include "macOS/fileproviderxpc_mac_utils.h"

#import <FileProvider/FileProvider.h>

@interface TestFileProviderService : NSObject
@property (nonatomic, copy) NSString *name;
@end

@implementation TestFileProviderService
@end

@interface TestFileProviderManager : NSObject
@property (nonatomic, retain) NSFileProviderService *service;
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
@property (nonatomic, retain) NSError *error;
@property (nonatomic, assign) BOOL *deallocatedFlag;
@end

@implementation TestClientCommunicationService

- (void)getFileProviderDomainIdentifierWithCompletionHandler:(void (^)(NSString *, NSError *))completionHandler
{
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

    [_domainIdentifier release];
    [_error release];
    [super dealloc];
}

@end

@interface TestXPCConnection : NSObject
@property (nonatomic, retain) NSObject<ClientCommunicationProtocol> *remoteService;
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

    [_remoteService release];
    [super dealloc];
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
            [service release];
            [manager release];
        }

        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(managers);
        QCOMPARE(services.count, managerCount);
        [services release];
    }

    void serviceWithoutNameIsIgnored()
    {
        const auto service = [TestFileProviderService new];
        const auto manager = [TestFileProviderManager new];
        manager.service = (NSFileProviderService *)service;

        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(@[(NSFileProviderManager *)manager]);
        QCOMPARE(services.count, 0);

        [services release];
        [service release];
        [manager release];
    }

    void clientCommunicationDomainIdentifierCanBeReleasedByCaller()
    {
        const auto service = [TestClientCommunicationService new];
        service.domainIdentifier = @"domain-id";

        const auto domainIdentifier = OCC::Mac::FileProviderXPCUtils::getFileProviderDomainIdentifier((NSObject<ClientCommunicationProtocol> *)service);

        QCOMPARE(QString::fromNSString(domainIdentifier), QStringLiteral("domain-id"));
        [domainIdentifier release];
        [service release];
    }

    void clientCommunicationConnectionValuesHaveOneRetain()
    {
        auto serviceDeallocated = false;
        const auto service = [TestClientCommunicationService new];
        service.domainIdentifier = @"domain-id";
        service.deallocatedFlag = &serviceDeallocated;

        auto connectionDeallocated = false;
        const auto connection = [TestXPCConnection new];
        connection.remoteService = service;
        connection.deallocatedFlag = &connectionDeallocated;

        const auto connections = [[NSMutableArray alloc] initWithObjects:(NSXPCConnection *)connection, nil];

        const auto clientCommConnections = OCC::Mac::FileProviderXPCUtils::processClientCommunicationConnections(connections, nullptr);

        QCOMPARE(clientCommConnections.size(), 1);
        const auto clientCommConnection = clientCommConnections.value(QStringLiteral("domain-id"));
        const auto returnedService = (NSObject *)clientCommConnection.clientCommunicationService;
        const auto returnedConnection = (TestXPCConnection *)clientCommConnection.xpcConnection;
        QVERIFY(returnedService != nil);
        QVERIFY(returnedConnection.resumed);

        [returnedService release];
        [returnedConnection invalidate];
        [returnedConnection release];
        [connections release];
        [connection release];
        [service release];

        QVERIFY(serviceDeallocated);
        QVERIFY(connectionDeallocated);
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

        QTRY_VERIFY_WITH_TIMEOUT(configured.load(std::memory_order_acquire), 5000);
        backgroundThread.join();

        QVERIFY(fileProvider->xpc());
        QCOMPARE(fileProvider->xpc()->thread(), QThread::currentThread());
    }
};

QTEST_APPLESS_MAIN(TestFileProviderXPCUtils)
#include "testfileproviderxpcutils.moc"
