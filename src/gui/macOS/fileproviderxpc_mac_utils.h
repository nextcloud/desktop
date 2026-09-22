/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QHash>
#include <QLoggingCategory>

#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>

#import "../../../shell_integration/MacOSX/NextcloudFileProviderKit/Sources/NextcloudFileProviderXPC/include/NextcloudFileProviderXPC.h"
#import "fileproviderservice.h"

namespace OCC::Mac::FileProviderXPCUtils {

NSArray<NSFileProviderManager *> *getDomainManagers();
NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *getFileProviderServices(NSArray<NSFileProviderManager *> *managers);
NSArray<NSXPCConnection *> *connectToFileProviderServices(NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *fpServices);
void configureFileProviderConnection(NSXPCConnection *connection);
NSObject *getRemoteServiceObject(NSXPCConnection *connection, Protocol *protocol);

/**
 * @brief Get the domain identifier for and from a given client communication service.
 */
NSString *getFileProviderDomainIdentifier(NSObject<ClientCommunicationProtocol> *clientCommService);

/**
 * @brief Holds a client communication service and its XPC connection for one File Provider domain.
 */
struct ClientCommunicationConnection {
    void *clientCommunicationService = nullptr; //!< Remote ClientCommunicationProtocol proxy used to send requests.
    void *xpcConnection = nullptr; //!< Underlying NSXPCConnection used to transport and invalidate those requests.
};

using ClientCommunicationConnections = QHash<QString, ClientCommunicationConnection>;

/**
 * @brief Configures client communication connections and indexes them by domain identifier.
 * @param connections XPC connections obtained from the File Provider services.
 * @param service The app-side service exported through each connection.
 * @return A map containing the retained remote proxy and XPC connection for each domain identifier.
 *
 * The caller owns both retained objects in the returned values and must invalidate and release them
 * when they are no longer needed.
 */
ClientCommunicationConnections processClientCommunicationConnections(NSArray<NSXPCConnection *> *connections, OCC::Mac::FileProviderService *service);
}
