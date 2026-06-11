/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as BasicControls
import QtQuick.Layouts

import com.nextcloud.desktopclient as NC
import Style
import "./tray"
import "./wizard/qml"

ApplicationWindow {
    id: root

    property int userIndex: -1

    readonly property int sectionSpacing: 12
    readonly property int rowSpacing: 8
    readonly property int contentWidth: 560

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    title: qsTr("Online status")
    width: contentWidth
    height: 700
    minimumWidth: 520
    minimumHeight: 560
    flags: Qt.Window
        | Qt.CustomizeWindowHint
        | Qt.WindowTitleHint
        | Qt.WindowSystemMenuHint
        | Qt.WindowCloseButtonHint
    color: Style.wizardWindowBackground
    palette.window: Style.wizardWindowBackground
    palette.windowText: Style.wizardPrimaryText
    palette.base: Style.wizardFieldBackground
    palette.text: Style.wizardPrimaryText
    palette.button: Style.wizardFieldBackground
    palette.buttonText: Style.wizardPrimaryText
    palette.mid: Style.wizardDisabledText
    palette.placeholderText: Style.wizardPlaceholderText

    background: Rectangle {
        color: Style.wizardWindowBackground
    }

    NC.UserStatusSelectorModel {
        id: statusModel

        finishOnOnlineStatusSet: false
        onFinished: root.close()
    }

    Binding {
        target: statusModel
        property: "userIndex"
        value: root.userIndex
        when: root.userIndex >= 0
    }

    function setOnlineStatus(status) {
        if (statusModel.onlineStatus !== status) {
            statusModel.onlineStatus = status
        }
    }

    function saveStatusMessage() {
        if (statusModel.userStatusMessage !== statusMessageField.text) {
            statusModel.userStatusMessage = statusMessageField.text
        }
        statusModel.setUserStatus()
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.close()
    }

    Connections {
        target: statusModel

        function onUserStatusChanged() {
            if (!statusMessageField.activeFocus) {
                statusMessageField.text = statusModel.userStatusMessage
            }
        }
    }

    WizardDialogFrame {
        id: frame

        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: frame.windowMargin
            spacing: root.sectionSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: root.rowSpacing

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    selected: statusModel.onlineStatus === NC.userStatus.Online
                    iconSource: statusModel.onlineIcon
                    text: qsTr("Online")
                    onClicked: root.setOnlineStatus(NC.userStatus.Online)
                }

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    selected: statusModel.onlineStatus === NC.userStatus.Away
                    iconSource: statusModel.awayIcon
                    text: qsTr("Away")
                    onClicked: root.setOnlineStatus(NC.userStatus.Away)
                }

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    visible: statusModel.busyStatusSupported
                    selected: statusModel.onlineStatus === NC.userStatus.Busy
                    iconSource: statusModel.busyIcon
                    text: qsTr("Busy")
                    onClicked: root.setOnlineStatus(NC.userStatus.Busy)
                }

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    selected: statusModel.onlineStatus === NC.userStatus.DoNotDisturb
                    iconSource: statusModel.dndIcon
                    text: qsTr("Do not disturb")
                    secondaryText: qsTr("Mute all notifications")
                    onClicked: root.setOnlineStatus(NC.userStatus.DoNotDisturb)
                }

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    selected: statusModel.onlineStatus === NC.userStatus.Invisible
                        || statusModel.onlineStatus === NC.userStatus.Offline
                    iconSource: statusModel.invisibleIcon
                    text: qsTr("Invisible")
                    secondaryText: qsTr("Appear offline")
                    onClicked: root.setOnlineStatus(NC.userStatus.Invisible)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: root.rowSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    EnforcedPlainTextLabel {
                        Layout.fillWidth: true
                        text: qsTr("Status message")
                        color: Style.wizardPrimaryText
                        font.pixelSize: Style.pixelSize + 6
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    BasicControls.Button {
                        id: emojiButton

                        readonly property string fallbackEmoji: "😀"

                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        padding: 0
                        text: statusModel.userStatusEmoji.length > 0
                            ? statusModel.userStatusEmoji
                            : fallbackEmoji
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Choose emoji")
                        onClicked: emojiPopup.open()

                        contentItem: Text {
                            text: emojiButton.text
                            opacity: statusModel.userStatusEmoji.length > 0 ? 1.0 : 0.7
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: Style.pixelSize + 6
                        }

                        background: Rectangle {
                            color: emojiButton.hovered ? Style.wizardRowBackground : "transparent"
                        }
                    }

                    WizardTextField {
                        id: statusMessageField

                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        placeholderText: qsTr("What is your status?")
                        selectByMouse: true
                        Component.onCompleted: text = statusModel.userStatusMessage
                        onEditingFinished: statusModel.userStatusMessage = text
                    }
                }

                BasicControls.Popup {
                    id: emojiPopup

                    width: 420
                    height: 360
                    padding: 0
                    margins: 0
                    modal: false
                    clip: true
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                    x: Math.round((root.width - width) / 2)
                    y: Math.round((root.height - height) / 2)

                    background: Rectangle {
                        color: Style.wizardWindowBackground
                        border.width: 1
                        border.color: Style.wizardFieldBorder
                        radius: 8
                    }

                    EmojiPicker {
                        width: emojiPopup.availableWidth
                        height: emojiPopup.availableHeight
                        showSearch: true
                        visibleRows: 10

                        onChosen: {
                            statusModel.userStatusEmoji = emoji
                            emojiPopup.close()
                        }
                    }
                }

                ScrollView {
                    id: predefinedStatusesScrollView

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    Column {
                        width: predefinedStatusesScrollView.availableWidth
                        spacing: 2

                        Repeater {
                            model: statusModel.predefinedStatuses

                            delegate: UserStatusWindowPredefinedStatusRow {
                                width: parent.width
                                emoji: modelData.icon
                                statusText: modelData.message
                                clearAtText: statusModel.clearAtReadable(modelData)
                                selected: statusModel.userStatusMessage === modelData.message
                                    && statusModel.userStatusEmoji === modelData.icon
                                onClicked: {
                                    statusModel.setPredefinedStatus(modelData)
                                    statusMessageField.text = statusModel.userStatusMessage
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    EnforcedPlainTextLabel {
                        Layout.preferredWidth: implicitWidth
                        Layout.preferredHeight: 36
                        text: qsTr("Clear status after")
                        color: Style.wizardPrimaryText
                        font.pixelSize: Style.pixelSize + 3
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.Wrap
                    }

                    BasicControls.ComboBox {
                        id: clearAtComboBox

                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        leftPadding: 12
                        rightPadding: 32
                        topPadding: 0
                        bottomPadding: 0
                        font.pixelSize: Style.pixelSize + 3
                        model: statusModel.clearStageTypes
                        textRole: "display"
                        valueRole: "clearStageType"
                        displayText: statusModel.clearAtDisplayString
                        Accessible.name: qsTr("Clear status after")
                        onActivated: statusModel.setClearAt(currentValue)

                        contentItem: Text {
                            text: clearAtComboBox.displayText
                            font: clearAtComboBox.font
                            color: Style.wizardPrimaryText
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        background: Rectangle {
                            radius: 8
                            color: Style.wizardFieldBackground
                            border.width: 1
                            border.color: clearAtComboBox.activeFocus ? Style.ncBlue : Style.wizardFieldBorder
                        }
                    }
                }

                ErrorBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    visible: statusModel.errorMessage !== ""
                    text: statusModel.errorMessage
                }
            }
        }

        footer: [
            WizardButton {
                text: qsTr("Clear status message")
                onClicked: statusModel.clearUserStatus()
            },

            Item {
                Layout.fillWidth: true
            },

            WizardButton {
                primary: true
                text: qsTr("Set status message")
                onClicked: root.saveStatusMessage()
            }
        ]
    }
}
