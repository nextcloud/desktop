/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <array>
#include <span>

namespace OCC {

/**
 * @brief Defines which account-menu content is relevant for the connection state.
 */
class TrayAccountMenuPolicy
{
public:
    /** @brief Semantic entries used by the disconnected account menu. */
    enum class Entry {
        LocalFolder,
        Separator,
        Reconnect,
    };

    /** @brief Account operation selected for the reconnect action. */
    enum class ReconnectMode {
        None,
        SignIn,
        RetryConnection,
    };

    /**
     * @brief Creates the menu policy for an account.
     * @param canReconnect Whether the account supports signing in, which excludes public shares.
     */
    explicit constexpr TrayAccountMenuPolicy(const bool isConnected, const bool canReconnect)
        : _isConnected(isConnected)
        , _canReconnect(canReconnect)
    {
    }

    /** @brief Whether server-backed account sections should be shown. */
    [[nodiscard]] constexpr bool showConnectedSections() const
    {
        return _isConnected;
    }

    /** @brief Ordered entries to show instead of server-backed sections. */
    [[nodiscard]] constexpr std::span<const Entry> disconnectedEntries() const
    {
        if (_isConnected) {
            return {};
        }
        if (_canReconnect) {
            return disconnectedReconnectEntries;
        }
        return disconnectedEntriesWithoutReconnect;
    }

    /** @brief Whether opening the menu should request fresh server-backed previews. */
    [[nodiscard]] constexpr bool fetchActivityPreview() const
    {
        return _isConnected;
    }

    /** @brief Selects the account operation behind the reconnect menu action. */
    [[nodiscard]] static constexpr ReconnectMode reconnectMode(
        const bool isConnected,
        const bool isSignedOut,
        const bool canReconnect)
    {
        if (isConnected || !canReconnect) {
            return ReconnectMode::None;
        }
        return isSignedOut
            ? ReconnectMode::SignIn
            : ReconnectMode::RetryConnection;
    }

private:
    static constexpr std::array disconnectedReconnectEntries{
        Entry::LocalFolder,
        Entry::Separator,
        Entry::Reconnect,
    };
    static constexpr std::array disconnectedEntriesWithoutReconnect{
        Entry::LocalFolder,
    };

    bool _isConnected;
    bool _canReconnect;
};

}
