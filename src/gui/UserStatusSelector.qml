/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Window

import com.nextcloud.desktopclient as NC
import Style
import "./tray"

Column {
    id: root

    property NC.UserStatusSelectorModel userStatusSelectorModel
    property bool showStatusButtons: true
    property bool showStatusMessageControls: true
    property bool showStatusMessageNavigation: false

    signal finished()
    signal openStatusMessageRequested()

    spacing: Style.standardSpacing * 2
    width: parent ? parent.width : implicitWidth

    Column {
        id: statusButtonsLayout

        width: parent.width
        spacing: Style.smallSpacing
        visible: root.showStatusButtons

        EnforcedPlainTextLabel {
            width: parent.width
            bottomPadding: Style.smallSpacing
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
            text: qsTr("Online status")
        }

        UserStatusSelectorButton {
            width: parent.width
            checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Online
            checkable: true
            icon.source: userStatusSelectorModel.onlineIcon
            icon.color: "transparent"
            text: qsTr("Online")
            onClicked: userStatusSelectorModel.onlineStatus = NC.UserStatus.Online
        }
        UserStatusSelectorButton {
            width: parent.width
            checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Away
            checkable: true
            icon.source: userStatusSelectorModel.awayIcon
            icon.color: "transparent"
            text: qsTr("Away")
            onClicked: userStatusSelectorModel.onlineStatus = NC.UserStatus.Away
        }
        UserStatusSelectorButton {
            width: parent.width
            visible: userStatusSelectorModel.busyStatusSupported
            checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Busy
            checkable: true
            icon.source: userStatusSelectorModel.busyIcon
            icon.color: "transparent"
            text: qsTr("Busy")
            onClicked: userStatusSelectorModel.onlineStatus = NC.UserStatus.Busy
        }
        UserStatusSelectorButton {
            width: parent.width
            checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.DoNotDisturb
            checkable: true
            icon.source: userStatusSelectorModel.dndIcon
            icon.color: "transparent"
            text: qsTr("Do not disturb")
            secondaryText: qsTr("Mute all notifications")
            onClicked: userStatusSelectorModel.onlineStatus = NC.UserStatus.DoNotDisturb
        }
        UserStatusSelectorButton {
            width: parent.width
            checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Invisible
                     || userStatusSelectorModel.onlineStatus === NC.UserStatus.Offline
            checkable: true
            icon.source: userStatusSelectorModel.invisibleIcon
            icon.color: "transparent"
            text: qsTr("Invisible")
            secondaryText: qsTr("Appear offline")
            onClicked: userStatusSelectorModel.onlineStatus = NC.UserStatus.Invisible
        }

        Button {
            width: parent.width
            visible: root.showStatusMessageNavigation
            text: qsTr("Status Message")
            onClicked: root.openStatusMessageRequested()
        }
    }

    Column {
        id: userStatusMessageLayout

        width: parent.width
        spacing: Style.smallSpacing
        visible: root.showStatusMessageControls

        EnforcedPlainTextLabel {
            width: parent.width
            bottomPadding: Style.smallSpacing
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
            text: qsTr("Status message")
        }

        RowLayout {
            id: statusFieldLayout
            width: parent.width
            spacing: 0

            AbstractButton {
                id: fieldButton

                readonly property bool showBorder: hovered || checked || emojiDialog.visible

                Layout.preferredWidth: userStatusMessageTextField.height
                Layout.preferredHeight: userStatusMessageTextField.height

                text: "😀"
                padding: 0
                z: showBorder ? 2 : 0
                hoverEnabled: true

                property color borderColor: showBorder ? Style.ncBlue : palette.dark

                background: Rectangle {
                    radius: Style.slightlyRoundedButtonRadius
                    color: palette.button
                    border.color: fieldButton.borderColor
                    border.width: Style.normalBorderWidth
                }

                contentItem: Label {
                    text: fieldButton.text
                    opacity: 0.7
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: emojiDialog.open()
            }

            Binding {
                when: userStatusSelectorModel.userStatusEmoji.length > 0
                fieldButton {
                    text: userStatusSelectorModel.userStatusEmoji
                    contentItem.opacity: 1.0
                }
            }

            Popup {
                id: emojiDialog
                padding: 0
                margins: 0
                clip: true

                anchors.centerIn: Overlay.overlay

                background: Rectangle {
                    color: palette.base
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

                Layout.fillWidth: true
                placeholderText: qsTr("What is your status?")
                text: userStatusSelectorModel.userStatusMessage
                selectByMouse: true
                onEditingFinished: userStatusSelectorModel.userStatusMessage = text
            }
        }

        ScrollView {
            id: predefinedStatusesScrollView
            width: parent.width
            visible: userStatusSelectorModel.predefinedStatuses.length > 0
            clip: true

            implicitHeight: Math.min(predefinedStatusesList.contentHeight,
                                     Style.trayWindowHeight / 2)
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ListView {
                id: predefinedStatusesList
                width: parent.width
                spacing: 0
                model: userStatusSelectorModel.predefinedStatuses
                implicitHeight: contentHeight

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
            width: parent.width
            spacing: Style.smallSpacing

            EnforcedPlainTextLabel {
                id: clearComboLabel

                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignVCenter

                text: qsTr("Clear status message after")
                wrapMode: Text.Wrap
            }

            ComboBox {
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
        width: parent.width
        spacing: Style.smallSpacing

        Button {
            text: qsTr("Cancel")
            onClicked: root.finished()
        }
        Item {
            Layout.fillWidth: true
        }
        Button {
            visible: root.showStatusMessageControls
            text: qsTr("Clear")
            onClicked: userStatusSelectorModel.clearUserStatus()
        }
        Button {
            visible: root.showStatusMessageControls
            focusPolicy: Qt.StrongFocus
            text: qsTr("Apply")
            onClicked: userStatusSelectorModel.setUserStatus()
        }
    }
}
