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
 * to communicate with the main application through XPC. It bridges XPC calls to the existing SocketApi
 * implementation to reuse the business logic.
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
     * @param socketApi The SocketApi instance (must remain valid for the lifetime of this service).
     */
    void setSocketApi(SocketApi *socketApi);

    /**
     * @brief Get the current SocketApi instance.
     * @return The SocketApi pointer, or nullptr if not set.
     */
    [[nodiscard]] SocketApi *socketApi() const;

private:
    class MacImplementation;
    std::unique_ptr<MacImplementation> d;

    SocketApi *_socketApi = nullptr;
};

} // namespace Mac

} // namespace OCC
