/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls
import QtQuick.Layouts
import com.nextcloud.desktopclient
import Style
import "../../tray"

Item {
    id: root

    required property var controller
    readonly property color primaryTextColor: Style.wizardPrimaryText
    readonly property color hintTextColor: Style.wizardSecondaryText

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 24
        anchors.bottomMargin: 24
        spacing: 4

        EnforcedPlainTextLabel {
            text: qsTr("Log in to %1").arg(root.controller.appName)
            color: root.primaryTextColor
            font.pixelSize: Style.pixelSize + 8
            font.bold: true
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        EnforcedPlainTextLabel {
            text: qsTr("Enter the link to your %1 web interface from the browser or the link to a folder shared with you.").arg(root.controller.appName)
            color: root.hintTextColor
            font.pixelSize: Style.pixelSize + 2
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.topMargin: 22

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: 6
                spacing: 8

                WizardTextField {
                    id: serverUrlField
                    visible: !root.controller.overrideServerSelectionRequired
                    Layout.fillWidth: true
                    text: root.controller.serverUrl
                    enabled: !root.controller.busy
                    readOnly: !root.controller.serverUrlEditable
                    placeholderText: root.controller.serverUrlPlaceholder
                    inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                    selectByMouse: true
                    onTextEdited: root.controller.serverUrl = text
                    onAccepted: root.controller.submitServerUrl()
                }

                BasicControls.ComboBox {
                    id: serverSelector

                    visible: root.controller.overrideServerSelectionRequired
                    Layout.fillWidth: true
                    Layout.preferredHeight: Style.standardPrimaryButtonHeight
                    implicitHeight: Style.standardPrimaryButtonHeight
                    model: root.controller.overrideServerNames
                    currentIndex: root.controller.overrideServerIndex
                    enabled: !root.controller.busy
                    font.pixelSize: Style.pixelSize + 3
                    onActivated: root.controller.overrideServerIndex = currentIndex

                    leftPadding: 12
                    rightPadding: 40
                    topPadding: 0
                    bottomPadding: 0

                    contentItem: Text {
                        text: serverSelector.displayText
                        font: serverSelector.font
                        color: serverSelector.enabled ? root.primaryTextColor : Style.wizardDisabledText
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    indicator: Image {
                        x: serverSelector.width - width - 12
                        y: Math.round((serverSelector.height - height) / 2)
                        width: 16
                        height: 16
                        source: "image://svgimage-custom-color/caret-down.svg/" + root.primaryTextColor
                        rotation: serverSelector.popup.visible ? 180 : 0
                        opacity: serverSelector.enabled ? 1 : 0.45
                        fillMode: Image.PreserveAspectFit

                        Behavior on rotation {
                            NumberAnimation {
                                duration: 120
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    background: Rectangle {
                        radius: 8
                        color: Style.wizardFieldBackground
                        border.width: 1
                        border.color: serverSelector.activeFocus || serverSelector.popup.visible
                            ? Style.ncBlue
                            : Style.wizardFieldBorder
                    }

                    delegate: BasicControls.ItemDelegate {
                        id: serverDelegate

                        required property int index
                        required property string modelData

                        width: ListView.view ? ListView.view.width : serverSelector.width - 8
                        height: Style.standardPrimaryButtonHeight
                        highlighted: serverSelector.highlightedIndex === index

                        contentItem: Text {
                            text: serverDelegate.modelData
                            font: serverSelector.font
                            color: serverSelector.currentIndex === serverDelegate.index
                                ? Style.wizardSelectedText
                                : root.primaryTextColor
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        background: Rectangle {
                            radius: 6
                            color: {
                                if (serverSelector.currentIndex === serverDelegate.index) {
                                    return Style.ncBlue
                                }
                                if (serverDelegate.highlighted) {
                                    return Style.wizardSecondaryButtonBackground
                                }
                                return Style.wizardFieldBackground
                            }
                        }
                    }

                    popup: BasicControls.Popup {
                        y: serverSelector.height + 4
                        width: serverSelector.width
                        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
                        padding: 4

                        contentItem: ListView {
                            clip: true
                            implicitHeight: Math.min(contentHeight, Style.standardPrimaryButtonHeight * 6)
                            model: serverSelector.popup.visible ? serverSelector.delegateModel : null
                            currentIndex: serverSelector.highlightedIndex
                        }

                        background: Rectangle {
                            radius: 8
                            color: Style.wizardFieldBackground
                            border.width: 1
                            border.color: Style.wizardSecondaryButtonBorder
                        }
                    }
                }

                WizardButton {
                    primary: true
                    Layout.preferredWidth: 76
                    enabled: !root.controller.busy
                    text: qsTr("Log in")
                    onClicked: root.controller.submitServerUrl()
                }
            }

            Rectangle {
                x: 8
                y: 0
                width: serverAddressLabel.implicitWidth + 8
                height: serverAddressLabel.implicitHeight
                color: Style.wizardWindowBackground

                EnforcedPlainTextLabel {
                    id: serverAddressLabel
                    anchors.centerIn: parent
                    text: qsTr("Server address")
                    color: root.hintTextColor
                    font.pixelSize: Style.pixelSize
                }
            }
        }

        EnforcedPlainTextLabel {
            visible: root.controller.errorText !== ""
            text: root.controller.errorText
            color: Style.wizardErrorText
            font.pixelSize: Style.pixelSize + 1
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        RowLayout {
            visible: root.controller.busy && root.controller.authStatusText !== ""
            Layout.fillWidth: true
            spacing: 8

            NCBusyIndicator {
                running: root.controller.busy
                visible: running
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
            }

            EnforcedPlainTextLabel {
                text: root.controller.authStatusText
                color: root.hintTextColor
                font.pixelSize: Style.pixelSize + 1
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
