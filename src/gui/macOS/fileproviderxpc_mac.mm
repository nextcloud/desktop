/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "common/utility.h"
#include "fileproviderxpc.h"

#include <QLoggingCategory>
#include <QPointer>
#include <QtConcurrentRun>

#include <utility>

#include "csync/csync_exclude.h"

#include "libsync/configfile.h"

#include "gui/accountmanager.h"
#include "gui/macOS/fileprovider.h"
#include "gui/macOS/fileproviderdomainmanager.h"
#include "gui/macOS/fileproviderutils_mac.h"
#include "gui/macOS/fileproviderxpc_mac_utils.h"

#import <Foundation/Foundation.h>

static_assert(__has_feature(objc_arc), "File Provider XPC requires ARC.");

namespace {
constexpr auto semaphoreWaitTimeout = std::chrono::seconds(1);
constexpr auto semaphoreWaitDelta = std::chrono::nanoseconds(semaphoreWaitTimeout).count();
constexpr auto reachableRetryTimeout = 300; // seconds
}

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFileProviderXPC, "nextcloud.gui.macos.fileprovider.xpc", QtInfoMsg)

namespace
{
using FileProviderXPCUtils::ClientCommunicationConnection;

enum class FileIdDeliveryResult {
    Delivered,
    RetryLater,
    Reconnect,
};

void releaseFileProviderConnection(ClientCommunicationConnection connection)
{
    [(__bridge_transfer NSXPCConnection *)connection.xpcConnection invalidate];
    (void)CFBridgingRelease(connection.clientCommunicationService);
}

struct DiscoveredConnection {
    ClientCommunicationConnection connection;

    ~DiscoveredConnection()
    {
        releaseFileProviderConnection(connection);
    }
};

std::shared_ptr<DiscoveredConnection> discoverFileProviderConnection(const QString &domainIdentifier, id<AppProtocol> delegate)
{
    auto replacement = std::make_shared<DiscoveredConnection>();
    for (NSFileProviderDomain *const domain in FileProviderUtils::fileProviderDomains()) {
        if (QString::fromNSString(domain.identifier) != domainIdentifier) {
            continue;
        }
        NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
        if (!manager) {
            break;
        }
        const auto services = FileProviderXPCUtils::getFileProviderServices(@[ manager ]);
        const auto connections = FileProviderXPCUtils::connectToFileProviderServices(services);
        auto newConnections = FileProviderXPCUtils::processClientCommunicationConnections(connections, delegate);
        replacement->connection = newConnections.take(domainIdentifier);
        for (const auto &unexpected : std::as_const(newConnections)) {
            releaseFileProviderConnection(unexpected);
        }
        break;
    }
    return replacement;
}

FileIdDeliveryResult sendFileIdsToExtension(NSXPCConnection *connection, const FileProviderPushBatch::FileIds &fileIds)
{
    if (!connection) {
        return FileIdDeliveryResult::Reconnect;
    }

    NSMutableArray<NSNumber *> *const nsFileIds = [NSMutableArray arrayWithCapacity:fileIds.size()];
    for (const auto fileId : fileIds) {
        [nsFileIds addObject:@(fileId)];
    }

    id response = nil;
    const auto replied = FileProviderUtils::waitForFileProviderRequest(
        "forwarding file ID changes",
        ^(FileProviderUtils::RequestReplyHandler reply) {
            id<ClientCommunicationProtocol> const service = [connection remoteObjectProxyWithErrorHandler:^(NSError *error) {
                const auto connectionLost =
                    [error.domain isEqualToString:NSCocoaErrorDomain] && (error.code == NSXPCConnectionInterrupted || error.code == NSXPCConnectionInvalid);
                reply(@(static_cast<int>(connectionLost ? FileIdDeliveryResult::Reconnect : FileIdDeliveryResult::RetryLater)), nil);
            }];
            [service processFileIdsChanged:nsFileIds
                         completionHandler:^(BOOL didProcess) {
                             reply(@(static_cast<int>(didProcess ? FileIdDeliveryResult::Delivered : FileIdDeliveryResult::RetryLater)), nil);
                         }];
        },
        &response,
        semaphoreWaitTimeout);
    return replied && response ? static_cast<FileIdDeliveryResult>([response intValue]) : FileIdDeliveryResult::RetryLater;
}

}

FileProviderXPC::FileProviderXPC(QObject *parent)
    : QObject{parent}
{
}

FileProviderXPC::~FileProviderXPC()
{
    disconnectFromFileProviderDomains();
}

void FileProviderXPC::disconnectFromFileProviderDomain(const QString &fileProviderDomainIdentifier)
{
    const auto clientCommConnection = _clientCommConnections.take(fileProviderDomainIdentifier);
    releaseFileProviderConnection(clientCommConnection);
}

void FileProviderXPC::disconnectFromFileProviderDomains()
{
    for (const auto &fileProviderDomainIdentifier : _clientCommConnections.keys()) {
        disconnectFromFileProviderDomain(fileProviderDomainIdentifier);
    }
}

void FileProviderXPC::connectToFileProviderDomains()
{
    qCInfo(lcFileProviderXPC) << "Connecting to file provider domains.";

    disconnectFromFileProviderDomains();

    const auto managers = FileProviderXPCUtils::getDomainManagers();
    const auto fpServices = FileProviderXPCUtils::getFileProviderServices(managers);
    const auto connections = FileProviderXPCUtils::connectToFileProviderServices(fpServices);
    
    // Get the FileProviderService singleton from FileProvider
    const auto fileProviderService = FileProvider::instance()->service();
    const auto delegate = fileProviderService ? (__bridge id<AppProtocol>)fileProviderService->delegate() : nil;
    const auto clientCommConnections = FileProviderXPCUtils::processClientCommunicationConnections(connections, delegate);

    for (const auto &domainIdentifier : clientCommConnections.keys()) {
        const auto clientCommConnection = clientCommConnections.value(domainIdentifier);
        _clientCommConnections.insert(domainIdentifier, clientCommConnection);
    }
}

void FileProviderXPC::authenticateFileProviderDomains()
{
    qCInfo(lcFileProviderXPC) << "Authenticating file provider domains...";

    for (const auto &fileProviderDomainIdentifier : _clientCommConnections.keys()) {
        authenticateFileProviderDomain(fileProviderDomainIdentifier);
    }
}

void FileProviderXPC::authenticateFileProviderDomain(const QString &fileProviderDomainIdentifier) const
{
    const auto accountState = AccountManager::instance()->accountFromFileProviderDomainIdentifier(fileProviderDomainIdentifier);

    if (!accountState) {
        qCWarning(lcFileProviderXPC) << "Account state is null for file provider domain to authenticate" << fileProviderDomainIdentifier;
        return;
    }

    connect(accountState.data(), &AccountState::stateChanged, this, &FileProviderXPC::slotAccountStateChanged, Qt::UniqueConnection);

    const auto account = accountState->account();
    const auto credentials = account->credentials();
    NSString *const user = credentials->user().toNSString();
    NSString *const userId = account->davUser().toNSString();
    NSString *const serverUrl = account->url().toString().toNSString();
    NSString *const password = credentials->password().toNSString();
    NSString *const passwordDescription = password.length > 0 ? @"SOME PASSWORD" : @"EMPTY PASSWORD";
    NSString *const userAgent = QString::fromUtf8(Utility::userAgentString()).toNSString();

    const auto clientCommConnection = _clientCommConnections.value(fileProviderDomainIdentifier);
    const auto clientCommService = (__bridge NSObject<ClientCommunicationProtocol> *)clientCommConnection.clientCommunicationService;

    qCInfo(lcFileProviderXPC) << "Authenticating file provider domain with identifier"
                              << fileProviderDomainIdentifier
                              << "as"
                              << user
                              << "on"
                              << serverUrl
                              << "with"
                              << passwordDescription;

    [clientCommService configureAccountWithUser:user
                                         userId:userId
                                      serverUrl:serverUrl
                                       password:password
                                      userAgent:userAgent];
}

void FileProviderXPC::unauthenticateFileProviderDomain(const QString &fileProviderDomainIdentifier) const
{
    qCInfo(lcFileProviderXPC) << "Unauthenticating file provider domain with identifier" << fileProviderDomainIdentifier;
    const auto clientCommConnection = _clientCommConnections.value(fileProviderDomainIdentifier);
    const auto clientCommService = (__bridge NSObject<ClientCommunicationProtocol> *)clientCommConnection.clientCommunicationService;
    [clientCommService removeAccountConfig];
}

void FileProviderXPC::slotAccountStateChanged(const AccountState::State state) const
{
    const auto slotSender = dynamic_cast<AccountState*>(sender());
    Q_ASSERT(slotSender);
    const auto fileProviderDomainId = slotSender->account()->fileProviderDomainIdentifier();

    switch(state) {
    case AccountState::Disconnected:
    case AccountState::ConfigurationError:
    case AccountState::NetworkError:
    case AccountState::ServiceUnavailable:
    case AccountState::MaintenanceMode:
        // Do nothing, File Provider will by itself figure out connection issue
        break;
    case AccountState::SignedOut:
    case AccountState::AskingCredentials:
    case AccountState::RedirectDetected:
    case AccountState::NeedToSignTermsOfService:
        // Notify File Provider that it should show the not authenticated message
        unauthenticateFileProviderDomain(fileProviderDomainId);
        break;
    case AccountState::Connected:
        // Provide credentials
        authenticateFileProviderDomain(fileProviderDomainId);
        // Push the ignore list alongside credentials — the extension rejects
        // `modifyItem` and friends with `.notAuthenticated` as long as its
        // `ignoredFiles` is still `nil`, so a domain that comes online after
        // startup needs both halves of the configuration, not just the one.
        setIgnoreList();
        break;
    }
}

bool FileProviderXPC::fileProviderDomainReachable(const QString &fileProviderDomainIdentifier, const bool retry, const bool reconfigureOnFail)
{
    const auto lastUnreachableTime = _unreachableFileProviderDomains.value(fileProviderDomainIdentifier);
    if (!retry
        && !reconfigureOnFail 
        && lastUnreachableTime.isValid() 
        && lastUnreachableTime.secsTo(QDateTime::currentDateTime()) < ::reachableRetryTimeout) {
        qCInfo(lcFileProviderXPC) << "File provider extension was unreachable less than a minute ago. Not checking again.";
        return false;
    }

    const auto clientCommConnection = _clientCommConnections.value(fileProviderDomainIdentifier);
    const auto service = (__bridge NSObject<ClientCommunicationProtocol> *)clientCommConnection.clientCommunicationService;

    if (service == nil) {
        qCWarning(lcFileProviderXPC) << "Could not get service for file provider domain" << fileProviderDomainIdentifier;
        return false;
    }

    __block auto response = false;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [service getFileProviderDomainIdentifierWithCompletionHandler:^(NSString *const, NSError *const) {
        response = true;
        dispatch_semaphore_signal(semaphore);
    }];
    dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, semaphoreWaitDelta));

    if (response) {
        _unreachableFileProviderDomains.remove(fileProviderDomainIdentifier);
    } else {
        qCWarning(lcFileProviderXPC) << "Could not reach file provider domain service.";

        if (reconfigureOnFail) {
            (void)reconnectFileProviderDomain(fileProviderDomainIdentifier);
        }

        if (retry) {
            qCWarning(lcFileProviderXPC) << "Could not reach file provider domain" << fileProviderDomainIdentifier << ", retrying.";
            return fileProviderDomainReachable(fileProviderDomainIdentifier, false, false);
        } else {
            _unreachableFileProviderDomains.insert(fileProviderDomainIdentifier, QDateTime::currentDateTime());
        }
    }
    return response;
}

bool FileProviderXPC::reconnectFileProviderDomain(const QString &domainIdentifier)
{
    const auto service = FileProvider::instance()->service();
    const auto delegate = service ? (__bridge id<AppProtocol>)service->delegate() : nil;
    auto replacement = discoverFileProviderConnection(domainIdentifier, delegate);
    if (!replacement->connection.xpcConnection) {
        return false;
    }

    installFileProviderConnection(domainIdentifier, std::exchange(replacement->connection, {}));
    return true;
}

void FileProviderXPC::installFileProviderConnection(const QString &domainIdentifier, ClientCommunicationConnection connection)
{
    disconnectFromFileProviderDomain(domainIdentifier);
    _clientCommConnections.insert(domainIdentifier, connection);
    const auto account = AccountManager::instance()->accountFromFileProviderDomainIdentifier(domainIdentifier);
    if (account && account->isConnected()) {
        authenticateFileProviderDomain(domainIdentifier);
    }
    setIgnoreListForDomain(domainIdentifier);
}

std::optional<bool> FileProviderXPC::fileProviderDomainHasDirtyUserData(const QString &fileProviderDomainIdentifier) const
{
    qCInfo(lcFileProviderXPC) << "Checking for dirty user data in file provider domain" << fileProviderDomainIdentifier;

    const auto clientCommConnection = _clientCommConnections.value(fileProviderDomainIdentifier);
    const auto service = (__bridge NSObject<ClientCommunicationProtocol> *)clientCommConnection.clientCommunicationService;

    if (service == nil) {
        qCWarning(lcFileProviderXPC) << "Could not get service for file provider domain" << fileProviderDomainIdentifier;
        return std::nullopt;
    }

    __block auto hasDirtyUserData = false;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    [service hasDirtyUserDataWithCompletionHandler:^(BOOL dirty) {
        hasDirtyUserData = dirty;
        dispatch_semaphore_signal(semaphore);
    }];

    const auto waitResult = dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, semaphoreWaitDelta));

    if (waitResult != 0) {
        qCWarning(lcFileProviderXPC) << "Timed out while checking for dirty user data in file provider domain" << fileProviderDomainIdentifier;
        return std::nullopt;
    }

    qCInfo(lcFileProviderXPC) << "File provider domain" << fileProviderDomainIdentifier << (hasDirtyUserData ? "has" : "does not have") << "dirty user data";

    return hasDirtyUserData;
}

void FileProviderXPC::deliverFileIds(const QString &domainIdentifier,
                                     const FileProviderPushBatch::Delivery &delivery,
                                     QObject *context,
                                     std::function<void(bool)> completed)
{
    sendFileIds(domainIdentifier, delivery, context, std::move(completed), true);
}

void FileProviderXPC::sendFileIds(const QString &domainIdentifier,
                                  const FileProviderPushBatch::Delivery &delivery,
                                  QObject *context,
                                  std::function<void(bool)> completed,
                                  bool allowReconnect)
{
    const auto account = AccountManager::instance()->accountFromFileProviderDomainIdentifier(domainIdentifier);
    NSXPCConnection *const connection = (__bridge NSXPCConnection *)_clientCommConnections.value(domainIdentifier).xpcConnection;
    const QPointer<FileProviderXPC> self(this);
    QtConcurrent::run([connection, delivery] {
        @autoreleasepool {
            return sendFileIdsToExtension(connection, *delivery);
        }
    })
        .then(context,
              [self, connection, domainIdentifier, delivery, context, completed = std::move(completed), allowReconnect, account](FileIdDeliveryResult result) {
                  if (result != FileIdDeliveryResult::Reconnect || !allowReconnect || !self) {
                      completed(result == FileIdDeliveryResult::Delivered);
                      return;
                  }
                  const auto currentAccount = AccountManager::instance()->accountFromFileProviderDomainIdentifier(domainIdentifier);
                  if (!currentAccount || currentAccount != account || !currentAccount->isConnected()) {
                      completed(false);
                      return;
                  }
                  if (self->_clientCommConnections.value(domainIdentifier).xpcConnection != (__bridge void *)connection) {
                      self->sendFileIds(domainIdentifier, delivery, context, std::move(completed), false);
                      return;
                  }

                  const auto service = FileProvider::instance()->service();
                  const auto delegate = service ? (__bridge id<AppProtocol>)service->delegate() : nil;
                  QtConcurrent::run([domainIdentifier, delegate] {
                      @autoreleasepool {
                          return discoverFileProviderConnection(domainIdentifier, delegate);
                      }
                  })
                      .then(context,
                            [self, connection, domainIdentifier, delivery, context, completed = std::move(completed), account](
                                const std::shared_ptr<DiscoveredConnection> &replacement) {
                                const auto currentAccount = AccountManager::instance()->accountFromFileProviderDomainIdentifier(domainIdentifier);
                                if (!self || currentAccount != account || !currentAccount->isConnected()) {
                                    completed(false);
                                    return;
                                }
                                if (self->_clientCommConnections.value(domainIdentifier).xpcConnection == (__bridge void *)connection) {
                                    if (!replacement->connection.xpcConnection) {
                                        completed(false);
                                        return;
                                    }
                                    self->installFileProviderConnection(domainIdentifier, std::exchange(replacement->connection, {}));
                                }
                                self->sendFileIds(domainIdentifier, delivery, context, std::move(completed), false);
                            });
              });
}

void FileProviderXPC::setIgnoreList() const
{
    setIgnoreListForDomain({});
}

void FileProviderXPC::setIgnoreListForDomain(const QString &domainIdentifier) const
{
    ExcludedFiles ignoreList;
    ConfigFile::setupDefaultExcludeFilePaths(ignoreList);
    ignoreList.reloadExcludeFiles();
    const auto qPatterns = ignoreList.activeExcludePatterns();
    qCInfo(lcFileProviderXPC) << "Updating ignore list with" << qPatterns.size() << "patterns";

    const auto mutableNsPatterns = NSMutableArray.array;

    for (const auto &pattern : qPatterns) {
        [mutableNsPatterns addObject:pattern.toNSString()];
    }

    NSArray<NSString *> *const nsPatterns = [mutableNsPatterns copy];

    for (const auto &fileProviderDomainIdentifier : _clientCommConnections.keys()) {
        if (!domainIdentifier.isEmpty() && fileProviderDomainIdentifier != domainIdentifier) {
            continue;
        }
        qCInfo(lcFileProviderXPC) << "Updating ignore list of file provider domain" << fileProviderDomainIdentifier;
        const auto clientCommConnection = _clientCommConnections.value(fileProviderDomainIdentifier);
        const auto service = (__bridge NSObject<ClientCommunicationProtocol> *)clientCommConnection.clientCommunicationService;
        [service setIgnoreList:nsPatterns];
    }
}

} // namespace OCC::Mac
