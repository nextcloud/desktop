/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

namespace OCC {

class SocketApi;

namespace Mac {

/**
 * @brief Service that implements the FinderSyncAppProtocol for XPC communication from FinderSync extensions.
 *
 * This class provides the implementation of the FinderSyncAppProtocol, allowing FinderSync extensions
 * to communicate with the main application through XPC (Apple's Inter-Process Communication mechanism).
 *
 * **Architecture Note**: Despite its name, SocketApi is NOT socket-specific. It's the core business logic
 * class that handles file status queries, menu commands, and folder operations across ALL platforms
 * (Windows, Linux, macOS). This service acts as a transport adapter, translating XPC calls into
 * SocketApi method calls, reusing the battle-tested business logic instead of duplicating code.
 *
 * **Transport Evolution**:
 * - Legacy: FinderSync → UNIX Socket → SocketLineProcessor → SocketApi
 * - Current: FinderSync → XPC → FinderSyncService → SocketApi
 *
 * The socket transport layer was removed, but the SocketApi business logic remains shared across platforms.
 */
class FinderSyncService : public QObject
{
    Q_OBJECT

public:
    explicit FinderSyncService(QObject *parent = nullptr);
    ~FinderSyncService() override;

    /**
     * @brief Get the Objective-C delegate object that implements FinderSyncAppProtocol.
     * @return The delegate pointer (void* to avoid Objective-C in header).
     */
    [[nodiscard]] void *delegate() const;

    /**
     * @brief Set the SocketApi instance to forward requests to.
     *
     * Note: SocketApi is the core business logic class (not socket-specific) that handles
     * file status queries, menu commands, and sync operations across all platforms.
     *
     * @param socketApi The SocketApi instance (must remain valid for the lifetime of this service).
     */
    void setSocketApi(SocketApi *socketApi);

    /**
     * @brief Get the current SocketApi instance.
     *
     * @return The SocketApi pointer, or nullptr if not set.
     */
    [[nodiscard]] SocketApi *socketApi() const;

    /**
     * @brief Get file data for a given path (accesses private SocketApi::FileData).
     *
     * This helper method exists because friend declarations don't extend to lambdas
     * or Objective-C contexts. The friend relationship allows FinderSyncService to
     * access FileData, and this method provides that access to our implementation.
     *
     * @param path The file path to query.
     * @return A tuple of (hasFolder, statusString) where hasFolder indicates if the
     *         file is in a sync folder, and statusString is the status (e.g., "OK", "SYNC").
     */
    [[nodiscard]] std::pair<bool, QString> getFileStatus(const QString &path) const;

    /**
     * @brief Get localized strings from SocketApi.
     *
     * Calls SocketApi::command_GET_STRINGS and returns the result as a QMap.
     *
     * @return Map of string keys to localized values.
     */
    [[nodiscard]] QMap<QString, QString> getLocalizedStrings() const;

    /**
     * @brief Get menu items for given paths from SocketApi.
     *
     * Calls SocketApi::command_GET_MENU_ITEMS and returns the parsed menu items.
     *
     * @param paths List of file paths to get menu items for.
     * @return List of menu items (each item is a map with command, flags, text keys).
     */
    [[nodiscard]] QList<QMap<QString, QString>> getMenuItems(const QStringList &paths) const;

private:
    class MacImplementation;
    std::unique_ptr<MacImplementation> d;

    SocketApi *_socketApi = nullptr;
};

} // namespace Mac

} // namespace OCC
