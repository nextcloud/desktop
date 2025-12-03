/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QLoggingCategory>

#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>

#import "ClientCommunicationProtocol.h"
#import "MainAppServiceProtocol.h"
#import "MainAppService.h"

namespace OCC::Mac::FileProviderXPCUtils {

/**
    @brief Get file provider domain managers for all file provider domains managed by this application.
 */
NSArray<NSFileProviderManager *> *getDomainManagers();

/**
    @brief Return the user-visible URLs for the root containers of the domains belonging to the given managers.
 */
NSArray<NSURL *> *getDomainUrlsForManagers(NSArray<NSFileProviderManager *> *managers);

NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *getFileProviderServices(NSArray<NSFileProviderManager *> *managers);
NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *getFileProviderServicesAtUrls(NSArray<NSURL *> *urls);
NSArray<NSXPCConnection *> *connectToFileProviderServices(NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *fpServices);
void configureFileProviderConnection(NSXPCConnection *connection);
NSObject *getRemoteServiceObject(NSXPCConnection *connection, Protocol *protocol);

QHash<QString, void*> processClientCommunicationConnections(NSArray<NSXPCConnection *> *connections);

}
