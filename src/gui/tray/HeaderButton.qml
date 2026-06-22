/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQml.Models
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

// Custom qml modules are in /theme (and included by resources.qrc)
import Style
import com.nextcloud.desktopclient

Button {
    id: root

    display: AbstractButton.IconOnly
    flat: true
    hoverEnabled: Style.hoverEffectsEnabled

    icon.width: Style.headerButtonIconSize
    icon.height: Style.headerButtonIconSize
    icon.color: "transparent"

    palette {
        text: Style.currentUserHeaderTextColor
        windowText: Style.currentUserHeaderTextColor
        buttonText: Style.currentUserHeaderTextColor
        button: Style.adjustedCurrentUserHeaderColor
    }

    Layout.alignment: Qt.AlignRight
    Layout.preferredWidth:  Style.trayWindowHeaderHeight
    Layout.preferredHeight: Style.trayWindowHeaderHeight
}
