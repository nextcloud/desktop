/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QLocalServer>

#include "libsync/accountfwd.h"
#include "libsync/syncresult.h"

namespace OCC {

namespace Mac {

class FileProviderSocketController;
using FileProviderSocketControllerPtr = QPointer<FileProviderSocketController>;

QString fileProviderSocketPath();

/*
 * Establishes communication between the app and the file provider extension.
 * This is done via a local socket server.
 * Note that this should be used for extension->client communication.
 *
 * We can communicate bidirectionally, but the File Provider XPC API is a better interface for this as we cannot account
 * for the lifetime of a file provider extension when using sockets, and cannot control this on the client side.
 * Use FileProviderXPC for client->extension communication when possible.
 *
 * This socket system is critical for the file provider extensions to be able to request authentication details.
 *
 * TODO: This should rewritten to use XPC instead of sockets
 */
class FileProviderSocketServer : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderSocketServer(QObject *parent = nullptr);

    [[nodiscard]] SyncResult::Status latestReceivedSyncStatusForAccount(const AccountPtr &account) const;

signals:
    void syncStateChanged(const AccountPtr &account, SyncResult::Status state) const;

private slots:
    void startListening();
    void slotNewConnection();
    void slotSocketDestroyed(const QLocalSocket * const socket);
    void slotSyncStateChanged(const AccountPtr &account, SyncResult::Status state);

private:
    QString _socketPath;
    QLocalServer _socketServer;
    QHash<const QLocalSocket*, FileProviderSocketControllerPtr> _socketControllers;
    QHash<QString, SyncResult::Status> _latestReceivedSyncStatus;
};

} // namespace Mac

} // namespace OCC
