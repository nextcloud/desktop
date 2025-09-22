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

ColumnLayout {
    id: rootLayout
    spacing: Style.standardSpacing * 2
    property NC.UserStatusSelectorModel userStatusSelectorModel
    property bool showOnlineStatusSection: true
    property bool showStatusMessageSection: true

    signal closeRequested()
    signal showStatusMessageRequested()

    function handleOnlineStatusSelection(status) {
        const currentStatus = userStatusSelectorModel.onlineStatus;
        const alreadySelected = currentStatus === status
                || (status === NC.UserStatus.Invisible
                    && currentStatus === NC.UserStatus.Offline);

        if (alreadySelected) {
            closeRequested();
            return;
        }

        userStatusSelectorModel.onlineStatus = status;
    }

    ColumnLayout {
        id: statusButtonsLayout

        Layout.fillWidth: true
        spacing: Style.smallSpacing
        visible: rootLayout.showOnlineStatusSection
        Layout.preferredHeight: visible ? implicitHeight : 0

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            Layout.bottomMargin: Style.smallSpacing
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
            text: qsTr("Online status")
        }

        ColumnLayout {
            id: topButtonsLayout

            Layout.fillWidth: true
            spacing: statusButtonsLayout.spacing

            UserStatusSelectorButton {
                checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Online
                checkable: true
                icon.source: userStatusSelectorModel.onlineIcon
                icon.color: "transparent"
                text: qsTr("Online")
                onClicked: handleOnlineStatusSelection(NC.UserStatus.Online)

                Layout.fillWidth: true
            }
            UserStatusSelectorButton {
                checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Away
                checkable: true
                icon.source: userStatusSelectorModel.awayIcon
                icon.color: "transparent"
                text: qsTr("Away")
                onClicked: handleOnlineStatusSelection(NC.UserStatus.Away)

                Layout.fillWidth: true

            }
            UserStatusSelectorButton {
                checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.DoNotDisturb
                checkable: true
                icon.source: userStatusSelectorModel.dndIcon
                icon.color: "transparent"
                text: qsTr("Do not disturb")
                secondaryText: qsTr("Mute all notifications")
                onClicked: handleOnlineStatusSelection(NC.UserStatus.DoNotDisturb)

                Layout.fillWidth: true
            }
            UserStatusSelectorButton {
                checked: userStatusSelectorModel.onlineStatus === NC.UserStatus.Invisible ||
                         userStatusSelectorModel.onlineStatus === NC.UserStatus.Offline
                checkable: true
                icon.source: userStatusSelectorModel.invisibleIcon
                icon.color: "transparent"
                text: qsTr("Invisible")
                secondaryText: qsTr("Appear offline")
                onClicked: handleOnlineStatusSelection(NC.UserStatus.Invisible)

                Layout.fillWidth: true
            }
        }

        UserStatusSelectorButton {
            Layout.fillWidth: true
            visible: rootLayout.showOnlineStatusSection && !rootLayout.showStatusMessageSection
            text: qsTr("Status message")
            primary: true
            onClicked: showStatusMessageRequested()
        }
    }

    ColumnLayout {
        id: userStatusMessageLayout

        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Style.smallSpacing
        visible: rootLayout.showStatusMessageSection
        Layout.preferredHeight: visible ? implicitHeight : 0

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

            AbstractButton {
                id: fieldButton

                readonly property bool showBorder: hovered || checked || emojiDialog.visible

                Layout.preferredWidth: userStatusMessageTextField.height
                Layout.preferredHeight: userStatusMessageTextField.height

                text: userStatusSelectorModel.userStatusEmoji
                padding: 0
                z: showBorder ? 2 : 0 // Make sure highlight is seen on top of text field
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
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: emojiDialog.open()
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
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignBottom
        visible: rootLayout.showStatusMessageSection
        Layout.preferredHeight: visible ? implicitHeight : 0

        Button {
            text: qsTr("Cancel")
            onClicked: closeRequested()
        }
        Item { // Spacing
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Clear")
            onClicked: userStatusSelectorModel.clearUserStatus()
        }
        Button {
            focusPolicy: Qt.StrongFocus
            text: qsTr("Apply")
            onClicked: userStatusSelectorModel.setUserStatus()
        }
    }
}
