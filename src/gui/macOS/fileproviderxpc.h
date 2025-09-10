/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QObject>
#include <QHash>
#include <QDateTime>

#include "accountstate.h"

#pragma once

namespace OCC::Mac {

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

    [[nodiscard]] bool fileProviderDomainReachable(const QString &fileProviderDomainIdentifier, bool retry = true, bool reconfigureOnFail = true);

    [[nodiscard]] std::optional<std::pair<bool, bool>> trashDeletionEnabledStateForFileProviderDomain(const QString &fileProviderDomainIdentifier) const;

public slots:
    void connectToFileProviderDomains();
    void authenticateFileProviderDomains();
    void authenticateFileProviderDomain(const QString &fileProviderDomainIdentifier) const;
    void unauthenticateFileProviderDomain(const QString &fileProviderDomainIdentifier) const;
    void createDebugArchiveForFileProviderDomain(const QString &fileProviderDomainIdentifier, const QString &filename);

    void setIgnoreList() const;
    void setTrashDeletionEnabledForFileProviderDomain(const QString &fileProviderDomainIdentifier, bool enabled) const;

private slots:
    void slotAccountStateChanged(AccountState::State state) const;

private:
    QHash<QString, void*> _clientCommServices;
    QHash<QString, QDateTime> _unreachableFileProviderDomains;
};

} // namespace OCC::Mac
