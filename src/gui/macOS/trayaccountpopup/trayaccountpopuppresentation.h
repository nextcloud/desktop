/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <utility>

namespace OCC::Mac {

/**
 * @brief Runs the operations needed to present the native tray popup.
 *
 * The popup is ordered before application activation is requested so an
 * LSUIElement launch does not make the activation request with zero windows.
 */
class TrayAccountPopupPresentation
{
public:
    /**
     * @brief Orders the popup, requests application activation, then makes it key.
     */
    template<typename OrderFront, typename ActivateApplication, typename MakeKey>
    static void present(OrderFront &&orderFront, ActivateApplication &&activateApplication, MakeKey &&makeKey)
    {
        std::forward<OrderFront>(orderFront)();
        std::forward<ActivateApplication>(activateApplication)();
        std::forward<MakeKey>(makeKey)();
    }
};

} // namespace OCC::Mac
