/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
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

    property NC.UserStatusSelectorModel userStatusSelectorModel

    signal finished

    spacing: Style.smallSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

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

            AbstractButton {
                id: fieldButton

                readonly property bool showBorder: hovered || checked || emojiDialog.visible

                Layout.preferredWidth: userStatusMessageTextField.height
                Layout.preferredHeight: userStatusMessageTextField.height

                readonly property string fallbackEmoji: "😀"

                text: userStatusSelectorModel && userStatusSelectorModel.userStatusEmoji.length > 0
                      ? userStatusSelectorModel.userStatusEmoji
                      : fallbackEmoji
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
                    opacity: userStatusSelectorModel && userStatusSelectorModel.userStatusEmoji.length > 0 ? 1.0 : 0.7
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
                        if (userStatusSelectorModel) {
                            userStatusSelectorModel.userStatusEmoji = emoji
                        }
                        emojiDialog.close()
                    }
                }
            }

            TextField {
                id: userStatusMessageTextField

                Layout.fillWidth: true
                placeholderText: qsTr("What is your status?")
                text: userStatusSelectorModel ? userStatusSelectorModel.userStatusMessage : ""
                selectByMouse: true
                onEditingFinished: {
                    if (userStatusSelectorModel) {
                        userStatusSelectorModel.userStatusMessage = text
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
                model: userStatusSelectorModel ? userStatusSelectorModel.predefinedStatuses : []
                delegate: PredefinedStatusButton {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    leftPadding: 0
                    emojiWidth: fieldButton.width
                    internalSpacing: statusFieldLayout.spacing + userStatusMessageTextField.leftPadding

                    emoji: modelData.icon
                    statusText: modelData.message
                    clearAtText: userStatusSelectorModel ? userStatusSelectorModel.clearAtReadable(modelData) : ""
                    onClicked: {
                        if (userStatusSelectorModel) {
                            userStatusSelectorModel.setPredefinedStatus(modelData)
                        }
                    }
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

                model: userStatusSelectorModel ? userStatusSelectorModel.clearStageTypes : []
                textRole: "display"
                valueRole: "clearStageType"
                displayText: userStatusSelectorModel ? userStatusSelectorModel.clearAtDisplayString : ""
                onActivated: {
                    if (userStatusSelectorModel) {
                        userStatusSelectorModel.setClearAt(currentValue)
                    }
                }
            }
        }
    }

    ErrorBox {
        width: parent.width

        visible: userStatusSelectorModel && userStatusSelectorModel.errorMessage !== ""
        text: userStatusSelectorModel ? "Error: " + userStatusSelectorModel.errorMessage : ""
    }

    RowLayout {
        id: bottomButtonBox
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignBottom

        Button {
            text: qsTr("Cancel")
            onClicked: finished()
        }
        Item {
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Clear")
            onClicked: {
                if (userStatusSelectorModel) {
                    userStatusSelectorModel.clearUserStatus()
                }
            }
        }
        Button {
            focusPolicy: Qt.StrongFocus
            text: qsTr("Apply")
            onClicked: {
                if (userStatusSelectorModel) {
                    userStatusSelectorModel.setUserStatus()
                }
            }
        }
    }
}
