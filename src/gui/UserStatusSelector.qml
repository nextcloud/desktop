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
    spacing: 0
    property NC.UserStatusSelectorModel userStatusSelectorModel

    Label {
        Layout.topMargin: Style.standardSpacing * 2
        Layout.leftMargin: Style.standardSpacing
        Layout.rightMargin: Style.standardSpacing
        Layout.bottomMargin: Style.standardSpacing
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        font.bold: true
        text: qsTr("Online status")
        color: Style.ncTextColor
    }
        
    GridLayout {
        id: topButtonsLayout

        Layout.margins: Style.standardSpacing
        Layout.alignment: Qt.AlignTop
        columns: 2
        rows: 2
        columnSpacing: Style.standardSpacing
        rowSpacing: Style.standardSpacing

        property int maxButtonHeight: 0
        function updateMaxButtonHeight(newHeight) {
            maxButtonHeight = Math.max(maxButtonHeight, newHeight)
        }

        UserStatusSelectorButton {
            checked: NC.UserStatus.Online == userStatusSelectorModel.onlineStatus
            checkable: true
            icon.source: userStatusSelectorModel.onlineIcon
            icon.color: "transparent"
            text: qsTr("Online")
            onClicked: userStatusSelectorModel.setOnlineStatus(NC.UserStatus.Online)

            Layout.fillWidth: true
            implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width
            Layout.preferredHeight: topButtonsLayout.maxButtonHeight
            onImplicitHeightChanged: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
            Component.onCompleted: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
        }
        UserStatusSelectorButton {
            checked: NC.UserStatus.Away == userStatusSelectorModel.onlineStatus
            checkable: true
            icon.source: userStatusSelectorModel.awayIcon
            icon.color: "transparent"
            text: qsTr("Away")
            onClicked: userStatusSelectorModel.setOnlineStatus(NC.UserStatus.Away)

            Layout.fillWidth: true
            implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width
            Layout.preferredHeight: topButtonsLayout.maxButtonHeight
            onImplicitHeightChanged: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
            Component.onCompleted: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
            
        }
        UserStatusSelectorButton {
            checked: NC.UserStatus.DoNotDisturb == userStatusSelectorModel.onlineStatus
            checkable: true
            icon.source: userStatusSelectorModel.dndIcon
            icon.color: "transparent"
            text: qsTr("Do not disturb")
            secondaryText: qsTr("Mute all notifications")
            onClicked: userStatusSelectorModel.setOnlineStatus(NC.UserStatus.DoNotDisturb)

            Layout.fillWidth: true
            implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width
            Layout.preferredHeight: topButtonsLayout.maxButtonHeight
            onImplicitHeightChanged: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
            Component.onCompleted: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
        }
        UserStatusSelectorButton {
            checked: NC.UserStatus.Invisible == userStatusSelectorModel.onlineStatus
            checkable: true
            icon.source: userStatusSelectorModel.invisibleIcon
            icon.color: "transparent"
            text: qsTr("Invisible")
            secondaryText: qsTr("Appear offline")
            onClicked: userStatusSelectorModel.setOnlineStatus(NC.UserStatus.Invisible)

            Layout.fillWidth: true
            implicitWidth: 200 // Pretty much a hack to ensure all the buttons are equal in width
            Layout.preferredHeight: topButtonsLayout.maxButtonHeight
            onImplicitHeightChanged: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
            Component.onCompleted: topButtonsLayout.updateMaxButtonHeight(implicitHeight)
        }
    }

    Label {
        Layout.topMargin: Style.standardSpacing * 2
        Layout.leftMargin: Style.standardSpacing
        Layout.rightMargin: Style.standardSpacing
        Layout.bottomMargin: Style.standardSpacing
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        font.bold: true
        text: qsTr("Status message")
        color: Style.ncTextColor
    }

    RowLayout {
        Layout.topMargin: Style.standardSpacing
        Layout.leftMargin: Style.standardSpacing
        Layout.rightMargin: Style.standardSpacing
        Layout.bottomMargin: Style.standardSpacing * 2
        Layout.alignment: Qt.AlignTop
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
            onEditingFinished: userStatusSelectorModel.setUserStatusMessage(text)

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

    Repeater {
        model: userStatusSelectorModel.predefinedStatusesCount

        PredefinedStatusButton {
            id: control
            Layout.fillWidth: true
            Layout.leftMargin: Style.standardSpacing
            Layout.rightMargin: Style.standardSpacing
            internalSpacing: Style.standardSpacing + fieldButton.padding + userStatusMessageTextField.padding

            emoji: userStatusSelectorModel.predefinedStatus(index).icon
            text: "<b>" + userStatusSelectorModel.predefinedStatus(index).message + "</b> - " + userStatusSelectorModel.predefinedStatusClearAt(index)
            onClicked: userStatusSelectorModel.setPredefinedStatus(index)
        }
    }

   RowLayout {
       Layout.topMargin: Style.standardSpacing * 2
       Layout.leftMargin: Style.standardSpacing
       Layout.rightMargin: Style.standardSpacing
       Layout.bottomMargin: Style.standardSpacing
       Layout.alignment: Qt.AlignTop
       spacing: Style.standardSpacing

       Label {
           text: qsTr("Clear status message after")
           color: Style.ncTextColor
       }

       BasicComboBox {
           id: clearComboBox

           Layout.fillWidth: true
           model: userStatusSelectorModel.clearAtValues
           displayText: userStatusSelectorModel.clearAt
           onActivated: userStatusSelectorModel.setClearAt(index)
       }
   }

    RowLayout {
        Layout.margins: Style.standardSpacing
        Layout.alignment: Qt.AlignTop
        
        UserStatusSelectorButton {
            Layout.fillWidth: true
            primary: true
            text: qsTr("Clear status message")
            onClicked: userStatusSelectorModel.clearUserStatus()
        }
        UserStatusSelectorButton {
            primary: true
            colored: true
            Layout.fillWidth: true
            text: qsTr("Set status message")
            onClicked: userStatusSelectorModel.setUserStatus()
        }
    }

    ErrorBox {
        Layout.margins: Style.standardSpacing
        Layout.fillWidth: true
        
        visible: userStatusSelectorModel.errorMessage != ""
        text: "<b>Error:</b> " + userStatusSelectorModel.errorMessage
    }
}
