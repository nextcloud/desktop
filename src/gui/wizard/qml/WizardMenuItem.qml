/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls
import QtQuick.Effects
import QtQuick.Layouts

import Style

BasicControls.MenuItem {
    id: root

    property bool tintIcon: false
    property color iconTintColor: Style.wizardPrimaryText

    hoverEnabled: true
    implicitHeight: Style.standardPrimaryButtonHeight
    leftPadding: Style.wizardMenuItemHorizontalPadding
    rightPadding: Style.wizardMenuItemHorizontalPadding
    font.pixelSize: Style.pixelSize + Style.extraSmallSpacing

    contentItem: RowLayout {
        spacing: Style.smallSpacing

        Item {
            Layout.preferredWidth: visible ? Style.smallIconSize : 0
            Layout.preferredHeight: Style.smallIconSize
            visible: root.icon.source.toString() !== ""

            Image {
                id: menuIconImage

                anchors.fill: parent
                visible: !root.tintIcon
                source: root.icon.source.toString() !== "" ? root.icon.source : ""
                sourceSize.width: Style.smallIconSize
                sourceSize.height: Style.smallIconSize
                fillMode: Image.PreserveAspectFit
                Accessible.ignored: true
            }

            MultiEffect {
                objectName: "wizardMenuItemIconTint"
                anchors.fill: menuIconImage
                visible: root.tintIcon
                source: menuIconImage
                colorization: 1.0
                colorizationColor: root.iconTintColor
                Accessible.ignored: true
            }

            Accessible.ignored: true
        }

        Text {
            Layout.fillWidth: true
            text: root.text
            font: root.font
            color: root.enabled ? Style.wizardPrimaryText : Style.wizardDisabledText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        color: root.hovered || root.highlighted || root.down
            ? Style.wizardSecondaryButtonPressed
            : "transparent"
        radius: Style.mediumRoundedButtonRadius
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
