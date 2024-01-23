/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
NSString *getExtensionAccountId(NSObject<ClientCommunicationProtocol> *clientCommService);
QHash<QString, void*> processClientCommunicationConnections(NSArray *connections);

}