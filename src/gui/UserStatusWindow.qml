/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import com.nextcloud.desktopclient as NC
import Style
import "./tray"
import "./wizard/qml"

WizardStyledWindow {
    id: root

    property int userIndex: -1

    readonly property int rowSpacing: 8

    title: qsTr("Online status")
    width: Style.userStatusWindowWidth
    height: Style.userStatusWindowHeight
    minimumWidth: Style.wizardStandaloneWindowMinimumWidth
    minimumHeight: Style.userStatusWindowMinimumHeight

    NC.UserStatusSelectorModel {
        id: statusModel

        finishOnOnlineStatusSet: false
        onFinished: root.close()
    }

    readonly property bool statusLoaded: statusModel.userStatusLoaded

    Binding {
        target: statusModel
        property: "userIndex"
        value: root.userIndex
        when: root.userIndex >= 0
    }

    function setOnlineStatus(status) {
        if (!root.statusLoaded) {
            return
        }

        if (statusModel.onlineStatus !== status) {
            statusModel.onlineStatus = status
        }
    }

    function saveStatusMessage() {
        if (!root.statusLoaded) {
            return
        }

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
            if (!statusMessageField.activeFocus || !root.statusLoaded) {
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
            spacing: Style.wizardSectionSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: root.rowSpacing

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    enabled: root.statusLoaded
                    selected: root.statusLoaded && statusModel.onlineStatus === NC.userStatus.Online
                    iconSource: statusModel.onlineIcon
                    text: qsTr("Online")
                    onClicked: root.setOnlineStatus(NC.userStatus.Online)
                }

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    enabled: root.statusLoaded
                    selected: root.statusLoaded && statusModel.onlineStatus === NC.userStatus.Away
                    iconSource: statusModel.awayIcon
                    text: qsTr("Away")
                    onClicked: root.setOnlineStatus(NC.userStatus.Away)
                }

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    visible: statusModel.busyStatusSupported
                    enabled: root.statusLoaded
                    selected: root.statusLoaded && statusModel.onlineStatus === NC.userStatus.Busy
                    iconSource: statusModel.busyIcon
                    text: qsTr("Busy")
                    onClicked: root.setOnlineStatus(NC.userStatus.Busy)
                }

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    enabled: root.statusLoaded
                    selected: root.statusLoaded && statusModel.onlineStatus === NC.userStatus.DoNotDisturb
                    iconSource: statusModel.dndIcon
                    text: qsTr("Do not disturb")
                    secondaryText: qsTr("Mute all notifications")
                    onClicked: root.setOnlineStatus(NC.userStatus.DoNotDisturb)
                }

                UserStatusWindowStatusRow {
                    Layout.fillWidth: true
                    enabled: root.statusLoaded
                    selected: root.statusLoaded && (statusModel.onlineStatus === NC.userStatus.Invisible
                        || statusModel.onlineStatus === NC.userStatus.Offline
                    )
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
                    spacing: root.rowSpacing

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
                    spacing: root.rowSpacing

                    Button {
                        id: emojiButton

                        readonly property string fallbackEmoji: "😀"

                        Layout.preferredWidth: Style.wizardFooterButtonHeight
                        Layout.preferredHeight: Style.wizardFooterButtonHeight
                        padding: 0
                        enabled: root.statusLoaded
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
                            visible: emojiButton.hovered || emojiButton.activeFocus
                            color: Style.wizardRowBackground
                        }
                    }

                    WizardTextField {
                        id: statusMessageField

                        Layout.fillWidth: true
                        Layout.preferredHeight: Style.wizardFooterButtonHeight
                        placeholderText: qsTr("What is your status?")
                        enabled: root.statusLoaded
                        selectByMouse: true
                        Component.onCompleted: text = statusModel.userStatusMessage
                        onEditingFinished: {
                            if (root.statusLoaded) {
                                statusModel.userStatusMessage = text
                            }
                        }
                    }
                }

                Popup {
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
                        border.width: Style.normalBorderWidth
                        border.color: Style.wizardFieldBorder
                        radius: Style.mediumRoundedButtonRadius
                    }

                    EmojiPicker {
                        width: emojiPopup.availableWidth
                        height: emojiPopup.availableHeight
                        showSearch: true
                        visibleRows: 10

                        onChosen: {
                            if (root.statusLoaded) {
                                statusModel.userStatusEmoji = emoji
                            }
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
                        spacing: Style.extraSmallSpacing

                        Repeater {
                            model: statusModel.predefinedStatuses

                            delegate: UserStatusWindowPredefinedStatusRow {
                                width: parent.width
                                emoji: modelData.icon
                                statusText: modelData.message
                                clearAtText: statusModel.clearAtReadable(modelData)
                                selected: statusModel.userStatusMessage === modelData.message
                                    && statusModel.userStatusEmoji === modelData.icon
                                enabled: root.statusLoaded
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
                    spacing: root.rowSpacing

                    EnforcedPlainTextLabel {
                        Layout.preferredWidth: implicitWidth
                        Layout.preferredHeight: Style.wizardFooterButtonHeight
                        text: qsTr("Clear status after")
                        color: Style.wizardPrimaryText
                        font.pixelSize: Style.pixelSize + 3
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.Wrap
                    }

                    ComboBox {
                        id: clearAtComboBox

                        Layout.fillWidth: true
                        Layout.preferredHeight: Style.wizardFooterButtonHeight
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
                        enabled: root.statusLoaded
                        onActivated: statusModel.setClearAt(currentValue)

                        contentItem: Text {
                            text: clearAtComboBox.displayText
                            font: clearAtComboBox.font
                            color: Style.wizardPrimaryText
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        background: Rectangle {
                            radius: Style.mediumRoundedButtonRadius
                            color: Style.wizardFieldBackground
                            border.width: Style.normalBorderWidth
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
                enabled: root.statusLoaded
                onClicked: statusModel.clearUserStatus()
            },

            Item {
                Layout.fillWidth: true
            },

            WizardButton {
                primary: true
                text: qsTr("Set status message")
                enabled: root.statusLoaded
                onClicked: root.saveStatusMessage()
            }
        ]
    }
}
