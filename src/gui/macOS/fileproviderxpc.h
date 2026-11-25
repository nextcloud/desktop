/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QTimer>

#include <optional>

#include "accountstate.h"
#include "fileproviderxpcconnection.h"

namespace OCC::Mac {

class FileProvider;

/*
 * Establishes communication between the app and the file provider extension processes.
 * This is done via services exposed by the file provider extension through XPC.
 * Note that this is for desktop client app to file provider extension communication only, not the other way around.
 * This is because the extension does not have a way to communicate with the client through XPC.
 */
class FileProviderXPC : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderXPC(QObject *parent = nullptr);
    ~FileProviderXPC() override;

    [[nodiscard]] bool fileProviderDomainReachable(const QString &fileProviderDomainIdentifier);
    [[nodiscard]] bool processFileIdsChanged(const QString &fileProviderDomainIdentifier, const QList<qint64> &fileIds) const;

public Q_SLOTS:
    void connectToFileProviderDomains();
    void authenticateFileProviderDomains();
    void authenticateFileProviderDomain(const QString &fileProviderDomainIdentifier) const;
    void unauthenticateFileProviderDomain(const QString &fileProviderDomainIdentifier) const;

    void setIgnoreList() const;

private Q_SLOTS:
    void slotAccountStateChanged(AccountState::State state) const;
    //! Reconnect and authenticate every domain whose extension stopped answering, as after a restart of fileproviderd.
    void checkFileProviderDomains();

private:
    friend class FileProvider;

    [[nodiscard]] std::optional<bool> fileProviderDomainHasDirtyUserData(const QString &fileProviderDomainIdentifier) const;

    void disconnectFromFileProviderDomains();
    void disconnectFromFileProviderDomain(const QString &fileProviderDomainIdentifier);
    //! Replace the connection to a domain's extension and send it the account and the ignore list again.
    //! Returns whether the extension answered on the new connection.
    bool reconnectFileProviderDomain(const QString &fileProviderDomainIdentifier);

    //! keys are File Provider domain identifiers
    FileProviderXPCUtils::ClientCommunicationConnections _clientCommConnections;
    //! keys are File Provider domain identifiers
    QHash<QString, QDateTime> _unreachableFileProviderDomains;

    //! XPC does not tell the app when fileproviderd replaces the extension process, so the app asks.
    QTimer _reachabilityTimer;
};

} // namespace OCC::Mac
