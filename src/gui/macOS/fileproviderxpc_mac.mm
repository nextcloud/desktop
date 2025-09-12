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

namespace {
    constexpr int64_t semaphoreWaitDelta = 1000000000; // 1 seconds
    constexpr auto reachableRetryTimeout = 300; // seconds
}

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFileProviderXPC, "nextcloud.gui.macos.fileprovider.xpc", QtInfoMsg)

FileProviderXPC::FileProviderXPC(QObject *parent)
    : QObject{parent}
{
}

void FileProviderXPC::connectToFileProviderDomains()
{
    qCInfo(lcFileProviderXPC) << "Connecting to file provider domains.";

    const auto managers = FileProviderXPCUtils::getDomainManagers();
    const auto fpServices = FileProviderXPCUtils::getFileProviderServices(managers);
    const auto connections = FileProviderXPCUtils::connectToFileProviderServices(fpServices);
    _clientCommServices = FileProviderXPCUtils::processClientCommunicationConnections(connections);
}

void FileProviderXPC::authenticateFileProviderDomains()
{
    for (const auto &fileProviderDomainIdentifier : _clientCommServices.keys()) {
        qCInfo(lcFileProviderXPC) << "Authenticating file provider domains.";
        authenticateFileProviderDomain(fileProviderDomainIdentifier);
    }
}

void FileProviderXPC::authenticateFileProviderDomain(const QString &fileProviderDomainIdentifier) const
{
    const auto accountState = FileProviderDomainManager::accountStateFromFileProviderDomainIdentifier(fileProviderDomainIdentifier);

    if (!accountState) {
        qCWarning(lcFileProviderXPC) << "Account state is null for file provider domain to authenticate"
                                     << fileProviderDomainIdentifier;
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

    const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(fileProviderDomainIdentifier);

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
    const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(fileProviderDomainIdentifier);
    [clientCommService removeAccountConfig];
}

void FileProviderXPC::slotAccountStateChanged(const AccountState::State state) const
{
    const auto slotSender = dynamic_cast<AccountState*>(sender());
    Q_ASSERT(slotSender);
    const auto extensionAccountId = slotSender->account()->userIdAtHostWithPort();

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
        unauthenticateFileProviderDomain(extensionAccountId);
        break;
    case AccountState::Connected:
        // Provide credentials
        authenticateFileProviderDomain(extensionAccountId);
        break;
    }
}
void FileProviderXPC::createDebugArchiveForFileProviderDomain(const QString &fileProviderDomainIdentifier, const QString &filename)
{
    qCInfo(lcFileProviderXPC) << "Creating debug archive for extension" << fileProviderDomainIdentifier << "at" << filename;

    if (!fileProviderDomainReachable(fileProviderDomainIdentifier)) {
        qCWarning(lcFileProviderXPC) << "Extension is not reachable. Cannot create debug archive";
        return;
    }

    // You need to fetch the contents from the extension and then create the archive from the client side.
    // The extension is not allowed to ask for permission to write into the file system as it is not a user facing process.
    const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(fileProviderDomainIdentifier);
    const auto group = dispatch_group_create();
    __block NSString *rcvdDebugLogString;
    dispatch_group_enter(group);
    [clientCommService createDebugLogStringWithCompletionHandler:^(NSString *const debugLogString, NSError *const error) {
        if (error != nil) {
            qCWarning(lcFileProviderXPC) << "Error getting debug log string" << error.localizedDescription;
            dispatch_group_leave(group);
            return;
        } else if (debugLogString == nil) {
            qCWarning(lcFileProviderXPC) << "Debug log string is nil";
            dispatch_group_leave(group);
            return;
        }
        rcvdDebugLogString = [NSString stringWithString:debugLogString];
        [rcvdDebugLogString retain];
        dispatch_group_leave(group);
    }];
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    QFile debugLogFile(filename);
    if (debugLogFile.open(QIODevice::WriteOnly)) {
        debugLogFile.write(rcvdDebugLogString.UTF8String);
        debugLogFile.close();
        qCInfo(lcFileProviderXPC) << "Debug log file written to" << filename;
    } else {
        qCWarning(lcFileProviderXPC) << "Could not open debug log file" << filename;
    }

    [rcvdDebugLogString release];
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

    const auto service = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(fileProviderDomainIdentifier);
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
            qCWarning(lcFileProviderXPC) << "Could not reach service of file provider domain"
                                         << fileProviderDomainIdentifier
                                         << "going to attempt reconfiguring interface";
            const auto ncDomainManager = FileProvider::instance()->domainManager();
            const auto accountState = ncDomainManager->accountStateFromFileProviderDomainIdentifier(fileProviderDomainIdentifier);
            const auto domain = (NSFileProviderDomain *)(ncDomainManager->domainForAccount(accountState.get()));
            const auto manager = [NSFileProviderManager managerForDomain:domain];
            const auto fpServices = FileProviderXPCUtils::getFileProviderServices(@[manager]);
            const auto connections = FileProviderXPCUtils::connectToFileProviderServices(fpServices);
            const auto services = FileProviderXPCUtils::processClientCommunicationConnections(connections);
            _clientCommServices.insert(services);
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

std::optional<std::pair<bool, bool>> FileProviderXPC::trashDeletionEnabledStateForFileProviderDomain(const QString &fileProviderDomainIdentifier) const
{
    qCInfo(lcFileProviderXPC) << "Checking if fast enumeration is enabled for file provider domain" << fileProviderDomainIdentifier;
    const auto service = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(fileProviderDomainIdentifier);

    if (service == nil) {
        qCWarning(lcFileProviderXPC) << "Could not get service for file provider domain" << fileProviderDomainIdentifier;
        return std::nullopt;
    }

    __block BOOL receivedTrashDeletionEnabled = YES; // What is the value of the setting being used by the extension?
    __block BOOL receivedTrashDeletionEnabledSet = NO; // Has the setting been set by the user?
    __block BOOL receivedResponse = NO;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [service getTrashDeletionEnabledStateWithCompletionHandler:^(BOOL enabled, BOOL set) {
        receivedTrashDeletionEnabled = enabled;
        receivedTrashDeletionEnabledSet = set;
        receivedResponse = YES;
        dispatch_semaphore_signal(semaphore);
    }];
    dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, semaphoreWaitDelta));
    if (!receivedResponse) {
        qCWarning(lcFileProviderXPC) << "Did not receive response for fast enumeration state";
        return std::nullopt;
    }
    return std::optional<std::pair<bool, bool>>{{receivedTrashDeletionEnabled, receivedTrashDeletionEnabledSet}};
}

void FileProviderXPC::setTrashDeletionEnabledForFileProviderDomain(const QString &fileProviderDomainIdentifier, bool enabled) const
{
    qCInfo(lcFileProviderXPC) << "Setting trash deletion enabled for file provider domain" << fileProviderDomainIdentifier << "to" << enabled;
    const auto service = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(fileProviderDomainIdentifier);
    [service setTrashDeletionEnabled:enabled];
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

    for (const auto &fileProviderDomainIdentifier : _clientCommServices.keys()) {
        qCInfo(lcFileProviderXPC) << "Updating ignore list of file provider domain" << fileProviderDomainIdentifier;
        const auto service = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(fileProviderDomainIdentifier);
        [service setIgnoreList:nsPatterns];
    }
}

} // namespace OCC::Mac
