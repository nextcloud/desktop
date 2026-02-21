/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>

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

    /**
     * @brief Store an extension proxy (called by listener delegate).
     * @param connectionId Unique identifier for this connection.
     * @param proxy The retained proxy object (void* to NSObject<FinderSyncProtocol>*).
     * @param connection The NSXPCConnection object (void* to NSXPCConnection*).
     */
    void storeExtensionProxy(const QString &connectionId, void *proxy, void *connection);

    /**
     * @brief Remove an extension proxy when connection is invalidated (called by listener delegate).
     * @param connection The NSXPCConnection object (void* to NSXPCConnection*).
     */
    void removeExtensionProxy(void *connection);

private:
    //! Objective-C listener object (NSXPCListener*)
    void *_listener = nullptr;

    //! Objective-C listener delegate (FinderSyncXPCListenerDelegate*)
    //! Must be retained separately because NSXPCListener holds weak reference
    void *_listenerDelegate = nullptr;

    //! Connected extension proxies, keyed by connection identifier
    //! Values are NSObject<FinderSyncProtocol>* proxies
    //! Protected by _proxiesMutex for thread-safe access
    QHash<QString, void*> _extensionProxies;

    //! Reverse mapping from NSXPCConnection* to connection identifier
    //! Used to find connectionId when connection is invalidated
    //! Protected by _proxiesMutex for thread-safe access
    QHash<void*, QString> _connectionToId;

    //! Mutex protecting _extensionProxies and _connectionToId access from multiple threads
    //! (XPC listener thread, main thread, destructor)
    mutable QMutex _proxiesMutex;
};

} // namespace OCC::Mac
