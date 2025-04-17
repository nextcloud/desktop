/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls

import Style

ToolTip {
    id: toolTip
    clip: true
    delay: Qt.styleHints.mousePressAndHoldInterval
    contentItem: EnforcedPlainTextLabel {
        text: toolTip.text
        wrapMode: Text.Wrap
    }
}
