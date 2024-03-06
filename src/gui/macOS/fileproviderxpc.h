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

#include <QObject>
#include <QHash>

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

    // Returns enabled and set state of fast enumeration for the given extension
    [[nodiscard]] std::optional<std::pair<bool, bool>> fastEnumerationStateForExtension(const QString &extensionAccountId) const;

public slots:
    void connectToExtensions();
    void configureExtensions();
    void authenticateExtension(const QString &extensionAccountId) const;
    void unauthenticateExtension(const QString &extensionAccountId) const;
    void createDebugArchiveForExtension(const QString &extensionAccountId, const QString &filename) const;

    void setFastEnumerationEnabledForExtension(const QString &extensionAccountId, bool enabled) const;

private slots:
    void slotAccountStateChanged(AccountState::State state) const;

private:
    QHash<QString, void*> _clientCommServices;
};

} // namespace OCC::Mac
