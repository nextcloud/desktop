/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <array>
#include <span>

namespace OCC {

/** @brief Semantic entries used by the disconnected account menu. */
enum class TrayAccountMenuEntry {
    LocalFolder,
    Separator,
    Reconnect,
};

/** @brief Account operation selected for the reconnect action. */
enum class TrayAccountReconnectMode {
    None,
    SignIn,
    RetryConnection,
};

/**
 * @brief Defines which account-menu content is relevant for the connection state.
 */
class TrayAccountMenuPolicy
{
public:
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
    [[nodiscard]] constexpr std::span<const TrayAccountMenuEntry> disconnectedEntries() const
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
    [[nodiscard]] static constexpr TrayAccountReconnectMode reconnectMode(
        const bool isConnected,
        const bool isSignedOut,
        const bool canReconnect)
    {
        if (isConnected || !canReconnect) {
            return TrayAccountReconnectMode::None;
        }
        return isSignedOut
            ? TrayAccountReconnectMode::SignIn
            : TrayAccountReconnectMode::RetryConnection;
    }

private:
    static constexpr std::array disconnectedReconnectEntries{
        TrayAccountMenuEntry::LocalFolder,
        TrayAccountMenuEntry::Separator,
        TrayAccountMenuEntry::Reconnect,
    };
    static constexpr std::array disconnectedEntriesWithoutReconnect{
        TrayAccountMenuEntry::LocalFolder,
    };

    bool _isConnected;
    bool _canReconnect;
};

}
