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
    
    // Get the FileProviderService singleton from FileProvider
    const auto fileProviderService = FileProvider::instance()->service();
    _clientCommServices = FileProviderXPCUtils::processClientCommunicationConnections(connections, fileProviderService);
}

void FileProviderXPC::authenticateFileProviderDomains()
{
    qCInfo(lcFileProviderXPC) << "Authenticating file provider domains...";

    for (const auto &fileProviderDomainIdentifier : _clientCommServices.keys()) {
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
            const auto accountState = AccountManager::instance()->accountFromFileProviderDomainIdentifier(fileProviderDomainIdentifier);

            if (!accountState || !accountState->account()) {
                qCWarning(lcFileProviderXPC) << "Could not get account for domain" << fileProviderDomainIdentifier << "during reconfigure.";
                return response;
            }

            const auto domain = (NSFileProviderDomain *)(ncDomainManager->domainForAccount(accountState->account().data()));
            const auto manager = [NSFileProviderManager managerForDomain:domain];
            const auto fpServices = FileProviderXPCUtils::getFileProviderServices(@[manager]);
            const auto connections = FileProviderXPCUtils::connectToFileProviderServices(fpServices);
            const auto fileProviderService = FileProvider::instance()->service();
            const auto services = FileProviderXPCUtils::processClientCommunicationConnections(connections, fileProviderService);
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

bool FileProviderXPC::fileProviderDomainHasDirtyUserData(const QString &fileProviderDomainIdentifier) const
{
    qCInfo(lcFileProviderXPC) << "Checking for dirty user data in file provider domain" << fileProviderDomainIdentifier;

    const auto service = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(fileProviderDomainIdentifier);

    if (service == nil) {
        qCWarning(lcFileProviderXPC) << "Could not get service for file provider domain" << fileProviderDomainIdentifier;
        return false;
    }

    __block auto hasDirtyUserData = false;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    [service hasDirtyUserDataWithCompletionHandler:^(BOOL dirty) {
        hasDirtyUserData = dirty;
        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    qCInfo(lcFileProviderXPC) << "File provider domain" << fileProviderDomainIdentifier << (hasDirtyUserData ? "has" : "does not have") << "dirty user data";

    return hasDirtyUserData;
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
