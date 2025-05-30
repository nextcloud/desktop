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
 * Establishes communication between the app and the file provider extension.
 * This is done via File Provider's XPC services API.
 * Note that this is for client->extension communication, not the other way around.
 * This is because the extension does not have a way to communicate with the client through the File Provider XPC API
 */
class FileProviderXPC : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderXPC(QObject *parent = nullptr);

    [[nodiscard]] bool fileProviderExtReachable(const QString &extensionAccountId, bool retry = true, bool reconfigureOnFail = true);

    // Returns enabled and set state of fast enumeration for the given extension
    [[nodiscard]] std::optional<std::pair<bool, bool>> fastEnumerationStateForExtension(const QString &extensionAccountId) const;

    [[nodiscard]] std::optional<std::pair<bool, bool>> trashDeletionEnabledStateForExtension(const QString &extensionAccountId) const;

public slots:
    void connectToExtensions();
    void configureExtensions();
    void authenticateExtension(const QString &extensionAccountId) const;
    void unauthenticateExtension(const QString &extensionAccountId) const;
    void createDebugArchiveForExtension(const QString &extensionAccountId, const QString &filename);

    void setFastEnumerationEnabledForExtension(const QString &extensionAccountId, bool enabled) const;
    void setTrashDeletionEnabledForExtension(const QString &extensionAccountId, bool enabled) const;

private slots:
    void slotAccountStateChanged(AccountState::State state) const;

private:
    QHash<QString, void*> _clientCommServices;
    QHash<QString, QDateTime> _unreachableAccountExtensions;
};

} // namespace OCC::Mac
