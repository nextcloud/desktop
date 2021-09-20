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

ColumnLayout {
    id: rootLayout
    spacing: 0
    property NC.UserStatusSelectorModel userStatusSelectorModel

    FontMetrics {
        id: metrics
    }

    Text {
        Layout.topMargin: 16
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        Layout.bottomMargin: 8
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        font.bold: true
        text: qsTr("Online status")
    }
        
    GridLayout {
        Layout.margins: 8
        Layout.alignment: Qt.AlignTop
        columns: 2
        rows: 2
        columnSpacing: 8
        rowSpacing: 8

        Button {
            Layout.fillWidth: true
            checked: NC.UserStatus.Online == userStatusSelectorModel.onlineStatus
            checkable: true
            icon.source: userStatusSelectorModel.onlineIcon
            icon.color: "transparent"
            text: qsTr("Online")
            onClicked: userStatusSelectorModel.setOnlineStatus(NC.UserStatus.Online)
            implicitWidth: 100
        }
        Button {
            Layout.fillWidth: true
            checked: NC.UserStatus.Away == userStatusSelectorModel.onlineStatus
            checkable: true
            icon.source: userStatusSelectorModel.awayIcon
            icon.color: "transparent"
            text: qsTr("Away")
            onClicked: userStatusSelectorModel.setOnlineStatus(NC.UserStatus.Away)
            implicitWidth: 100
            
        }
        Button {
            Layout.fillWidth: true
            checked: NC.UserStatus.DoNotDisturb == userStatusSelectorModel.onlineStatus
            checkable: true
            icon.source: userStatusSelectorModel.dndIcon
            icon.color: "transparent"
            text: qsTr("Do not disturb")
            onClicked: userStatusSelectorModel.setOnlineStatus(NC.UserStatus.DoNotDisturb)
            implicitWidth: 100
        }
        Button {
            Layout.fillWidth: true
            checked: NC.UserStatus.Invisible == userStatusSelectorModel.onlineStatus
            checkable: true
            icon.source: userStatusSelectorModel.invisibleIcon
            icon.color: "transparent"
            text: qsTr("Invisible")
            onClicked: userStatusSelectorModel.setOnlineStatus(NC.UserStatus.Invisible)
            implicitWidth: 100
        }
    }

    Text {
        Layout.topMargin: 16
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        Layout.bottomMargin: 8
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        font.bold: true
        text: qsTr("Status message")
    }

    RowLayout {
        Layout.topMargin: 8
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        Layout.bottomMargin: 16
        Layout.alignment: Qt.AlignTop
        Layout.fillWidth: true

        Button {
            Layout.preferredWidth: userStatusMessageTextField.height // metrics.height * 2
            Layout.preferredHeight: userStatusMessageTextField.height // metrics.height * 2
            text: userStatusSelectorModel.userStatusEmoji
            onClicked: emojiDialog.open()
        }

        Popup {
            id: emojiDialog
            padding: 0
            margins: 0

            anchors.centerIn: Overlay.overlay
            
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
            text: userStatusSelectorModel.userStatusMessage
            selectByMouse: true
            onEditingFinished: userStatusSelectorModel.setUserStatusMessage(text)
        }
    }

    Repeater {
        model: userStatusSelectorModel.predefinedStatusesCount

        Button {
            id: control
            Layout.fillWidth: true
            flat: !hovered
            hoverEnabled: true
            text: userStatusSelectorModel.predefinedStatus(index).icon + " <b>" + userStatusSelectorModel.predefinedStatus(index).message + "</b> - " + userStatusSelectorModel.predefinedStatusClearAt(index)
            onClicked: userStatusSelectorModel.setPredefinedStatus(index)
        }
    }

   RowLayout {
       Layout.topMargin: 16
       Layout.leftMargin: 8
       Layout.rightMargin: 8
       Layout.bottomMargin: 8
       Layout.alignment: Qt.AlignTop

       Text {
           text: qsTr("Clear status message after")
       }

       ComboBox {
           Layout.fillWidth: true
           model: userStatusSelectorModel.clearAtValues
           displayText: userStatusSelectorModel.clearAt
           onActivated: userStatusSelectorModel.setClearAt(index)
       }
   }

    RowLayout {
        Layout.margins: 8
        Layout.alignment: Qt.AlignTop
        
        Button {
            Layout.fillWidth: true
            text: qsTr("Clear status message")
            onClicked: userStatusSelectorModel.clearUserStatus()
        }
        Button {
            highlighted: true
            Layout.fillWidth: true
            text: qsTr("Set status message")
            onClicked: userStatusSelectorModel.setUserStatus()
        }
    }

    ErrorBox {
        Layout.margins: 8
        Layout.fillWidth: true
        
        visible: userStatusSelectorModel.errorMessage != ""
        text: "<b>Error:</b> " + userStatusSelectorModel.errorMessage
    }
}
