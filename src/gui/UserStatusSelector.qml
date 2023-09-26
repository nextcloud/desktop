/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import QtQuick 2.6
import QtQuick.Dialogs
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

import com.nextcloud.desktopclient 1.0 as NC
import Style 1.0
import "./tray"

ColumnLayout {
    id: rootLayout
    spacing: Style.standardSpacing * 2
    property NC.UserStatusSelectorModel userStatusSelectorModel

    ColumnLayout {
        id: statusButtonsLayout

        Layout.fillWidth: true
        spacing: Style.smallSpacing

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            Layout.bottomMargin: Style.smallSpacing
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
            text: qsTr("Online status")
        }

        GridLayout {
            id: topButtonsLayout
            columns: 2
            rows: 2
            columnSpacing: statusButtonsLayout.spacing
            rowSpacing: statusButtonsLayout.spacing

            property int maxButtonHeight: 0
            function updateMaxButtonHeight(newHeight) {
                maxButtonHeight = Math.max(maxButtonHeight, newHeight)
            }

            UserStatusSelectorButton {
                checked: userStatusSelectorModel.onlineStatus === NC.userStatus.Online
                checkable: true
                icon.source: userStatusSelectorModel.onlineIcon
                icon.color: "transparent"
                text: qsTr("Online")
                onClicked: userStatusSelectorModel.onlineStatus = NC.userStatus.Online

                Layout.fillWidth: true
                implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width
            }
            UserStatusSelectorButton {
                checked: userStatusSelectorModel.onlineStatus === NC.userStatus.Away
                checkable: true
                icon.source: userStatusSelectorModel.awayIcon
                icon.color: "transparent"
                text: qsTr("Away")
                onClicked: userStatusSelectorModel.onlineStatus = NC.userStatus.Away

                Layout.fillWidth: true
                implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width

            }
            UserStatusSelectorButton {
                checked: userStatusSelectorModel.onlineStatus === NC.userStatus.DoNotDisturb
                checkable: true
                icon.source: userStatusSelectorModel.dndIcon
                icon.color: "transparent"
                text: qsTr("Do not disturb")
                secondaryText: qsTr("Mute all notifications")
                onClicked: userStatusSelectorModel.onlineStatus = NC.userStatus.DoNotDisturb

                Layout.fillWidth: true
                implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width
                Layout.preferredHeight: topButtonsLayout.maxButtonHeight
                onImplicitHeightChanged: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
                Component.onCompleted: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
            }
            UserStatusSelectorButton {
                checked: userStatusSelectorModel.onlineStatus === NC.userStatus.Invisible ||
                         userStatusSelectorModel.onlineStatus === NC.userStatus.Offline
                checkable: true
                icon.source: userStatusSelectorModel.invisibleIcon
                icon.color: "transparent"
                text: qsTr("Invisible")
                secondaryText: qsTr("Appear offline")
                onClicked: userStatusSelectorModel.onlineStatus = NC.userStatus.Invisible

                Layout.fillWidth: true
                implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width
                Layout.preferredHeight: topButtonsLayout.maxButtonHeight
                onImplicitHeightChanged: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
                Component.onCompleted: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
            }
        }
    }

    ColumnLayout {
        id: userStatusMessageLayout

        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Style.smallSpacing

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            Layout.bottomMargin: Style.smallSpacing
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
            text: qsTr("Status message")
        }

        RowLayout {
            id: statusFieldLayout
            Layout.fillWidth: true
            spacing: 0

            UserStatusSelectorButton {
                id: fieldButton

                Layout.preferredWidth: userStatusMessageTextField.height
                Layout.preferredHeight: userStatusMessageTextField.height

                text: userStatusSelectorModel.userStatusEmoji

                onClicked: emojiDialog.open()
                onHeightChanged: topButtonsLayout.maxButtonHeight = Math.max(topButtonsLayout.maxButtonHeight, height)

                primary: true
                padding: 0
                z: hovered ? 2 : 0 // Make sure highlight is seen on top of text field

                property color borderColor: showBorder ? Style.ncBlue : palette.dark

                // We create the square with only the top-left and bottom-left rounded corners
                // by overlaying different rectangles on top of each other
                background: Rectangle {
                    radius: Style.slightlyRoundedButtonRadius
                    color: palette.button
                    border.color: fieldButton.borderColor
                    border.width: Style.normalBorderWidth

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: parent.width / 2
                        anchors.rightMargin: -1
                        z: 1
                        color: palette.button
                        border.color: fieldButton.borderColor
                        border.width: Style.normalBorderWidth
                    }

                    Rectangle { // We need to cover the blue border of the non-radiused rectangle
                        anchors.fill: parent
                        anchors.leftMargin: parent.width / 4
                        anchors.rightMargin: parent.width / 4
                        anchors.topMargin: Style.normalBorderWidth
                        anchors.bottomMargin: Style.normalBorderWidth
                        z: 2
                        color: palette.button
                    }
                }
            }

            Popup {
                id: emojiDialog
                padding: 0
                margins: 0
                clip: true

                anchors.centerIn: Overlay.overlay

                background: Rectangle {
                    color: palette.toolTipBase
                    border.width: Style.normalBorderWidth
                    border.color: palette.dark
                    radius: Style.slightlyRoundedButtonRadius
                }

                EmojiPicker {
                    id: emojiPicker

                    onChosen: {
                        userStatusSelectorModel.userStatusEmoji = emoji
                        emojiDialog.close()
                    }
                }
            }

            TextField {
                id: userStatusMessageTextField

                property color borderColor: activeFocus ? Style.ncBlue : palette.dark

                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight + (Style.smallSpacing * 2)

                placeholderText: qsTr("What is your status?")
                placeholderTextColor: palette.midlight
                text: userStatusSelectorModel.userStatusMessage
                verticalAlignment: TextInput.AlignVCenter
                selectByMouse: true
                onEditingFinished: userStatusSelectorModel.userStatusMessage = text

                background: Rectangle {
                    radius: Style.slightlyRoundedButtonRadius
                    color: palette.base
                    border.color: userStatusMessageTextField.borderColor
                    border.width: Style.normalBorderWidth

                    Rectangle {
                        anchors.fill: parent
                        anchors.rightMargin: parent.width / 2
                        z: 1
                        color: palette.base
                        border.color: userStatusMessageTextField.borderColor
                        border.width: Style.normalBorderWidth
                    }

                    Rectangle { // We need to cover the blue border of the non-radiused rectangle
                        anchors.fill: parent
                        anchors.leftMargin: parent.width / 4
                        anchors.rightMargin: parent.width / 4
                        anchors.topMargin: Style.normalBorderWidth
                        anchors.bottomMargin: Style.normalBorderWidth
                        z: 2
                        color: palette.base
                    }
                }
            }
        }

        ScrollView {
            id: predefinedStatusesScrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
	    
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ListView {
                spacing: 0
                model: userStatusSelectorModel.predefinedStatuses
                delegate: PredefinedStatusButton {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    leftPadding: 0
                    emojiWidth: fieldButton.width
                    internalSpacing: statusFieldLayout.spacing + userStatusMessageTextField.leftPadding

                    emoji: modelData.icon
                    statusText: modelData.message
                    clearAtText: userStatusSelectorModel.clearAtReadable(modelData)
                    onClicked: userStatusSelectorModel.setPredefinedStatus(modelData)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Style.smallSpacing

            EnforcedPlainTextLabel {
                id: clearComboLabel

                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignVCenter

                text: qsTr("Clear status message after")
                wrapMode: Text.Wrap
            }

            BasicComboBox {
                id: clearComboBox

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: implicitWidth

                model: userStatusSelectorModel.clearStageTypes
                textRole: "display"
                valueRole: "clearStageType"
                displayText: userStatusSelectorModel.clearAtDisplayString
                onActivated: userStatusSelectorModel.setClearAt(currentValue)
            }
        }
    }

    ErrorBox {
        width: parent.width

        visible: userStatusSelectorModel.errorMessage != ""
        text: "Error: " + userStatusSelectorModel.errorMessage
    }

    RowLayout {
        id: bottomButtonBox
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignBottom

        UserStatusSelectorButton {
            // Prevent being squashed by the other buttons with larger text
            Layout.minimumWidth: implicitWidth
            Layout.fillHeight: true
            primary: true
            text: qsTr("Cancel")
            onClicked: finished()
        }
        UserStatusSelectorButton {
            Layout.fillWidth: true
            Layout.fillHeight: true
            primary: true
            text: qsTr("Clear status message")
            onClicked: userStatusSelectorModel.clearUserStatus()
        }
        UserStatusSelectorButton {
            Layout.fillWidth: true
            Layout.fillHeight: true
            primary: true
            colored: true
            text: qsTr("Set status message")
            onClicked: userStatusSelectorModel.setUserStatus()
        }
    }
}
