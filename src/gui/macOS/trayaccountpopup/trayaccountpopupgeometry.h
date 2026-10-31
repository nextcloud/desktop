/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

namespace OCC::Mac::TrayPopupGeometry
{

constexpr auto topAlignedPopupY(const double visibleFrameMinY, const double visibleFrameMaxY, const double popupHeight, const double edgePadding)
{
    const auto topAlignedY = visibleFrameMaxY - popupHeight;
    const auto minimumY = visibleFrameMinY + edgePadding;
    return topAlignedY < minimumY ? minimumY : topAlignedY;
}

} // namespace OCC::Mac::TrayPopupGeometry
