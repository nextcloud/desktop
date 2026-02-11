/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QHash>

namespace OCC::Mac {

class FinderSyncService;

/**
 * @brief Establishes XPC communication between the app and the FinderSync extension.
 *
 * This class creates an NSXPCListener that the FinderSync extension can connect to,
 * replacing the previous UNIX socket-based communication. The FinderSync extension
 * exports FinderSyncProtocol, and the app exports FinderSyncAppProtocol.
 */
class FinderSyncXPC : public QObject
{
    Q_OBJECT

public:
    explicit FinderSyncXPC(QObject *parent = nullptr);
    ~FinderSyncXPC() override;

    /**
     * @brief Start the XPC listener for FinderSync connections.
     * @param service The FinderSyncService that implements the app-side protocol.
     */
    void startListener(Mac::FinderSyncService *service = nullptr);

    /**
     * @brief Check if we have any active FinderSync extension connections.
     * @return true if at least one extension is connected.
     */
    [[nodiscard]] bool hasActiveConnections() const;

public slots:
    /**
     * @brief Register a sync folder path with all connected FinderSync extensions.
     * @param path The absolute path to register.
     */
    void registerPath(const QString &path);

    /**
     * @brief Unregister a sync folder path from all connected FinderSync extensions.
     * @param path The absolute path to unregister.
     */
    void unregisterPath(const QString &path);

    /**
     * @brief Notify all extensions to update the view at the specified path.
     * @param path The absolute path where the view should be refreshed.
     */
    void updateViewAtPath(const QString &path);

    /**
     * @brief Send a status update for a file/folder to all connected extensions.
     * @param status The status string (e.g., "SYNC", "OK", "ERROR").
     * @param path The absolute path of the file/folder.
     */
    void setStatusResult(const QString &status, const QString &path);

    /**
     * @brief Send a localized string to all connected extensions.
     * @param key The string key.
     * @param value The localized string value.
     */
    void setLocalizedString(const QString &key, const QString &value);

    /**
     * @brief Reset menu items on all connected extensions.
     */
    void resetMenuItems();

    /**
     * @brief Add a menu item to all connected extensions.
     * @param command The command identifier.
     * @param flags The menu item flags.
     * @param text The menu item display text.
     */
    void addMenuItem(const QString &command, const QString &flags, const QString &text);

    /**
     * @brief Signal that menu items are complete.
     */
    void menuItemsComplete();

    // Allow the listener delegate to access _extensionProxies
    friend class FinderSyncXPCListenerDelegate;

private:
    //! Objective-C listener object (NSXPCListener*)
    void *_listener = nullptr;

    //! Connected extension proxies, keyed by connection identifier
    //! Values are NSObject<FinderSyncProtocol>* proxies
    QHash<QString, void*> _extensionProxies;
};

} // namespace OCC::Mac
