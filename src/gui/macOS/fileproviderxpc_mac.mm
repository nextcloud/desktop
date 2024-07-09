/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "fileproviderxpc.h"

#include <QLoggingCategory>

#include "gui/accountmanager.h"
#include "gui/macOS/fileproviderdomainmanager.h"
#include "gui/macOS/fileproviderxpc_mac_utils.h"

namespace {
    constexpr int64_t semaphoreWaitDelta = 3000000000; // 3 seconds
}

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFileProviderXPC, "nextcloud.gui.macos.fileprovider.xpc", QtInfoMsg)

FileProviderXPC::FileProviderXPC(QObject *parent)
    : QObject{parent}
{
}

void FileProviderXPC::connectToExtensions()
{
    qCInfo(lcFileProviderXPC) << "Starting file provider XPC";
    const auto managers = FileProviderXPCUtils::getDomainManagers();
    const auto fpServices = FileProviderXPCUtils::getFileProviderServices(managers);
    const auto connections = FileProviderXPCUtils::connectToFileProviderServices(fpServices);
    _clientCommServices = FileProviderXPCUtils::processClientCommunicationConnections(connections);
}

void FileProviderXPC::configureExtensions()
{
    for (const auto &extensionNcAccount : _clientCommServices.keys()) {
        qCInfo(lcFileProviderXPC) << "Sending message to client communication service";
        authenticateExtension(extensionNcAccount);
    }
}

void FileProviderXPC::authenticateExtension(const QString &extensionAccountId) const
{
    const auto accountState = FileProviderDomainManager::accountStateFromFileProviderDomainIdentifier(extensionAccountId);
    if (!accountState) {
        qCWarning(lcFileProviderXPC) << "Account state is null for received account"
                                     << extensionAccountId;
        return;
    }

    connect(accountState.data(), &AccountState::stateChanged, this, &FileProviderXPC::slotAccountStateChanged, Qt::UniqueConnection);

    const auto account = accountState->account();
    const auto credentials = account->credentials();
    NSString *const user = credentials->user().toNSString();
    NSString *const serverUrl = account->url().toString().toNSString();
    NSString *const password = credentials->password().toNSString();

    const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(extensionAccountId);
    [clientCommService configureAccountWithUser:user
                                      serverUrl:serverUrl
                                       password:password];
}

void FileProviderXPC::unauthenticateExtension(const QString &extensionAccountId) const
{
    qCInfo(lcFileProviderXPC) << "Unauthenticating extension" << extensionAccountId;
    const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(extensionAccountId);
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
        // Notify File Provider that it should show the not authenticated message
        unauthenticateExtension(extensionAccountId);
        break;
    case AccountState::Connected:
        // Provide credentials
        authenticateExtension(extensionAccountId);
        break;
    }
}
void FileProviderXPC::createDebugArchiveForExtension(const QString &extensionAccountId, const QString &filename) const
{
    qCInfo(lcFileProviderXPC) << "Creating debug archive for extension" << extensionAccountId << "at" << filename;
    // You need to fetch the contents from the extension and then create the archive from the client side.
    // The extension is not allowed to ask for permission to write into the file system as it is not a user facing process.
    const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(extensionAccountId);
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
}

std::optional<std::pair<bool, bool>> FileProviderXPC::fastEnumerationStateForExtension(const QString &extensionAccountId) const
{
    qCInfo(lcFileProviderXPC) << "Checking if fast enumeration is enabled for extension" << extensionAccountId;
    const auto service = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(extensionAccountId);
    if (service == nil) {
        qCWarning(lcFileProviderXPC) << "Could not get service for extension" << extensionAccountId;
        return std::nullopt;
    }

    __block BOOL receivedFastEnumerationEnabled; // What is the value of the setting being used by the extension?
    __block BOOL receivedFastEnumerationSet; // Has the setting been set by the user?
    __block BOOL receivedResponse = NO;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [service getFastEnumerationStateWithCompletionHandler:^(BOOL enabled, BOOL set) {
        receivedFastEnumerationEnabled = enabled;
        receivedFastEnumerationSet = set;
        receivedResponse = YES;
        dispatch_semaphore_signal(semaphore);
    }];
    dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, semaphoreWaitDelta));
    if (!receivedResponse) {
        qCWarning(lcFileProviderXPC) << "Did not receive response for fast enumeration state";
        return std::nullopt;
    }
    return std::optional<std::pair<bool, bool>>{{receivedFastEnumerationEnabled, receivedFastEnumerationSet}};
}

void FileProviderXPC::setFastEnumerationEnabledForExtension(const QString &extensionAccountId, bool enabled) const
{
    qCInfo(lcFileProviderXPC) << "Setting fast enumeration for extension" << extensionAccountId << "to" << enabled;
    const auto service = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(extensionAccountId);
    [service setFastEnumerationEnabled:enabled];
}

} // namespace OCC::Mac
