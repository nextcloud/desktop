/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <atomic>
#include <optional>
#include <thread>

#include "account.h"
#include "accountmanager.h"
#include "configfile.h"
#include "macOS/fileprovider.h"
#include "macOS/fileproviderdomainmanager.h"
#include "macOS/fileprovideritemmetadata.h"
#include "macOS/fileprovidersettingscontroller.h"
#include "macOS/fileproviderutils.h"
#include "macOS/fileproviderutils_mac.h"
#include "macOS/fileproviderxpc_mac_utils.h"
#include "pushnotificationstestutils.h"
#include "testfileprovideritem.h"

#import <FileProvider/FileProvider.h>
#import <objc/runtime.h>

static_assert(__has_feature(objc_arc), "testfileproviderxpcutils requires ARC.");

using namespace Qt::StringLiterals;

namespace
{

constexpr auto backgroundThreadTimeoutMs = 5000;
constexpr auto setupTimeoutUpperBoundMs = 7000;
NSString *const communicationServiceName = @"com.nextcloud.desktopclient.ClientCommunicationService";

NSArray<NSFileProviderDomain *> *discoveredDomains = nil;
NSDictionary<NSString *, NSFileProviderManager *> *discoveredManagers = nil;
NSError *domainDiscoveryError = nil;
bool deferDomainDiscovery = false;
bool suppressDomainDiscovery = false;
auto domainManagerLookupCount = 0;
void (^pendingDomainDiscovery)(NSArray<NSFileProviderDomain *> *, NSError *) = nil;
NSError *domainAdditionError = nil;
NSError *removeDomainError = nil;
bool deferDomainAddition = false;
bool deferDomainRemoval = false;
void (^pendingDomainAddition)(NSError *) = nil;
void (^pendingDomainRemoval)(NSURL *, NSError *) = nil;
NSMutableArray<NSString *> *addedDomainIdentifiers = nil;
NSURL *preservedDataUrl = nil;
auto domainRemovalCount = 0;

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
@property (nonatomic, strong) NSXPCConnection *connection;
@property (nonatomic, strong) NSError *connectionError;
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

@end

@interface TestFileProviderManager : NSObject
@property (nonatomic, strong) NSFileProviderService *service;
@property (nonatomic, strong) NSError *serviceError;
@property (nonatomic, assign) BOOL deferServiceReply;
@property (nonatomic, assign) BOOL suppressServiceReply;
@property (nonatomic, assign) BOOL synchronousServiceReply;
@property (nonatomic, copy) void (^pendingServiceReply)(NSFileProviderService *, NSError *);
@property (nonatomic, strong) NSError *requestError;
@property (nonatomic, assign) BOOL deferRequestReply;
@property (nonatomic, copy) void (^pendingRequestReply)(NSError *);
@property (nonatomic, copy) void (^pendingVisibleUrlReply)(NSURL *, NSError *);
@property (nonatomic, strong) NSURL *visibleUrl;
@property (nonatomic, assign) NSUInteger disconnectCount;
@property (nonatomic, assign) NSUInteger reconnectCount;
@property (nonatomic, assign) NSUInteger signalCount;
@property (nonatomic, assign) NSUInteger visibleUrlCount;
@property (nonatomic, copy) NSString *lastItemIdentifier;
@property (nonatomic, copy) NSString *lastDisconnectionReason;
- (void)replyToServiceRequest;
- (void)finishRequestOrDefer:(void (^)(NSError *))completionHandler;
+ (void)getDomainsWithCompletionHandler:(void (^)(NSArray<NSFileProviderDomain *> *, NSError *))completionHandler;
+ (NSFileProviderManager *)managerForDomain:(NSFileProviderDomain *)domain;
+ (void)addDomain:(NSFileProviderDomain *)domain completionHandler:(void (^)(NSError *))completionHandler;
+ (void)removeDomain:(NSFileProviderDomain *)domain
                 mode:(NSFileProviderDomainRemovalMode)mode
    completionHandler:(void (^)(NSURL *, NSError *))completionHandler;
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

    if (self.synchronousServiceReply) {
        completionHandler(self.service, self.serviceError);
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

+ (void)addDomain:(NSFileProviderDomain *)domain completionHandler:(void (^)(NSError *))completionHandler
{
    [addedDomainIdentifiers addObject:domain.identifier];
    if (deferDomainAddition) {
        pendingDomainAddition = [completionHandler copy];
        return;
    }
    completionHandler(domainAdditionError);
}

+ (void)removeDomain:(NSFileProviderDomain *)domain mode:(NSFileProviderDomainRemovalMode)mode completionHandler:(void (^)(NSURL *, NSError *))completionHandler
{
    ++domainRemovalCount;
    if (deferDomainRemoval) {
        pendingDomainRemoval = [completionHandler copy];
        return;
    }
    completionHandler(preservedDataUrl, removeDomainError);
}

- (void)disconnectWithReason:(NSString *)reason
                     options:(NSFileProviderManagerDisconnectionOptions)options
           completionHandler:(void (^)(NSError *))completionHandler
{
    ++_disconnectCount;
    self.lastDisconnectionReason = reason;
    [self finishRequestOrDefer:completionHandler];
}

- (void)reconnectWithCompletionHandler:(void (^)(NSError *))completionHandler
{
    ++_reconnectCount;
    [self finishRequestOrDefer:completionHandler];
}

- (void)signalEnumeratorForContainerItemIdentifier:(NSFileProviderItemIdentifier)identifier completionHandler:(void (^)(NSError *))completionHandler
{
    ++_signalCount;
    self.lastItemIdentifier = identifier;
    [self finishRequestOrDefer:completionHandler];
}

- (void)finishRequestOrDefer:(void (^)(NSError *))completionHandler
{
    if (self.deferRequestReply) {
        self.pendingRequestReply = completionHandler;
        return;
    }
    completionHandler(self.requestError);
}

- (void)getUserVisibleURLForItemIdentifier:(NSFileProviderItemIdentifier)identifier completionHandler:(void (^)(NSURL *, NSError *))completionHandler
{
    ++_visibleUrlCount;
    self.lastItemIdentifier = identifier;
    if (self.deferRequestReply) {
        self.pendingVisibleUrlReply = completionHandler;
        return;
    }
    completionHandler(self.visibleUrl, self.requestError);
}

@end

@interface TestClientCommunicationService : NSObject <ClientCommunicationProtocol>
@property (nonatomic, copy) NSString *domainIdentifier;
@property (nonatomic, strong) NSError *error;
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

@end

@interface TestXPCConnection : NSObject
@property (nonatomic, strong) NSObject<ClientCommunicationProtocol> *remoteService;
@property (nonatomic, copy) dispatch_block_t invalidationObserver;
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
    if (self.invalidationObserver) {
        self.invalidationObserver();
    }
}

- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *))errorHandler
{
    return self.remoteService;
}

@end

class TestFileProviderXPCUtils : public QObject
{
    Q_OBJECT

public:
    static void initMain()
    {
        qputenv("QT_EVENT_DISPATCHER_CORE_FOUNDATION", "1");
    }

private:
    bool domainLookupMocksInstalled = false;
    QTemporaryDir configDirectory;
    OCC::AccountState *testAccountState = nullptr;

    OCC::AccountState *addTestAccount(const QString &domainIdentifier = {})
    {
        OCC::ConfigFile().setMacFileProviderModeEnabled(false);
        const auto account = OCC::Account::create();
        account->setUrl(QUrl(u"https://cloud.example.test"_s));
        account->setDavUser(u"alice"_s);
        account->setCredentials(new CredentialsStub(u"alice"_s, u"password"_s));
        account->setFileProviderDomainIdentifier(domainIdentifier);
        testAccountState = OCC::AccountManager::instance()->addAccount(account);
        return testAccountState;
    }

    TestFileProviderManager *installRegisteredDomain(const QString &identifier = u"registered-domain"_s)
    {
        const auto domain = [[NSFileProviderDomain alloc] initWithIdentifier:identifier.toNSString() displayName:@"Test domain"];
        const auto manager = [[TestFileProviderManager alloc] init];
        manager.visibleUrl = [NSURL fileURLWithPath:@"/tmp/test-file-provider-item"];
        discoveredDomains = [[NSArray alloc] initWithObjects:domain, nil];
        discoveredManagers = [[NSDictionary alloc] initWithObjectsAndKeys:(NSFileProviderManager *)manager, domain.identifier, nil];
        return manager;
    }

    void exchangeDomainLookupMocks()
    {
        for (const auto selector : {
                 @selector(getDomainsWithCompletionHandler:), @selector(managerForDomain:), @selector(addDomain:completionHandler:),
                     @selector(removeDomain:mode:completionHandler:)
             }) {
            method_exchangeImplementations(class_getClassMethod(NSFileProviderManager.class, selector),
                                           class_getClassMethod(TestFileProviderManager.class, selector));
        }
    }

    void installDomainLookupMocks()
    {
        exchangeDomainLookupMocks();
        domainLookupMocksInstalled = true;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(configDirectory.isValid());
        QVERIFY(OCC::ConfigFile::setConfDir(configDirectory.path()));
        OCC::ConfigFile::setDeviceSourcesFactory([] {
            return std::vector<std::unique_ptr<OCC::SettingSource>>{};
        });
    }

    void cleanup()
    {
        if (testAccountState) {
            testAccountState->account()->setFileProviderDomainIdentifier({});
            OCC::AccountManager::instance()->removeAccountState(testAccountState);
            testAccountState = nullptr;
        }
        if (domainLookupMocksInstalled) {
            exchangeDomainLookupMocks();
            domainLookupMocksInstalled = false;
        }
        pendingDomainDiscovery = nil;
        discoveredDomains = nil;
        discoveredManagers = nil;
        domainDiscoveryError = nil;
        deferDomainDiscovery = false;
        suppressDomainDiscovery = false;
        domainManagerLookupCount = 0;
        domainAdditionError = nil;
        removeDomainError = nil;
        pendingDomainAddition = nil;
        pendingDomainRemoval = nil;
        addedDomainIdentifiers = nil;
        preservedDataUrl = nil;
        deferDomainAddition = false;
        deferDomainRemoval = false;
        domainRemovalCount = 0;
    }

    void domainEnumerationDistinguishesEmptySuccessFromFailure_data()
    {
        QTest::addColumn<bool>("hasError");
        QTest::addColumn<bool>("deferReply");
        QTest::newRow("empty success") << false << false;
        QTest::newRow("error") << true << false;
        QTest::newRow("timeout") << false << true;
    }

    void domainEnumerationDistinguishesEmptySuccessFromFailure()
    {
        QFETCH(bool, hasError);
        QFETCH(bool, deferReply);
        installDomainLookupMocks();
        discoveredDomains = [[NSArray alloc] init];
        deferDomainDiscovery = deferReply;
        if (hasError) {
            domainDiscoveryError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        }
        @autoreleasepool {
            const auto owner = std::make_unique<OCC::Mac::FileProviderDomainManager>();
            auto succeeded = true;
            QElapsedTimer elapsed;
            elapsed.start();
            const auto domains = owner->getDomains(&succeeded);
            QVERIFY(elapsed.elapsed() < setupTimeoutUpperBoundMs);
            QVERIFY(domains.isEmpty());
            QCOMPARE(succeeded, !hasError && !deferReply);
        }
        if (deferReply) {
            pendingDomainDiscovery(@[], nil);
            QCOMPARE(domainManagerLookupCount, 0);
        }
    }

    void managerLookupUsesAcceptedDomainSnapshot_data()
    {
        QTest::addColumn<int>("replyMode");
        QTest::newRow("success") << 0;
        QTest::newRow("missing domain") << 1;
        QTest::newRow("error") << 2;
        QTest::newRow("timeout") << 3;
    }

    void managerLookupUsesAcceptedDomainSnapshot()
    {
        QFETCH(int, replyMode);
        installDomainLookupMocks();
        const auto manager = installRegisteredDomain();
        if (replyMode == 2) {
            domainDiscoveryError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        }
        deferDomainDiscovery = replyMode == 3;
        @autoreleasepool {
            QElapsedTimer elapsed;
            elapsed.start();
            const auto resolved =
                OCC::Mac::FileProviderUtils::managerForDomainIdentifier(replyMode == 1 ? u"missing-domain"_s : QString(u"registered-domain"_s));
            QVERIFY(elapsed.elapsed() < setupTimeoutUpperBoundMs);
            QVERIFY(resolved == (replyMode == 0 ? (NSFileProviderManager *)manager : nil));
        }
        if (replyMode == 3) {
            pendingDomainDiscovery(discoveredDomains, nil);
            QCOMPARE(domainManagerLookupCount, 0);
        }
    }

    void domainRequestsCompleteOrTimeOutSafely_data()
    {
        QTest::addColumn<QString>("operation");
        QTest::addColumn<int>("replyMode");
        for (const auto &operation : {u"disconnect"_s, u"reconnect"_s, u"signal"_s, u"root URL"_s}) {
            for (const auto &[mode, name] : {std::pair{0, "success"}, {1, "error"}, {2, "timeout"}, {3, "missing manager"}}) {
                const auto rowName = (operation + u' ' + QLatin1StringView(name)).toUtf8();
                QTest::newRow(rowName.constData()) << operation << mode;
            }
        }
    }

    void domainRequestsCompleteOrTimeOutSafely()
    {
        QFETCH(QString, operation);
        QFETCH(int, replyMode);
        installDomainLookupMocks();
        const auto manager = installRegisteredDomain();
        const auto accountState = addTestAccount(u"registered-domain"_s);
        if (replyMode == 1) {
            manager.requestError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        }
        manager.deferRequestReply = replyMode == 2;
        if (replyMode == 3) {
            discoveredManagers = [[NSDictionary alloc] init];
        }
        @autoreleasepool {
            const auto owner = std::make_unique<OCC::Mac::FileProviderDomainManager>();
            QElapsedTimer elapsed;
            elapsed.start();
            if (operation == u"disconnect"_s) {
                QVERIFY(QMetaObject::invokeMethod(owner.get(),
                                                  "disconnectFileProviderDomainForAccount",
                                                  Qt::DirectConnection,
                                                  Q_ARG(const OCC::AccountState *, accountState),
                                                  Q_ARG(QString, u"Test disconnection"_s)));
            } else if (operation == u"reconnect"_s) {
                owner->reconnectAll();
            } else if (operation == u"signal"_s) {
                owner->signalEnumeratorChanged(accountState->account().data());
            } else {
                const auto path = owner->userVisibleUrlForDomainIdentifier(u"registered-domain"_s);
                QCOMPARE(path, replyMode == 0 ? u"/tmp/test-file-provider-item"_s : QString());
            }
            QVERIFY(elapsed.elapsed() < setupTimeoutUpperBoundMs);
            if (replyMode == 3) {
                QVERIFY(elapsed.elapsed() < backgroundThreadTimeoutMs);
            }
        }
        if (replyMode == 2) {
            if (operation == u"root URL"_s) {
                QVERIFY(manager.pendingVisibleUrlReply != nil);
                manager.pendingVisibleUrlReply(manager.visibleUrl, nil);
                manager.pendingVisibleUrlReply = nil;
            } else {
                QVERIFY(manager.pendingRequestReply != nil);
                manager.pendingRequestReply(nil);
                manager.pendingRequestReply = nil;
            }
        }
        const auto requested = replyMode == 3 ? 0 : 1;
        QCOMPARE(manager.disconnectCount, operation == u"disconnect"_s ? requested : 0);
        QCOMPARE(manager.reconnectCount, operation == u"reconnect"_s ? requested : 0);
        QCOMPARE(manager.visibleUrlCount, operation == u"root URL"_s ? requested : 0);
        const auto expectedSignals = operation == u"signal"_s ? requested : operation == u"reconnect"_s && replyMode == 0 ? 1 : 0;
        QCOMPARE(manager.signalCount, expectedSignals);
        if (manager.signalCount) {
            QVERIFY([manager.lastItemIdentifier isEqualToString:NSFileProviderWorkingSetContainerItemIdentifier]);
        }
        if (manager.disconnectCount) {
            QVERIFY([manager.lastDisconnectionReason isEqualToString:@"Test disconnection"]);
        }
    }

    void additionRequiresConfirmedDiscoveryAndReusesUnconfirmedIdentifier()
    {
        installDomainLookupMocks();
        const auto accountState = addTestAccount();
        addedDomainIdentifiers = [[NSMutableArray alloc] init];
        discoveredDomains = [[NSArray alloc] init];
        domainDiscoveryError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        auto owner = std::make_unique<OCC::Mac::FileProviderDomainManager>();

        QVERIFY(owner->addDomainForAccount(accountState).isEmpty());
        QCOMPARE(addedDomainIdentifiers.count, 0);
        QVERIFY(accountState->account()->fileProviderDomainIdentifier().isEmpty());
        domainDiscoveryError = nil;

        deferDomainAddition = true;
        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY(owner->addDomainForAccount(accountState).isEmpty());
        QVERIFY(elapsed.elapsed() < setupTimeoutUpperBoundMs);
        QCOMPARE(addedDomainIdentifiers.count, 1);
        QVERIFY(accountState->account()->fileProviderDomainIdentifier().isEmpty());
        QVERIFY(pendingDomainAddition != nil);

        domainAdditionError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:nil];
        deferDomainAddition = false;
        QVERIFY(owner->addDomainForAccount(accountState).isEmpty());
        QCOMPARE(addedDomainIdentifiers.count, 2);
        QVERIFY([addedDomainIdentifiers[0] isEqualToString:addedDomainIdentifiers[1]]);
        QVERIFY(accountState->account()->fileProviderDomainIdentifier().isEmpty());
        domainAdditionError = nil;

        const auto confirmed = owner->addDomainForAccount(accountState);
        QCOMPARE(addedDomainIdentifiers.count, 3);
        QCOMPARE(confirmed, QString::fromNSString(addedDomainIdentifiers.firstObject));
        QCOMPARE(accountState->account()->fileProviderDomainIdentifier(), confirmed);
        @autoreleasepool {
            owner.reset();
        }
        pendingDomainAddition(nil);
        QCOMPARE(accountState->account()->fileProviderDomainIdentifier(), confirmed);
    }

    void missingStoredDomainUsesFreshIdentifierUntilRegistrationIsConfirmed()
    {
        installDomainLookupMocks();
        const auto accountState = addTestAccount(u"stale-domain"_s);
        addedDomainIdentifiers = [[NSMutableArray alloc] init];
        discoveredDomains = [[NSArray alloc] init];
        domainAdditionError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:nil];
        const auto owner = std::make_unique<OCC::Mac::FileProviderDomainManager>();

        QVERIFY(owner->addDomainForAccount(accountState).isEmpty());
        QCOMPARE(addedDomainIdentifiers.count, 1);
        const auto pendingIdentifier = QString::fromNSString(addedDomainIdentifiers.firstObject);
        QVERIFY(!pendingIdentifier.isEmpty());
        QVERIFY(pendingIdentifier != u"stale-domain"_s);
        QCOMPARE(accountState->account()->fileProviderDomainIdentifier(), u"stale-domain"_s);

        QVERIFY(owner->addDomainForAccount(accountState).isEmpty());
        QCOMPARE(addedDomainIdentifiers.count, 2);
        QCOMPARE(QString::fromNSString(addedDomainIdentifiers.lastObject), pendingIdentifier);
        QCOMPARE(accountState->account()->fileProviderDomainIdentifier(), u"stale-domain"_s);
        domainAdditionError = nil;

        installRegisteredDomain(pendingIdentifier);
        QCOMPARE(owner->addDomainForAccount(accountState), pendingIdentifier);
        QCOMPARE(addedDomainIdentifiers.count, 2);
        QCOMPARE(accountState->account()->fileProviderDomainIdentifier(), pendingIdentifier);
    }

    void removeAllDomainsReportsDiscoveryAndRemovalFailures()
    {
        installDomainLookupMocks();
        installRegisteredDomain();
        const auto owner = std::make_unique<OCC::Mac::FileProviderDomainManager>();
        domainDiscoveryError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        QVERIFY(!owner->removeAllDomains());
        QCOMPARE(domainRemovalCount, 0);
        domainDiscoveryError = nil;
        removeDomainError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:nil];
        QVERIFY(!owner->removeAllDomains());
        QCOMPARE(domainRemovalCount, 1);
        removeDomainError = nil;
        QVERIFY(owner->removeAllDomains());
        QCOMPARE(domainRemovalCount, 2);
    }

    void removingAccountWithoutDomainReturnsImmediately()
    {
        installDomainLookupMocks();
        suppressDomainDiscovery = true;
        const auto accountState = addTestAccount();
        OCC::Mac::FileProviderDomainManager owner;
        QElapsedTimer elapsed;
        elapsed.start();
        const auto result = owner.removeDomainByAccount(accountState);
        QVERIFY(elapsed.elapsed() < backgroundThreadTimeoutMs);
        QVERIFY(result.has_value());
        QVERIFY(result->isEmpty());
        QCOMPARE(domainRemovalCount, 0);
    }

    void replyCollectionWithNoRequestsDoesNotStartWork()
    {
        __block auto started = false;
        NSArray *results = nil;
        const auto succeeded = OCC::Mac::FileProviderUtils::waitForFileProviderReplies(
            0,
            "collecting no replies",
            ^(OCC::Mac::FileProviderUtils::RequestReplyHandler reply) {
                Q_UNUSED(reply)
                started = true;
            },
            &results);

        QVERIFY(succeeded);
        QVERIFY(!started);
        QVERIFY(results != nil);
        QCOMPARE(results.count, 0);
        QVERIFY(![results isKindOfClass:NSMutableArray.class]);
    }

    void replyCollectionPreservesOpaqueValuesAndAcceptsNilReplies_data()
    {
        QTest::addColumn<bool>("includeValues");
        QTest::newRow("nil replies") << false;
        QTest::newRow("opaque values") << true;
    }

    void replyCollectionPreservesOpaqueValuesAndAcceptsNilReplies()
    {
        QFETCH(bool, includeValues);
        NSArray *const nestedValue = @[ @"nested" ];
        __block auto acceptedCount = 0;
        __block auto acceptedExtra = YES;
        NSArray *results = nil;
        const auto succeeded = OCC::Mac::FileProviderUtils::waitForFileProviderReplies(
            3,
            "collecting successful replies",
            ^(OCC::Mac::FileProviderUtils::RequestReplyHandler reply) {
                acceptedCount += reply(includeValues ? @"first" : nil, nil);
                acceptedCount += reply(includeValues ? nestedValue : nil, nil);
                acceptedCount += reply(nil, nil);
                acceptedExtra = reply(@"extra", nil);
            },
            &results);

        QVERIFY(succeeded);
        QCOMPARE(acceptedCount, 3);
        QVERIFY(!acceptedExtra);
        QCOMPARE(results.count, includeValues ? 2 : 0);
        QVERIFY(![results isKindOfClass:NSMutableArray.class]);
        if (includeValues) {
            QVERIFY([results[0] isEqualToString:@"first"]);
            QVERIFY([results[1] isEqualToArray:nestedValue]);
        }
    }

    void replyCollectionPreservesSuccessfulValuesWhenAnotherReplyFails()
    {
        __block auto acceptedCount = 0;
        NSArray *results = nil;
        const auto succeeded = OCC::Mac::FileProviderUtils::waitForFileProviderReplies(
            3,
            "collecting replies including an error",
            ^(OCC::Mac::FileProviderUtils::RequestReplyHandler reply) {
                acceptedCount += reply(@"before", nil);
                acceptedCount += reply(@"error payload", [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil]);
                acceptedCount += reply(@"after", nil);
            },
            &results);

        QVERIFY(!succeeded);
        QCOMPARE(acceptedCount, 3);
        NSArray *const expected = @[ @"before", @"after" ];
        QVERIFY([results isEqualToArray:expected]);
        QVERIFY(![results isKindOfClass:NSMutableArray.class]);
    }

    void synchronousRequestRejectsDuplicateReplies()
    {
        id result = @"old value";
        __block auto acceptedFirst = NO;
        __block auto acceptedDuplicate = YES;
        const auto succeeded = OCC::Mac::FileProviderUtils::waitForFileProviderRequest(
            "testing duplicate replies",
            ^(OCC::Mac::FileProviderUtils::RequestReplyHandler reply) {
                acceptedFirst = reply(@"first", nil);
                acceptedDuplicate = reply(@"duplicate", [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil]);
            },
            &result);
        QVERIFY(succeeded);
        QVERIFY(acceptedFirst);
        QVERIFY(!acceptedDuplicate);
        QVERIFY([result isEqualToString:@"first"]);
    }

    void synchronousRequestAcceptsOnlyOneConcurrentReply()
    {
        const auto accepted = std::make_shared<std::atomic_int>(0);
        const auto rejected = std::make_shared<std::atomic_int>(0);
        const auto group = dispatch_group_create();
        id result = nil;
        const auto succeeded = OCC::Mac::FileProviderUtils::waitForFileProviderRequest(
            "testing concurrent replies",
            ^(OCC::Mac::FileProviderUtils::RequestReplyHandler reply) {
                for (NSString *const value in @[ @"first", @"second" ]) {
                    dispatch_group_async(group, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
                        if (reply(value, nil)) {
                            ++*accepted;
                        } else {
                            ++*rejected;
                        }
                    });
                }
            },
            &result);
        const auto completed = dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, setupTimeoutUpperBoundMs * NSEC_PER_MSEC));
        QVERIFY(succeeded);
        QCOMPARE(completed, 0);
        QCOMPARE(accepted->load(), 1);
        QCOMPARE(rejected->load(), 1);
        QVERIFY([result isEqualToString:@"first"] || [result isEqualToString:@"second"]);
    }

    void removalDistinguishesSuccessFromUnknownOutcome_data()
    {
        QTest::addColumn<int>("replyMode");
        QTest::newRow("no dirty data") << 0;
        QTest::newRow("preserved dirty data") << 1;
        QTest::newRow("error") << 2;
        QTest::newRow("timeout") << 3;
    }

    void removalDistinguishesSuccessFromUnknownOutcome()
    {
        QFETCH(int, replyMode);
        installDomainLookupMocks();
        installRegisteredDomain();
        if (replyMode == 1) {
            preservedDataUrl = [NSURL fileURLWithPath:@"/tmp/preserved-file-provider-data"];
        } else if (replyMode == 2) {
            removeDomainError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:nil];
        }
        deferDomainRemoval = replyMode == 3;
        @autoreleasepool {
            const auto owner = std::make_unique<OCC::Mac::FileProviderDomainManager>();
            QElapsedTimer elapsed;
            elapsed.start();
            const auto result = owner->removeDomain(discoveredDomains.firstObject);
            QVERIFY(elapsed.elapsed() < setupTimeoutUpperBoundMs);
            QCOMPARE(result.has_value(), replyMode < 2);
            if (result) {
                QCOMPARE(*result, replyMode == 1 ? u"/tmp/preserved-file-provider-data"_s : QString());
            }
        }
        QCOMPARE(domainRemovalCount, 1);
        if (replyMode == 3) {
            pendingDomainRemoval([NSURL fileURLWithPath:@"/tmp/late-preserved-file-provider-data"], nil);
        }
    }

    void itemVisiblePathCompletesOrTimesOutSafely_data()
    {
        QTest::addColumn<int>("replyMode");
        QTest::newRow("success") << 0;
        QTest::newRow("error") << 1;
        QTest::newRow("timeout") << 2;
        QTest::newRow("missing URL") << 3;
        QTest::newRow("missing manager") << 4;
    }

    void itemVisiblePathCompletesOrTimesOutSafely()
    {
        QFETCH(int, replyMode);
        installDomainLookupMocks();
        const auto manager = installRegisteredDomain();
        if (replyMode == 1) {
            manager.requestError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        }
        manager.deferRequestReply = replyMode == 2;
        if (replyMode == 3) {
            manager.visibleUrl = nil;
        } else if (replyMode == 4) {
            discoveredManagers = [[NSDictionary alloc] init];
        }
        @autoreleasepool {
            const auto item = [TestFileProviderItem new];
            item.itemIdentifier = @"test-item";
            item.parentItemIdentifier = NSFileProviderRootContainerItemIdentifier;
            item.filename = @"test-item.txt";
            item.documentSize = @1;
            QElapsedTimer elapsed;
            elapsed.start();
            const auto metadata = OCC::Mac::FileProviderItemMetadata::fromNSFileProviderItem((__bridge const void *)item, u"registered-domain"_s);
            QVERIFY(elapsed.elapsed() < setupTimeoutUpperBoundMs);
            QCOMPARE(metadata.userVisiblePath(), replyMode == 0 ? u"/tmp/test-file-provider-item"_s : u"Unknown"_s);
        }
        if (replyMode == 2) {
            QVERIFY(manager.pendingVisibleUrlReply != nil);
            manager.pendingVisibleUrlReply(manager.visibleUrl, nil);
            manager.pendingVisibleUrlReply = nil;
        }
        if (replyMode != 4) {
            QVERIFY([manager.lastItemIdentifier isEqualToString:@"test-item"]);
        }
    }

    void settingsMigrationFailureKeepsLegacyMappingsAndXpcAvailable()
    {
        if (!OCC::Mac::FileProvider::available()) {
            QSKIP("File Provider is unavailable on this macOS version.");
        }
        installDomainLookupMocks();
        OCC::ConfigFile config;
        config.setFileProviderDomainsAppSandboxMigrationCompleted(false);
        config.setMacFileProviderModeEnabled(false);
        QSettings legacySettings(config.configFile(), QSettings::IniFormat);
        legacySettings.setValue(u"FileProviderDomainUuids/legacy-account"_s, u"legacy-domain"_s);
        legacySettings.sync();
        const auto oldLegacyAccounts = [NSUserDefaults.standardUserDefaults objectForKey:@"enabledAccounts"];
        const auto restorePreferences = qScopeGuard([oldLegacyAccounts] {
            if (oldLegacyAccounts != nil) {
                [NSUserDefaults.standardUserDefaults setObject:oldLegacyAccounts forKey:@"enabledAccounts"];
            } else {
                [NSUserDefaults.standardUserDefaults removeObjectForKey:@"enabledAccounts"];
            }
            OCC::ConfigFile().setFileProviderDomainsAppSandboxMigrationCompleted(true);
        });
        [NSUserDefaults.standardUserDefaults setObject:@[ @"legacy-account" ] forKey:@"enabledAccounts"];
        domainDiscoveryError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];

        const auto controller = OCC::Mac::FileProviderSettingsController::instance();
        QVERIFY(OCC::Mac::FileProvider::instance()->xpc() != nullptr);
        controller->performStartupReconciliation();
        QVERIFY(!config.fileProviderDomainsAppSandboxMigrationCompleted());
        legacySettings.sync();
        QCOMPARE(legacySettings.value(u"FileProviderDomainUuids/legacy-account"_s).toString(), u"legacy-domain"_s);
        QVERIFY([[NSUserDefaults.standardUserDefaults objectForKey:@"enabledAccounts"] isEqualToArray:@[ @"legacy-account" ]]);
        QCOMPARE(domainRemovalCount, 0);
    }

    void settingsDisableRetainsDomainAfterUnconfirmedRemoval_data()
    {
        QTest::addColumn<bool>("cleanDataKnown");
        QTest::addColumn<bool>("lateReply");
        QTest::newRow("unknown dirty data, native error") << false << false;
        QTest::newRow("clean data, native error") << true << false;
        QTest::newRow("unknown dirty data, late removal") << false << true;
        QTest::newRow("clean data, late removal") << true << true;
    }

    void settingsDisableRetainsDomainAfterUnconfirmedRemoval()
    {
        if (!OCC::Mac::FileProvider::available()) {
            QSKIP("File Provider is unavailable on this macOS version.");
        }
        QFETCH(bool, cleanDataKnown);
        QFETCH(bool, lateReply);
        installDomainLookupMocks();
        OCC::ConfigFile().setFileProviderDomainsAppSandboxMigrationCompleted(true);
        const auto accountState = addTestAccount(u"registered-domain"_s);
        const auto account = accountState->account();
        const auto manager = installRegisteredDomain();
        if (cleanDataKnown) {
            const auto remoteService = [[TestClientCommunicationService alloc] init];
            remoteService.domainIdentifier = @"registered-domain";
            const auto connection = [[TestXPCConnection alloc] init];
            connection.remoteService = remoteService;
            const auto service = [[TestFileProviderService alloc] init];
            service.name = communicationServiceName;
            service.connection = (NSXPCConnection *)connection;
            manager.service = (NSFileProviderService *)service;
        }
        const auto controller = OCC::Mac::FileProviderSettingsController::instance();
        OCC::Mac::FileProvider::instance()->configureXPC();
        const auto dirtyData = OCC::Mac::FileProvider::instance()->fileProviderDomainHasDirtyUserData(u"registered-domain"_s);
        QCOMPARE(dirtyData.has_value(), cleanDataKnown);
        if (dirtyData) {
            QVERIFY(!*dirtyData);
        }
        OCC::ConfigFile().setMacFileProviderModeEnabled(true);
        if (lateReply) {
            deferDomainRemoval = true;
        } else {
            removeDomainError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:nil];
        }
        const QSignalSpy changed(controller, &OCC::Mac::FileProviderSettingsController::vfsEnabledForAccountChanged);
        const QSignalSpy completed(controller, &OCC::Mac::FileProviderSettingsController::fileProviderModeApplyFinished);
        controller->setFileProviderModeEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->isOperationInProgress(), setupTimeoutUpperBoundMs);
        QCOMPARE(account->fileProviderDomainIdentifier(), u"registered-domain"_s);
        QCOMPARE(changed.count(), 0);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(completed.first().at(1).toStringList(), QStringList{account->userIdAtHostWithPort()});
        if (lateReply) {
            QVERIFY(pendingDomainRemoval != nil);
            pendingDomainRemoval(nil, nil);
            QCoreApplication::processEvents();
            QCOMPARE(account->fileProviderDomainIdentifier(), u"registered-domain"_s);
            QCOMPARE(changed.count(), 0);
        }
    }

    void settingsResetDoesNotRecreateDomainAfterFailedRemoval()
    {
        if (!OCC::Mac::FileProvider::available()) {
            QSKIP("File Provider is unavailable on this macOS version.");
        }
        installDomainLookupMocks();
        OCC::ConfigFile().setFileProviderDomainsAppSandboxMigrationCompleted(true);
        const auto accountState = addTestAccount(u"registered-domain"_s);
        installRegisteredDomain();
        const auto controller = OCC::Mac::FileProviderSettingsController::instance();
        OCC::Mac::FileProvider::instance()->configureXPC();
        OCC::ConfigFile().setMacFileProviderModeEnabled(true);
        addedDomainIdentifiers = [[NSMutableArray alloc] init];
        removeDomainError = [[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:nil];
        const QSignalSpy changed(controller, &OCC::Mac::FileProviderSettingsController::vfsEnabledForAccountChanged);

        controller->resetVfsForAccount(accountState->account()->userIdAtHostWithPort());
        QTRY_VERIFY_WITH_TIMEOUT(!controller->isOperationInProgress(), setupTimeoutUpperBoundMs);
        QCOMPARE(accountState->account()->fileProviderDomainIdentifier(), u"registered-domain"_s);
        QCOMPARE(addedDomainIdentifiers.count, 0);
        QCOMPARE(changed.count(), 0);
        OCC::ConfigFile().setMacFileProviderModeEnabled(false);
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
        pendingDomainDiscovery = nil;
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

        __weak NSArray *weakServices = nil;
        @autoreleasepool {
            const auto managers = [NSMutableArray arrayWithObjects:(NSFileProviderManager *)lateManager, (NSFileProviderManager *)laterManager, nil];
            if (includeImmediateReply) {
                [managers addObject:(NSFileProviderManager *)immediateManager];
            }

            QElapsedTimer elapsed;
            elapsed.start();
            auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(managers);
            weakServices = services;
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
            services = nil;
        }
        if (includeImmediateReply) {
            QVERIFY(weakServices == nil);
        }

        if (!discardReplies) {
            [laterManager replyToServiceRequest];
        }
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

        __weak NSArray *weakConnections = nil;
        @autoreleasepool {
            const auto services = [NSMutableArray arrayWithObjects:@{communicationServiceName : (NSFileProviderService *)lateService},
                                                                   @{communicationServiceName : (NSFileProviderService *)laterService},
                                                                   nil];
            if (includeImmediateReply) {
                [services addObject:@{communicationServiceName : (NSFileProviderService *)immediateService}];
            }

            QElapsedTimer elapsed;
            elapsed.start();
            auto connections = OCC::Mac::FileProviderXPCUtils::connectToFileProviderServices(services);
            weakConnections = connections;
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
            connections = nil;
        }
        if (includeImmediateReply) {
            QVERIFY(weakConnections == nil);
        }

        if (!discardReplies) {
            [laterService replyToConnectionRequest];
            QVERIFY(laterConnection.invalidated);
        }
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
        QVERIFY(services != nil);
        QVERIFY(![services isKindOfClass:NSMutableArray.class]);
        QCOMPARE(services.count, 0);
    }

    void connectionLookupCollectsOnlySuccessfulCommunicationServices()
    {
        const auto connection = [TestXPCConnection new];
        const auto otherConnection = [TestXPCConnection new];
        const auto failedConnection = [TestXPCConnection new];
        const auto successfulService = [TestFileProviderService new];
        successfulService.connection = (NSXPCConnection *)connection;
        const auto otherSuccessfulService = [TestFileProviderService new];
        otherSuccessfulService.connection = (NSXPCConnection *)otherConnection;
        const auto failedService = [TestFileProviderService new];
        failedService.connection = (NSXPCConnection *)failedConnection;
        failedService.connectionError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
        const auto missingConnectionService = [TestFileProviderService new];
        const auto unrelatedService = [TestFileProviderService new];
        unrelatedService.deferConnectionReply = YES;

        const auto connections = OCC::Mac::FileProviderXPCUtils::connectToFileProviderServices(@[
            @{communicationServiceName : (NSFileProviderService *)successfulService},
            @{communicationServiceName : (NSFileProviderService *)otherSuccessfulService},
            @{communicationServiceName : (NSFileProviderService *)failedService},
            @{communicationServiceName : (NSFileProviderService *)missingConnectionService},
            @{@"unrelated-service" : (NSFileProviderService *)unrelatedService}
        ]);

        QCOMPARE(connections.count, 2);
        QVERIFY(![connections isKindOfClass:NSMutableArray.class]);
        QVERIFY([connections containsObject:(NSXPCConnection *)connection]);
        QVERIFY([connections containsObject:(NSXPCConnection *)otherConnection]);
        QVERIFY(![connections containsObject:(NSXPCConnection *)failedConnection]);
        QVERIFY(!connection.invalidated);
        QVERIFY(!otherConnection.invalidated);
        QVERIFY(failedConnection.invalidated);
        QVERIFY(unrelatedService.pendingConnectionReply == nil);
    }

    void concurrentServiceLookupsCanBeCollected()
    {
        constexpr auto managerCount = 128;
        NSMutableArray<NSFileProviderManager *> *const managers = NSMutableArray.array;
        NSMutableDictionary<NSString *, NSFileProviderService *> *const expectedServices = NSMutableDictionary.dictionary;

        for (auto index = 0; index < managerCount; ++index) {
            const auto service = [TestFileProviderService new];
            service.name = [NSString stringWithFormat:@"service-%d", index];

            const auto manager = [TestFileProviderManager new];
            manager.service = (NSFileProviderService *)service;
            [managers addObject:(NSFileProviderManager *)manager];
            expectedServices[service.name] = (NSFileProviderService *)service;
        }

        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(managers);
        QCOMPARE(services.count, managerCount);
        QVERIFY(![services isKindOfClass:NSMutableArray.class]);
        NSMutableSet<NSString *> *const resolvedNames = NSMutableSet.set;
        for (id const entry in services) {
            QVERIFY([entry isKindOfClass:NSDictionary.class]);
            NSDictionary<NSString *, NSFileProviderService *> *const serviceDictionary = entry;
            QCOMPARE(serviceDictionary.count, 1);
            NSString *const name = serviceDictionary.allKeys.firstObject;
            QVERIFY(expectedServices[name] != nil);
            QVERIFY(serviceDictionary[name] == expectedServices[name]);
            [resolvedNames addObject:name];
        }
        QCOMPARE(resolvedNames.count, managerCount);
    }

    void serviceWithoutNameIsIgnored()
    {
        const auto service = [TestFileProviderService new];
        const auto manager = [TestFileProviderManager new];
        manager.service = (NSFileProviderService *)service;

        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(@[ (NSFileProviderManager *)manager ]);
        QCOMPARE(services.count, 0);
    }

    void clientCommunicationDomainIdentifierCanBeReleasedByCaller()
    {
        const auto service = [TestClientCommunicationService new];
        service.domainIdentifier = @"domain-id";

        const auto domainIdentifier = OCC::Mac::FileProviderXPCUtils::getFileProviderDomainIdentifier((NSObject<ClientCommunicationProtocol> *)service);

        QCOMPARE(QString::fromNSString(domainIdentifier), QStringLiteral("domain-id"));
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
        __weak TestClientCommunicationService *weakService = nil;
        __weak TestXPCConnection *weakConnection = nil;
        @autoreleasepool {
            const auto service = [TestClientCommunicationService new];
            service.domainIdentifier = @"domain-id";
            weakService = service;

            const auto connection = [TestXPCConnection new];
            connection.remoteService = service;
            weakConnection = connection;
            const auto connections = [[NSMutableArray alloc] initWithObjects:(NSXPCConnection *)connection, nil];
            const auto clientCommConnections = OCC::Mac::FileProviderXPCUtils::processClientCommunicationConnections(connections, nullptr);

            QCOMPARE(clientCommConnections.size(), 1);
            const auto clientCommConnection = clientCommConnections.value(QStringLiteral("domain-id"));
            const auto returnedService = (__bridge_transfer NSObject *)clientCommConnection.clientCommunicationService;
            const auto returnedConnection = (__bridge_transfer TestXPCConnection *)clientCommConnection.xpcConnection;
            QVERIFY(returnedService != nil);
            QVERIFY(returnedConnection.resumed);
            [returnedConnection invalidate];
        }
        QVERIFY(weakService == nil);
        QVERIFY(weakConnection == nil);
    }

    void nilDomainIdentifierInvalidatesConnectionAndReleasesService()
    {
        __weak TestClientCommunicationService *weakService = nil;
        __weak TestXPCConnection *weakConnection = nil;
        @autoreleasepool {
            const auto service = [TestClientCommunicationService new];
            service.domainIdentifier = nil;
            weakService = service;

            const auto connection = [TestXPCConnection new];
            connection.remoteService = service;
            weakConnection = connection;
            const auto connections = [[NSMutableArray alloc] initWithObjects:(NSXPCConnection *)connection, nil];
            const auto clientCommConnections = OCC::Mac::FileProviderXPCUtils::processClientCommunicationConnections(connections, nullptr);

            QCOMPARE(clientCommConnections.size(), 0);
            QVERIFY(connection.invalidated);
        }
        QVERIFY(weakService == nil);
        QVERIFY(weakConnection == nil);
    }

    void duplicateDomainIdentifierInvalidatesDuplicateConnectionAndReleasesService()
    {
        __weak TestClientCommunicationService *weakFirstService = nil;
        __weak TestXPCConnection *weakFirstConnection = nil;
        __weak TestClientCommunicationService *weakDuplicateService = nil;
        __weak TestXPCConnection *weakDuplicateConnection = nil;
        @autoreleasepool {
            const auto firstService = [TestClientCommunicationService new];
            firstService.domainIdentifier = @"domain-id";
            weakFirstService = firstService;

            const auto firstConnection = [TestXPCConnection new];
            firstConnection.remoteService = firstService;
            weakFirstConnection = firstConnection;

            const auto duplicateService = [TestClientCommunicationService new];
            duplicateService.domainIdentifier = @"domain-id";
            weakDuplicateService = duplicateService;

            const auto duplicateConnection = [TestXPCConnection new];
            duplicateConnection.remoteService = duplicateService;
            weakDuplicateConnection = duplicateConnection;
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
        QVERIFY(weakFirstService == nil);
        QVERIFY(weakFirstConnection == nil);
        QVERIFY(weakDuplicateService == nil);
        QVERIFY(weakDuplicateConnection == nil);
    }

    void xpcOwnerReleasesConnectionsAndServicesOnTeardown_data()
    {
        QTest::addColumn<bool>("reconnect");
        QTest::newRow("reconnect") << true;
        QTest::newRow("owner destruction") << false;
    }

    void xpcOwnerReleasesConnectionsAndServicesOnTeardown()
    {
        QFETCH(bool, reconnect);
        installDomainLookupMocks();
        auto owner = std::make_unique<OCC::Mac::FileProviderXPC>();
        __weak TestClientCommunicationService *weakService = nil;
        __weak TestXPCConnection *weakConnection = nil;
        __block auto invalidationCount = 0;
        @autoreleasepool {
            const auto manager = installRegisteredDomain(u"owner-domain"_s);
            manager.synchronousServiceReply = YES;
            const auto remoteService = [TestClientCommunicationService new];
            remoteService.domainIdentifier = @"owner-domain";
            weakService = remoteService;
            const auto connection = [TestXPCConnection new];
            connection.remoteService = remoteService;
            connection.invalidationObserver = ^{ ++invalidationCount; };
            weakConnection = connection;
            const auto service = [TestFileProviderService new];
            service.name = communicationServiceName;
            service.connection = (NSXPCConnection *)connection;
            manager.service = (NSFileProviderService *)service;

            owner->connectToFileProviderDomains();
            QVERIFY(connection.resumed);
            QVERIFY(!connection.invalidated);
            manager.service = nil;
        }
        @autoreleasepool {
            QVERIFY(weakService != nil);
            QVERIFY(weakConnection != nil);
        }

        @autoreleasepool {
            if (reconnect) {
                discoveredDomains = @[];
                discoveredManagers = @{};
                owner->connectToFileProviderDomains();
            } else {
                owner.reset();
            }
        }
        QCOMPARE(invalidationCount, 1);
        QVERIFY(weakService == nil);
        QVERIFY(weakConnection == nil);
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
