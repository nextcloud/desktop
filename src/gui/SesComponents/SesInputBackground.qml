/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick

import Style 1.0

Rectangle {
    anchors.fill: parent
    radius: Style.slightlyRoundedButtonRadius
    border.width: Style.thickBorderWidth
    border.color: Style.sesTrayInputField
    color: Style.sesBackgroundColor
    z: -1
}
