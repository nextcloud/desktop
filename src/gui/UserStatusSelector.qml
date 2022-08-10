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
import QtQuick.Dialogs 1.3
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import com.nextcloud.desktopclient 1.0 as NC

import Style 1.0

ColumnLayout {
    id: rootLayout
    spacing: Style.standardSpacing * 2
    property NC.UserStatusSelectorModel userStatusSelectorModel

    Column {
        // We use a normal column here because layouts often don't adjust to any custom
        // alignments for each other. If Item 2 is below Item 1, Item 2 will always set
        // its alignment in relation to Item 1 being in default alignment of vertically
        // centered. So when we set Item 2 to align top, even if Item 1 is aligned top,
        // Item 2 will align itself as if Item 1 were vertically centered.
        //
        // Since in this case we want to set everything to align top, we use the Column
        // which does this well, have it fill the height of the parent ColumnLayout,
        // pushing the bottom button box down.

        id: mainContentsLayout
        spacing: rootLayout.spacing

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop

        ColumnLayout {
            id: statusButtonsLayout
            width: parent.width
            spacing: Style.smallSpacing

            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: Style.smallSpacing
                horizontalAlignment: Text.AlignHCenter
                font.bold: true
                text: qsTr("Online status")
                color: Style.ncTextColor
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
                    checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Online
                    checkable: true
                    icon.source: userStatusSelectorModel.onlineIcon
                    icon.color: "transparent"
                    text: qsTr("Online")
                    onClicked: userStatusSelectorModel.onlineStatus = NC.UserStatus.Online

                    Layout.fillWidth: true
                    implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width
                }
                UserStatusSelectorButton {
                    checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Away
                    checkable: true
                    icon.source: userStatusSelectorModel.awayIcon
                    icon.color: "transparent"
                    text: qsTr("Away")
                    onClicked: userStatusSelectorModel.onlineStatus = NC.UserStatus.Away

                    Layout.fillWidth: true
                    implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width

                }
                UserStatusSelectorButton {
                    checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.DoNotDisturb
                    checkable: true
                    icon.source: userStatusSelectorModel.dndIcon
                    icon.color: "transparent"
                    text: qsTr("Do not disturb")
                    secondaryText: qsTr("Mute all notifications")
                    onClicked: userStatusSelectorModel.onlineStatus = NC.UserStatus.DoNotDisturb

                    Layout.fillWidth: true
                    implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width
                    Layout.preferredHeight: topButtonsLayout.maxButtonHeight
                    onImplicitHeightChanged: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
                    Component.onCompleted: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
                }
                UserStatusSelectorButton {
                    checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Invisible
                    checkable: true
                    icon.source: userStatusSelectorModel.invisibleIcon
                    icon.color: "transparent"
                    text: qsTr("Invisible")
                    secondaryText: qsTr("Appear offline")
                    onClicked: userStatusSelectorModel.onlineStatus = NC.UserStatus.Invisible

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
            width: parent.width
            spacing: Style.smallSpacing

            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: Style.smallSpacing
                horizontalAlignment: Text.AlignHCenter
                font.bold: true
                text: qsTr("Status message")
                color: Style.ncTextColor
            }

            RowLayout {
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

                    property color borderColor: showBorder ? Style.ncBlue : Style.menuBorder

                    // We create the square with only the top-left and bottom-left rounded corners
                    // by overlaying different rectangles on top of each other
                    background: Rectangle {
                        radius: Style.slightlyRoundedButtonRadius
                        color: Style.buttonBackgroundColor
                        border.color: fieldButton.borderColor
                        border.width: Style.normalBorderWidth

                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: parent.width / 2
                            anchors.rightMargin: -1
                            z: 1
                            color: Style.buttonBackgroundColor
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
                            color: Style.buttonBackgroundColor
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
                        color: Style.backgroundColor
                        border.width: Style.normalBorderWidth
                        border.color: Style.menuBorder
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
                    Layout.fillWidth: true
                    placeholderText: qsTr("What is your status?")
                    placeholderTextColor: Style.ncSecondaryTextColor
                    text: userStatusSelectorModel.userStatusMessage
                    color: Style.ncTextColor
                    selectByMouse: true
                    onEditingFinished: userStatusSelectorModel.userStatusMessage = text

                    property color borderColor: activeFocus ? Style.ncBlue : Style.menuBorder

                    background: Rectangle {
                        radius: Style.slightlyRoundedButtonRadius
                        color: Style.backgroundColor
                        border.color: userStatusMessageTextField.borderColor
                        border.width: Style.normalBorderWidth

                        Rectangle {
                            anchors.fill: parent
                            anchors.rightMargin: parent.width / 2
                            z: 1
                            color: Style.backgroundColor
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
                            color: Style.backgroundColor
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Repeater {
                    model: userStatusSelectorModel.predefinedStatuses

                    PredefinedStatusButton {
                        id: control
                        Layout.fillWidth: true
                        internalSpacing: Style.standardSpacing + fieldButton.padding + userStatusMessageTextField.padding

                        emoji: modelData.icon
                        text: "<b>%1</b> – %2".arg(modelData.message).arg(userStatusSelectorModel.clearAtReadable(modelData))
                        onClicked: userStatusSelectorModel.setPredefinedStatus(modelData)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Style.standardSpacing

                Label {
                    text: qsTr("Clear status message after")
                    color: Style.ncTextColor
                }

                BasicComboBox {
                    id: clearComboBox
                    Layout.fillWidth: true
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
            text: "<b>Error:</b> " + userStatusSelectorModel.errorMessage
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignBottom

        UserStatusSelectorButton {
            Layout.fillWidth: true
            primary: true
            text: qsTr("Cancel")
            onClicked: finished()
        }
        UserStatusSelectorButton {
            Layout.fillWidth: true
            primary: true
            text: qsTr("Clear status message")
            onClicked: userStatusSelectorModel.clearUserStatus()
        }
        UserStatusSelectorButton {
            Layout.fillWidth: true
            primary: true
            colored: true
            text: qsTr("Set status message")
            onClicked: userStatusSelectorModel.setUserStatus()
        }
    }
}
