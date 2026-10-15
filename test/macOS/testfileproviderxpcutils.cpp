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
#include <chrono>
#include <optional>
#include <thread>

#include "macOS/fileprovider.h"
#include "macOS/fileproviderxpc_mac_utils.h"

#import <FileProvider/FileProvider.h>
#import <objc/runtime.h>

static_assert(__has_feature(objc_arc), "File Provider XPC utility tests require ARC.");

using namespace Qt::StringLiterals;

namespace
{

constexpr auto backgroundThreadTimeoutMs = 5000;
constexpr auto passwordDeletionReplyTimeout = std::chrono::seconds{1};
constexpr auto passwordDeletionShortTimeout = std::chrono::milliseconds{50};
constexpr auto latePasswordDeletionReplyDelay = std::chrono::milliseconds{200};
constexpr auto setupTimeoutUpperBoundMs = 7000;
NSString *const communicationServiceName = @"com.nextcloud.desktopclient.ClientCommunicationService";

NSArray<NSFileProviderDomain *> *discoveredDomains = nil;
NSDictionary<NSString *, NSFileProviderManager *> *discoveredManagers = nil;
NSError *domainDiscoveryError = nil;
bool deferDomainDiscovery = false;
bool suppressDomainDiscovery = false;
auto domainManagerLookupCount = 0;
void (^pendingDomainDiscovery)(NSArray<NSFileProviderDomain *> *, NSError *) = nil;

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
@property (nonatomic, retain) NSXPCConnection *connection;
@property (nonatomic, retain) NSError *connectionError;
@property (nonatomic, assign) BOOL deferConnectionReply;
@property (nonatomic, assign) BOOL suppressConnectionReply;
@property (nonatomic, copy) void (^pendingConnectionReply)(NSXPCConnection *, NSError *);
- (void)replyToConnectionRequest;
@end

@implementation TestFileProviderService

- (void)getFileProviderConnectionWithCompletionHandler:(void (^)(NSXPCConnection *, NSError *))completionHandler
{
    if (self.suppressConnectionReply) {
        return;
    }

    if (self.deferConnectionReply) {
        self.pendingConnectionReply = completionHandler;
        return;
    }

    completionHandler(self.connection, self.connectionError);
}

- (void)replyToConnectionRequest
{
    self.pendingConnectionReply(self.connection, self.connectionError);
    self.pendingConnectionReply = nil;
}

- (void)dealloc
{
    [_name release];
    [_connection release];
    [_connectionError release];
    [_pendingConnectionReply release];
    [super dealloc];
}

@end

@interface TestFileProviderManager : NSObject
@property (nonatomic, strong) NSFileProviderService *service;
@property (nonatomic, retain) NSError *serviceError;
@property (nonatomic, assign) BOOL deferServiceReply;
@property (nonatomic, assign) BOOL suppressServiceReply;
@property (nonatomic, copy) void (^pendingServiceReply)(NSFileProviderService *, NSError *);
- (void)replyToServiceRequest;
+ (void)getDomainsWithCompletionHandler:(void (^)(NSArray<NSFileProviderDomain *> *, NSError *))completionHandler;
+ (NSFileProviderManager *)managerForDomain:(NSFileProviderDomain *)domain;
@end

@implementation TestFileProviderManager

- (void)getServiceWithName:(NSFileProviderServiceName)serviceName
             itemIdentifier:(NSFileProviderItemIdentifier)itemIdentifier
          completionHandler:(void (^)(NSFileProviderService *service, NSError *error))completionHandler
{
    if (self.suppressServiceReply) {
        return;
    }

    if (self.deferServiceReply) {
        self.pendingServiceReply = completionHandler;
        return;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{ completionHandler(self.service, self.serviceError); });
}

- (void)replyToServiceRequest
{
    self.pendingServiceReply(self.service, self.serviceError);
    self.pendingServiceReply = nil;
}

+ (void)getDomainsWithCompletionHandler:(void (^)(NSArray<NSFileProviderDomain *> *, NSError *))completionHandler
{
    if (suppressDomainDiscovery) {
        return;
    }

    if (deferDomainDiscovery) {
        pendingDomainDiscovery = [completionHandler copy];
        return;
    }

    completionHandler(discoveredDomains, domainDiscoveryError);
}

+ (NSFileProviderManager *)managerForDomain:(NSFileProviderDomain *)domain
{
    ++domainManagerLookupCount;
    return discoveredManagers[domain.identifier];
}

- (void)dealloc
{
    [_service release];
    [_serviceError release];
    [_pendingServiceReply release];
    [super dealloc];
}

@end

@interface TestClientCommunicationService : NSObject <ClientCommunicationProtocol>
@property (nonatomic, copy) NSString *domainIdentifier;
@property (nonatomic, strong) NSError *error;
@property (nonatomic, assign) BOOL *deallocatedFlag;
@property (nonatomic, assign) BOOL suppressDomainIdentifierReply;
@property (nonatomic, assign) BOOL passwordDeletionReply;
@property (nonatomic, assign) BOOL suppressPasswordDeletionReply;
@property (nonatomic, assign) NSTimeInterval passwordDeletionReplyDelay;
@property (atomic, assign) NSUInteger passwordDeletionRequestCount;
@property (atomic, assign) BOOL passwordDeletionReplied;
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

- (void)deletePasswordWithCompletionHandler:(void (^)(BOOL))completionHandler
{
    self.passwordDeletionRequestCount += 1;

    if (self.suppressPasswordDeletionReply) {
        return;
    }

    const auto reply = self.passwordDeletionReply;

    if (self.passwordDeletionReplyDelay > 0) {
        // The copied block retains self and the completion handler until it has run.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(self.passwordDeletionReplyDelay * NSEC_PER_SEC)),
                       dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0),
                       ^{
                           completionHandler(reply);
                           self.passwordDeletionReplied = YES;
                       });
        return;
    }

    completionHandler(reply);
    self.passwordDeletionReplied = YES;
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

    bool domainLookupMocksInstalled = false;

    void installDomainLookupMocks()
    {
        for (const auto selector : { @selector(getDomainsWithCompletionHandler:), @selector(managerForDomain:) }) {
            method_exchangeImplementations(class_getClassMethod(NSFileProviderManager.class, selector),
                                           class_getClassMethod(TestFileProviderManager.class, selector));
        }
        domainLookupMocksInstalled = true;
    }

private Q_SLOTS:
    void cleanup()
    {
        if (domainLookupMocksInstalled) {
            for (const auto selector : { @selector(getDomainsWithCompletionHandler:), @selector(managerForDomain:) }) {
                method_exchangeImplementations(class_getClassMethod(NSFileProviderManager.class, selector),
                                               class_getClassMethod(TestFileProviderManager.class, selector));
            }
            domainLookupMocksInstalled = false;
        }

        [pendingDomainDiscovery release];
        pendingDomainDiscovery = nil;
        [discoveredDomains release];
        discoveredDomains = nil;
        [discoveredManagers release];
        discoveredManagers = nil;
        [domainDiscoveryError release];
        domainDiscoveryError = nil;
        deferDomainDiscovery = false;
        suppressDomainDiscovery = false;
        domainManagerLookupCount = 0;
    }

    void domainDiscoveryCollectsAvailableManagers()
    {
        installDomainLookupMocks();
        const auto availableDomain = [[NSFileProviderDomain alloc] initWithIdentifier:@"available-domain" displayName:@"Available domain"];
        const auto unavailableDomain = [[NSFileProviderDomain alloc] initWithIdentifier:@"unavailable-domain" displayName:@"Unavailable domain"];
        const auto manager = [TestFileProviderManager new];
        discoveredDomains = [[NSArray alloc] initWithObjects:availableDomain, unavailableDomain, nil];
        discoveredManagers = [[NSDictionary alloc] initWithObjectsAndKeys:(NSFileProviderManager *)manager, availableDomain.identifier, nil];

        const auto managers = OCC::Mac::FileProviderXPCUtils::getDomainManagers();
        QCOMPARE(managers.count, 1);
        QVERIFY(managers.firstObject == (NSFileProviderManager *)manager);

        [managers release];
        [manager release];
        [availableDomain release];
        [unavailableDomain release];
    }

    void domainDiscoveryErrorAndEmptyResult_data()
    {
        QTest::addColumn<bool>("hasError");
        QTest::newRow("empty domains") << false;
        QTest::newRow("discovery error") << true;
    }

    void domainDiscoveryErrorAndEmptyResult()
    {
        QFETCH(bool, hasError);
        installDomainLookupMocks();
        discoveredDomains = [[NSArray alloc] init];
        if (hasError) {
            domainDiscoveryError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        }

        const auto managers = OCC::Mac::FileProviderXPCUtils::getDomainManagers();
        QCOMPARE(managers.count, 0);
        [managers release];
    }

    void domainDiscoveryTimesOutAndIgnoresLateReply_data()
    {
        QTest::addColumn<bool>("discardReply");
        QTest::newRow("discarded reply") << true;
        QTest::newRow("late reply") << false;
    }

    void domainDiscoveryTimesOutAndIgnoresLateReply()
    {
        QFETCH(bool, discardReply);
        installDomainLookupMocks();
        deferDomainDiscovery = true;
        suppressDomainDiscovery = discardReply;

        @autoreleasepool {
            QElapsedTimer elapsed;
            elapsed.start();
            const auto managers = OCC::Mac::FileProviderXPCUtils::getDomainManagers();
            QVERIFY2(elapsed.elapsed() < setupTimeoutUpperBoundMs, "Domain discovery did not return within the setup timeout bound.");
            QCOMPARE(managers.count, 0);
            [managers release];
        }

        if (discardReply) {
            QVERIFY(pendingDomainDiscovery == nil);
            return;
        }

        QVERIFY(pendingDomainDiscovery != nil);
        const auto domain = [[NSFileProviderDomain alloc] initWithIdentifier:@"late-domain" displayName:@"Late domain"];
        const auto manager = [TestFileProviderManager new];
        discoveredManagers = [[NSDictionary alloc] initWithObjectsAndKeys:(NSFileProviderManager *)manager, domain.identifier, nil];
        pendingDomainDiscovery(@[ domain ], nil);
        QCOMPARE(domainManagerLookupCount, 0);
        [pendingDomainDiscovery release];
        pendingDomainDiscovery = nil;
        [manager release];
        [domain release];
    }

    void serviceLookupTimesOutWithStablePartialResults_data()
    {
        QTest::addColumn<bool>("includeImmediateReply");
        QTest::addColumn<bool>("discardReplies");
        QTest::newRow("discarded replies") << false << true;
        QTest::newRow("late replies") << false << false;
        QTest::newRow("partial replies") << true << false;
    }

    void serviceLookupTimesOutWithStablePartialResults()
    {
        QFETCH(bool, includeImmediateReply);
        QFETCH(bool, discardReplies);
        const auto service = [TestFileProviderService new];
        service.name = communicationServiceName;
        const auto immediateManager = [TestFileProviderManager new];
        immediateManager.service = (NSFileProviderService *)service;
        const auto lateManager = [TestFileProviderManager new];
        lateManager.service = (NSFileProviderService *)service;
        lateManager.deferServiceReply = YES;
        lateManager.suppressServiceReply = discardReplies;
        const auto laterManager = [TestFileProviderManager new];
        laterManager.service = (NSFileProviderService *)service;
        laterManager.deferServiceReply = YES;
        laterManager.suppressServiceReply = discardReplies;

        @autoreleasepool {
            const auto managers = [NSMutableArray arrayWithObjects:(NSFileProviderManager *)lateManager, (NSFileProviderManager *)laterManager, nil];
            if (includeImmediateReply) {
                [managers addObject:(NSFileProviderManager *)immediateManager];
            }

            QElapsedTimer elapsed;
            elapsed.start();
            const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(managers);
            QVERIFY2(elapsed.elapsed() < setupTimeoutUpperBoundMs, "Service lookup did not return within the setup timeout bound.");
            QVERIFY(services != nil);
            QCOMPARE(services.count, includeImmediateReply ? 1 : 0);
            if (!discardReplies) {
                [lateManager replyToServiceRequest];
            }
            QCOMPARE(services.count, includeImmediateReply ? 1 : 0);
            if (includeImmediateReply) {
                QVERIFY(services.firstObject[communicationServiceName] == (NSFileProviderService *)service);
            }
            [services release];
        }

        if (!discardReplies) {
            [laterManager replyToServiceRequest];
        }
        [immediateManager release];
        [lateManager release];
        [laterManager release];
        [service release];
    }

    void connectionLookupTimesOutAndInvalidatesLateConnections_data()
    {
        QTest::addColumn<bool>("includeImmediateReply");
        QTest::addColumn<bool>("discardReplies");
        QTest::newRow("discarded replies") << false << true;
        QTest::newRow("late replies") << false << false;
        QTest::newRow("partial replies") << true << false;
    }

    void connectionLookupTimesOutAndInvalidatesLateConnections()
    {
        QFETCH(bool, includeImmediateReply);
        QFETCH(bool, discardReplies);
        const auto immediateConnection = [TestXPCConnection new];
        const auto lateConnection = [TestXPCConnection new];
        const auto laterConnection = [TestXPCConnection new];
        const auto immediateService = [TestFileProviderService new];
        immediateService.connection = (NSXPCConnection *)immediateConnection;
        const auto lateService = [TestFileProviderService new];
        lateService.connection = (NSXPCConnection *)lateConnection;
        lateService.deferConnectionReply = YES;
        lateService.suppressConnectionReply = discardReplies;
        const auto laterService = [TestFileProviderService new];
        laterService.connection = (NSXPCConnection *)laterConnection;
        laterService.deferConnectionReply = YES;
        laterService.suppressConnectionReply = discardReplies;

        @autoreleasepool {
            const auto services = [NSMutableArray arrayWithObjects:@{communicationServiceName : (NSFileProviderService *)lateService},
                                                                   @{communicationServiceName : (NSFileProviderService *)laterService},
                                                                   nil];
            if (includeImmediateReply) {
                [services addObject:@{communicationServiceName : (NSFileProviderService *)immediateService}];
            }

            QElapsedTimer elapsed;
            elapsed.start();
            const auto connections = OCC::Mac::FileProviderXPCUtils::connectToFileProviderServices(services);
            QVERIFY2(elapsed.elapsed() < setupTimeoutUpperBoundMs, "Connection lookup did not return within the setup timeout bound.");
            QVERIFY(connections != nil);
            QCOMPARE(connections.count, includeImmediateReply ? 1 : 0);
            if (!discardReplies) {
                [lateService replyToConnectionRequest];
                QVERIFY(lateConnection.invalidated);
            }
            QCOMPARE(connections.count, includeImmediateReply ? 1 : 0);
            if (includeImmediateReply) {
                QVERIFY(connections.firstObject == (NSXPCConnection *)immediateConnection);
                QVERIFY(!immediateConnection.invalidated);
            }
            [connections release];
        }

        if (!discardReplies) {
            [laterService replyToConnectionRequest];
            QVERIFY(laterConnection.invalidated);
        }
        [immediateService release];
        [lateService release];
        [laterService release];
        [immediateConnection release];
        [lateConnection release];
        [laterConnection release];
    }

    void emptySetupInputsReturnImmediately()
    {
        QElapsedTimer elapsed;
        elapsed.start();
        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(@[]);
        const auto connections = OCC::Mac::FileProviderXPCUtils::connectToFileProviderServices(@[]);
        QVERIFY(elapsed.elapsed() < backgroundThreadTimeoutMs);
        QCOMPARE(services.count, 0);
        QCOMPARE(connections.count, 0);
        [services release];
        [connections release];
    }

    void serviceLookupRejectsErrorsAndMissingServices_data()
    {
        QTest::addColumn<bool>("hasError");
        QTest::newRow("missing service") << false;
        QTest::newRow("lookup error") << true;
    }

    void serviceLookupRejectsErrorsAndMissingServices()
    {
        QFETCH(bool, hasError);
        const auto manager = [TestFileProviderManager new];
        const auto service = [TestFileProviderService new];
        service.name = communicationServiceName;
        if (hasError) {
            manager.service = (NSFileProviderService *)service;
            manager.serviceError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        }

        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(@[ (NSFileProviderManager *)manager ]);
        QCOMPARE(services.count, 0);
        [services release];
        [manager release];
        [service release];
    }

    void connectionLookupCollectsOnlySuccessfulCommunicationServices()
    {
        const auto connection = [TestXPCConnection new];
        const auto failedConnection = [TestXPCConnection new];
        const auto successfulService = [TestFileProviderService new];
        successfulService.connection = (NSXPCConnection *)connection;
        const auto failedService = [TestFileProviderService new];
        failedService.connection = (NSXPCConnection *)failedConnection;
        failedService.connectionError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        const auto missingConnectionService = [TestFileProviderService new];
        const auto unrelatedService = [TestFileProviderService new];
        unrelatedService.deferConnectionReply = YES;

        const auto connections = OCC::Mac::FileProviderXPCUtils::connectToFileProviderServices(@[
            @{communicationServiceName : (NSFileProviderService *)successfulService},
            @{communicationServiceName : (NSFileProviderService *)failedService},
            @{communicationServiceName : (NSFileProviderService *)missingConnectionService},
            @{@"unrelated-service" : (NSFileProviderService *)unrelatedService}
        ]);

        QCOMPARE(connections.count, 1);
        QVERIFY(connections.firstObject == (NSXPCConnection *)connection);
        QVERIFY(!connection.invalidated);
        QVERIFY(failedConnection.invalidated);
        QVERIFY(unrelatedService.pendingConnectionReply == nil);
        [connections release];
        [successfulService release];
        [failedService release];
        [missingConnectionService release];
        [unrelatedService release];
        [connection release];
        [failedConnection release];
    }

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

    void passwordDeletionReturnsConfirmedReply()
    {
        const auto service = [TestClientCommunicationService new];
        service.passwordDeletionReply = YES;

        const auto deleted = OCC::Mac::FileProviderXPCUtils::deletePassword((NSObject<ClientCommunicationProtocol> *)service, passwordDeletionReplyTimeout);

        QVERIFY(deleted.has_value());
        QVERIFY(*deleted);
        QCOMPARE(service.passwordDeletionRequestCount, NSUInteger{1});
    }

    void passwordDeletionReturnsFailedReply()
    {
        const auto service = [TestClientCommunicationService new];
        service.passwordDeletionReply = NO;

        const auto deleted = OCC::Mac::FileProviderXPCUtils::deletePassword((NSObject<ClientCommunicationProtocol> *)service, passwordDeletionReplyTimeout);

        QVERIFY(deleted.has_value());
        QVERIFY(!*deleted);
        QCOMPARE(service.passwordDeletionRequestCount, NSUInteger{1});
    }

    void passwordDeletionWithoutServiceReturnsUnknown()
    {
        const auto deleted = OCC::Mac::FileProviderXPCUtils::deletePassword(nil, passwordDeletionReplyTimeout);

        QVERIFY(!deleted.has_value());
    }

    void passwordDeletionWithoutReplyTimesOut()
    {
        const auto service = [TestClientCommunicationService new];
        service.suppressPasswordDeletionReply = YES;

        const auto deleted = OCC::Mac::FileProviderXPCUtils::deletePassword((NSObject<ClientCommunicationProtocol> *)service, passwordDeletionShortTimeout);

        QVERIFY(!deleted.has_value());
        QCOMPARE(service.passwordDeletionRequestCount, NSUInteger{1});
    }

    void latePasswordDeletionReplyAfterTimeoutIsIgnored()
    {
        BOOL serviceDeallocated = NO;
        @autoreleasepool {
            const auto service = [TestClientCommunicationService new];
            service.passwordDeletionReply = YES;
            service.passwordDeletionReplyDelay = std::chrono::duration<double>(latePasswordDeletionReplyDelay).count();
            service.deallocatedFlag = &serviceDeallocated;

            const auto deleted = OCC::Mac::FileProviderXPCUtils::deletePassword((NSObject<ClientCommunicationProtocol> *)service, passwordDeletionShortTimeout);

            QVERIFY(!deleted.has_value());
            QTRY_VERIFY_WITH_TIMEOUT(service.passwordDeletionReplied, backgroundThreadTimeoutMs);
        }
        QTRY_VERIFY_WITH_TIMEOUT(serviceDeallocated, backgroundThreadTimeoutMs);
    }

    void passwordDeletionForUnregisteredDomainReturnsUnknown()
    {
        if (!OCC::Mac::FileProvider::available()) {
            QSKIP("File Provider is unavailable on this macOS version.");
        }

        const auto domain = [[NSFileProviderDomain alloc] initWithIdentifier:@"unregistered-domain" displayName:@"Unregistered"];

        const auto deleted = OCC::Mac::FileProviderXPCUtils::deletePassword(domain, nullptr);

        QVERIFY(!deleted.has_value());
    }
};

QTEST_GUILESS_MAIN(TestFileProviderXPCUtils)
#include "testfileproviderxpcutils.moc"
