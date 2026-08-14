/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Style

RowLayout {
    id: root

    required property string title
    required property string description

    spacing: Style.smallSpacing

    EnforcedPlainTextLabel {
        text: root.title
        font.weight: Font.DemiBold
    }

    ToolButton {
        visible: root.description.length > 0
        display: AbstractButton.IconOnly
        icon.source: "image://svgimage-custom-color/info.svg/" + palette.buttonText

        Accessible.name: qsTr("About %1").arg(root.title)
        Accessible.description: root.description
        ToolTip.visible: hovered
        ToolTip.text: root.description
        ToolTip.delay: 0
    }

    Item {
        Layout.fillWidth: true
    }
}
