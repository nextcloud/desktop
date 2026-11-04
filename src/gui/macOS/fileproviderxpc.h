/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>

#include <functional>
#include <optional>

#include "accountstate.h"
#include "fileproviderpushbatch.h"
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

    [[nodiscard]] bool fileProviderDomainReachable(const QString &fileProviderDomainIdentifier, bool retry = true, bool reconfigureOnFail = true);
    [[nodiscard]] bool reconnectFileProviderDomain(const QString &fileProviderDomainIdentifier);
    void
    deliverFileIds(const QString &domainIdentifier, const FileProviderPushBatch::Delivery &delivery, QObject *context, std::function<void(bool)> completed);

public Q_SLOTS:
    void connectToFileProviderDomains();
    void authenticateFileProviderDomains();
    void authenticateFileProviderDomain(const QString &fileProviderDomainIdentifier) const;
    void unauthenticateFileProviderDomain(const QString &fileProviderDomainIdentifier) const;

    void setIgnoreList() const;

private Q_SLOTS:
    void slotAccountStateChanged(AccountState::State state) const;

private:
    friend class FileProvider;

    [[nodiscard]] std::optional<bool> fileProviderDomainHasDirtyUserData(const QString &fileProviderDomainIdentifier) const;

    void installFileProviderConnection(const QString &domainIdentifier, FileProviderXPCUtils::ClientCommunicationConnection connection);
    void sendFileIds(const QString &domainIdentifier,
                     const FileProviderPushBatch::Delivery &delivery,
                     QObject *context,
                     std::function<void(bool)> completed,
                     bool allowReconnect);
    void setIgnoreListForDomain(const QString &domainIdentifier) const;
    void disconnectFromFileProviderDomains();
    void disconnectFromFileProviderDomain(const QString &fileProviderDomainIdentifier);

    //! keys are File Provider domain identifiers
    FileProviderXPCUtils::ClientCommunicationConnections _clientCommConnections;
    //! keys are File Provider domain identifiers
    QHash<QString, QDateTime> _unreachableFileProviderDomains;
};

} // namespace OCC::Mac
