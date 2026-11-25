/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "common/utility.h"
#include "fileproviderxpc.h"

#include <QLoggingCategory>

#include "csync/csync_exclude.h"

#include "libsync/configfile.h"

#include "gui/accountmanager.h"
#include "gui/macOS/fileprovider.h"
#include "gui/macOS/fileproviderdomainmanager.h"
#include "gui/macOS/fileproviderxpc_mac_utils.h"

#import <Foundation/Foundation.h>

static_assert(__has_feature(objc_arc), "File Provider XPC requires ARC.");

namespace {
    constexpr int64_t semaphoreWaitDelta = 1000000000; // 1 seconds
    constexpr auto reachableRetryTimeout = 300; // seconds
    constexpr auto reachabilityCheckInterval = std::chrono::seconds(15);

    //! The states in which the app takes the account back from the extension.
    bool accountWithdrawnFromExtension(const OCC::AccountState::State state)
    {
        return state == OCC::AccountState::SignedOut || state == OCC::AccountState::AskingCredentials || state == OCC::AccountState::RedirectDetected
            || state == OCC::AccountState::NeedToSignTermsOfService;
    }
}

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFileProviderXPC, "nextcloud.gui.macos.fileprovider.xpc", QtInfoMsg)

FileProviderXPC::FileProviderXPC(QObject *parent)
    : QObject{parent}
{
    _reachabilityTimer.setInterval(reachabilityCheckInterval);
    connect(&_reachabilityTimer, &QTimer::timeout, this, &FileProviderXPC::checkFileProviderDomains);
    if (FileProvider::available()) {
        _reachabilityTimer.start();
    }
}

FileProviderXPC::~FileProviderXPC()
{
    disconnectFromFileProviderDomains();
}

void FileProviderXPC::disconnectFromFileProviderDomain(const QString &fileProviderDomainIdentifier)
{
    const auto clientCommConnection = _clientCommConnections.take(fileProviderDomainIdentifier);
    [(__bridge_transfer NSXPCConnection *)clientCommConnection.xpcConnection invalidate];
    (void)CFBridgingRelease(clientCommConnection.clientCommunicationService);
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
    const auto clientCommConnections = FileProviderXPCUtils::processClientCommunicationConnections(connections, fileProviderService);

    for (const auto &domainIdentifier : clientCommConnections.keys()) {
        const auto clientCommConnection = clientCommConnections.value(domainIdentifier);
        _clientCommConnections.insert(domainIdentifier, clientCommConnection);
    }
}

void FileProviderXPC::checkFileProviderDomains()
{
    for (const auto &accountState : AccountManager::instance()->accounts()) {
        const auto fileProviderDomainIdentifier = accountState->account()->fileProviderDomainIdentifier();
        if (!fileProviderDomainIdentifier.isEmpty()) {
            (void)fileProviderDomainReachable(fileProviderDomainIdentifier);
        }
    }
}

void FileProviderXPC::reconnectFileProviderDomain(const QString &fileProviderDomainIdentifier)
{
    if (!FileProvider::available()) {
        return;
    }

    const auto accountState = AccountManager::instance()->accountFromFileProviderDomainIdentifier(fileProviderDomainIdentifier);

    if (!accountState || !accountState->account()) {
        qCWarning(lcFileProviderXPC) << "Could not get account for domain" << fileProviderDomainIdentifier << "during reconnect.";
        return;
    }

    const auto domain = (NSFileProviderDomain *)(FileProvider::instance()->domainManager()->domainForAccount(accountState->account().data()));

    if (domain == nil) {
        qCWarning(lcFileProviderXPC) << "No file provider domain" << fileProviderDomainIdentifier << "to reconnect.";
        return;
    }

    const auto manager = [NSFileProviderManager managerForDomain:domain];

    if (manager == nil) {
        qCWarning(lcFileProviderXPC) << "No file provider manager for domain" << fileProviderDomainIdentifier << "during reconnect.";
        return;
    }

    const auto fpServices = FileProviderXPCUtils::getFileProviderServices(@[ manager ]);
    const auto connections = FileProviderXPCUtils::connectToFileProviderServices(fpServices);
    const auto clientCommConnections = FileProviderXPCUtils::processClientCommunicationConnections(connections, FileProvider::instance()->service());

    for (const auto &domainIdentifier : clientCommConnections.keys()) {
        disconnectFromFileProviderDomain(domainIdentifier);
        _clientCommConnections.insert(domainIdentifier, clientCommConnections.value(domainIdentifier));
        authenticateFileProviderDomain(domainIdentifier);
    }

    if (!clientCommConnections.isEmpty()) {
        setIgnoreList();
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

    if (accountWithdrawnFromExtension(accountState->state())) {
        // The account reaches the extension through slotAccountStateChanged once it is connected again.
        qCInfo(lcFileProviderXPC) << "Not authenticating file provider domain" << fileProviderDomainIdentifier << "while its account is signed out.";
        return;
    }

    const auto account = accountState->account();
    const auto credentials = account->credentials();
    NSString *const user = credentials->user().toNSString();
    NSString *const userId = account->davUser().toNSString();
    NSString *const serverUrl = FileProviderXPCUtils::urlForFileProviderXPC(account->url()).toNSString();
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

    if (accountWithdrawnFromExtension(state)) {
        // Notify File Provider that it should show the not authenticated message
        unauthenticateFileProviderDomain(fileProviderDomainId);
    } else if (state == AccountState::Connected) {
        // Provide credentials
        authenticateFileProviderDomain(fileProviderDomainId);
        // Push the ignore list alongside credentials — the extension rejects
        // `modifyItem` and friends with `.notAuthenticated` as long as its
        // `ignoredFiles` is still `nil`, so a domain that comes online after
        // startup needs both halves of the configuration, not just the one.
        setIgnoreList();
    }
    // In the other states File Provider will by itself figure out the connection issue.
}

bool FileProviderXPC::fileProviderDomainReachable(const QString &fileProviderDomainIdentifier, const bool retry, const bool reconfigureOnFail)
{
    const auto lastUnreachableTime = _unreachableFileProviderDomains.value(fileProviderDomainIdentifier);
    if (lastUnreachableTime.isValid() && lastUnreachableTime.secsTo(QDateTime::currentDateTime()) < ::reachableRetryTimeout) {
        qCDebug(lcFileProviderXPC) << "File provider extension was unreachable less than five minutes ago. Not checking again.";
        return false;
    }

    const auto clientCommConnection = _clientCommConnections.value(fileProviderDomainIdentifier);
    const auto service = (__bridge NSObject<ClientCommunicationProtocol> *)clientCommConnection.clientCommunicationService;

    // A domain the app never got a connection to counts as unreachable, so it is connected like one which stopped answering.
    __block auto response = false;
    if (service == nil) {
        qCWarning(lcFileProviderXPC) << "Could not get service for file provider domain" << fileProviderDomainIdentifier;
    } else {
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        [service getFileProviderDomainIdentifierWithCompletionHandler:^(NSString *const, NSError *const) {
            response = true;
            dispatch_semaphore_signal(semaphore);
        }];
        dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, semaphoreWaitDelta));
    }

    if (response) {
        _unreachableFileProviderDomains.remove(fileProviderDomainIdentifier);
    } else {
        qCWarning(lcFileProviderXPC) << "Could not reach file provider domain service.";

        if (reconfigureOnFail) {
            qCWarning(lcFileProviderXPC) << "Could not reach service of file provider domain"
                                         << fileProviderDomainIdentifier
                                         << "going to attempt reconfiguring interface";
            reconnectFileProviderDomain(fileProviderDomainIdentifier);
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

bool FileProviderXPC::processFileIdsChanged(const QString &fileProviderDomainIdentifier, const QList<qint64> &fileIds) const
{
    const auto clientCommConnection = _clientCommConnections.value(fileProviderDomainIdentifier);
    const auto service = (__bridge NSObject<ClientCommunicationProtocol> *)clientCommConnection.clientCommunicationService;

    if (service == nil) {
        qCWarning(lcFileProviderXPC) << "Could not get service for file provider domain" << fileProviderDomainIdentifier;
        return false;
    }

    NSMutableArray<NSNumber *> *const nsFileIds = [NSMutableArray arrayWithCapacity:fileIds.size()];

    for (const auto fileId : fileIds) {
        [nsFileIds addObject:@(fileId)];
    }

    __block auto processed = false;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [service processFileIdsChanged:nsFileIds completionHandler:^(BOOL didProcess) {
        processed = didProcess;
        dispatch_semaphore_signal(semaphore);
    }];

    const auto waitResult = dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, semaphoreWaitDelta));

    if (waitResult != 0) {
        qCWarning(lcFileProviderXPC) << "Timed out while forwarding file ID changes to file provider domain"
                                     << fileProviderDomainIdentifier;
        return false;
    }

    return processed;
}

void FileProviderXPC::setIgnoreList() const
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
        qCInfo(lcFileProviderXPC) << "Updating ignore list of file provider domain" << fileProviderDomainIdentifier;
        const auto clientCommConnection = _clientCommConnections.value(fileProviderDomainIdentifier);
        const auto service = (__bridge NSObject<ClientCommunicationProtocol> *)clientCommConnection.clientCommunicationService;
        [service setIgnoreList:nsPatterns];
    }
}

} // namespace OCC::Mac
