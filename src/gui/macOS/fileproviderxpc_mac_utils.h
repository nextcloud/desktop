/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QLoggingCategory>

#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>

#import "ClientCommunicationProtocol.h"

namespace OCC::Mac::FileProviderXPCUtils {

NSArray<NSFileProviderManager *> *getDomainManagers();
NSArray<NSURL *> *getDomainUrlsForManagers(NSArray<NSFileProviderManager *> *managers);
NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *getFileProviderServices(NSArray<NSFileProviderManager *> *managers);
NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *getFileProviderServicesAtUrls(NSArray<NSURL *> *urls);
NSArray<NSXPCConnection *> *connectToFileProviderServices(NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *fpServices);
void configureFileProviderConnection(NSXPCConnection *connection);
NSObject *getRemoteServiceObject(NSXPCConnection *connection, Protocol *protocol);

/**
 * @brief Get the domain identifier for and from a given client communication service.
 */
NSString *getFileProviderDomainIdentifier(NSObject<ClientCommunicationProtocol> *clientCommService);

QHash<QString, void*> processClientCommunicationConnections(NSArray<NSXPCConnection *> *connections);

}
