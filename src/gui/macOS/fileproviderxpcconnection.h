/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QHash>
#include <QString>

namespace OCC::Mac::FileProviderXPCUtils
{

/**
 * @brief Holds a client communication service and its XPC connection for one File Provider domain.
 */
struct ClientCommunicationConnection {
    void *clientCommunicationService = nullptr; //!< Remote ClientCommunicationProtocol proxy used to send requests.
    void *xpcConnection = nullptr; //!< Underlying NSXPCConnection used to transport and invalidate those requests.
};

using ClientCommunicationConnections = QHash<QString, ClientCommunicationConnection>;

} // namespace OCC::Mac::FileProviderXPCUtils
