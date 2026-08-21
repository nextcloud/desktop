/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import Style

BasicControls.Button {
    id: root

    property bool primary: false
    property string iconSource: ""
    property bool iconBeforeText: false
    property bool tintIcon: false
    property color iconTintColor: Style.wizardPrimaryText
    property real cornerRadius: Style.mediumRoundedButtonRadius
    property string textSuffix: ""
    property string trailingIconSource: ""
    readonly property color primaryColor: Style.wizardPrimaryButtonBackground
    readonly property color primaryPressedColor: Style.wizardPrimaryButtonPressed
    readonly property color secondaryColor: Style.wizardSecondaryButtonBackground
    readonly property color secondaryPressedColor: Style.wizardSecondaryButtonPressed
    readonly property color secondaryBorderColor: Style.wizardSecondaryButtonBorder
    readonly property color disabledColor: Style.wizardDisabledButtonBackground
    readonly property color disabledBorderColor: Style.wizardDisabledButtonBorder

    implicitHeight: Style.wizardFooterButtonHeight
    leftPadding: 18
    rightPadding: 18
    font.pixelSize: Style.pixelSize + 3
    font.weight: Font.Medium
    Accessible.role: Accessible.Button
    Accessible.name: textSuffix === "" ? text : text + " " + textSuffix

    contentItem: RowLayout {
        spacing: 6

        Item {
            visible: root.iconSource !== "" && root.iconBeforeText
            Layout.preferredWidth: visible ? Style.smallIconSize : 0
            Layout.preferredHeight: Style.smallIconSize

            Image {
                id: leadingIconImage

                anchors.fill: parent
                visible: !root.tintIcon
                source: root.iconSource !== "" && root.iconBeforeText ? root.iconSource : ""
                sourceSize.width: Style.smallIconSize
                sourceSize.height: Style.smallIconSize
                fillMode: Image.PreserveAspectFit
                Accessible.ignored: true
            }

            ColorOverlay {
                objectName: "wizardButtonLeadingIconTint"
                anchors.fill: leadingIconImage
                visible: root.tintIcon
                source: leadingIconImage
                color: root.iconTintColor
                cached: true
                Accessible.ignored: true
            }
        }

        Text {
            objectName: "wizardButtonText"
            Layout.fillWidth: true
            text: root.textSuffix === "" ? root.text : root.text + " " + root.textSuffix
            font: root.font
            color: root.enabled
                ? (root.primary ? Style.wizardSelectedText : root.palette.buttonText)
                : Style.wizardDisabledText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Image {
            visible: root.iconSource !== "" && !root.iconBeforeText
            source: root.iconSource !== "" && !root.iconBeforeText ? root.iconSource : ""
            sourceSize.width: Style.smallIconSize
            sourceSize.height: Style.smallIconSize
            Layout.preferredWidth: visible ? Style.smallIconSize : 0
            Layout.preferredHeight: Style.smallIconSize
            fillMode: Image.PreserveAspectFit
            Accessible.ignored: true
        }

        Image {
            id: trailingIcon

            objectName: "wizardButtonTrailingIcon"
            visible: root.trailingIconSource !== ""
            source: root.trailingIconSource
            sourceSize.width: Style.smallIconSize
            sourceSize.height: Style.smallIconSize
            Layout.preferredWidth: visible ? Style.smallIconSize : 0
            Layout.preferredHeight: Style.smallIconSize
            fillMode: Image.PreserveAspectFit
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: root.cornerRadius
        border.width: root.primary ? 0 : 1
        border.color: root.enabled ? root.secondaryBorderColor : root.disabledBorderColor
        color: {
            if (!root.enabled) {
                return root.disabledColor
            }
            if (root.primary) {
                return root.down || hoverArea.containsMouse
                    ? root.primaryPressedColor
                    : root.primaryColor
            }
            return root.down || hoverArea.containsMouse
                ? root.secondaryPressedColor
                : root.secondaryColor
        }
    }

    MouseArea {
        id: hoverArea

        objectName: "wizardButtonHoverArea"
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        enabled: root.enabled
        hoverEnabled: enabled
        cursorShape: Qt.PointingHandCursor
    }
}
