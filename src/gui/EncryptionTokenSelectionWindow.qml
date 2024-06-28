/*
 * Copyright (C) 2023 by Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtQml.Models 2.15

import com.nextcloud.desktopclient 1.0
import Style 1.0

import "./tray"

ApplicationWindow {
    id: encryptionKeyChooserDialog

    required property var certificatesInfo
    required property ClientSideEncryptionTokenSelector certificateSelector
    property string selectedSerialNumber: ''

    flags: Qt.Window | Qt.Dialog
    visible: true
    modality: Qt.ApplicationModal

    width: 400
    height: 600
    minimumWidth: 400
    minimumHeight: 600

    title: qsTr('Token Encryption Key Chooser')

    // TODO: Rather than setting all these palette colours manually,
    // create a custom style and do it for all components globally
    palette {
        text: Style.ncTextColor
        windowText: Style.ncTextColor
        buttonText: Style.ncTextColor
        brightText: Style.ncTextBrightColor
        highlight: Style.lightHover
        highlightedText: Style.ncTextColor
        light: Style.lightHover
        midlight: Style.ncSecondaryTextColor
        mid: Style.darkerHover
        dark: Style.menuBorder
        button: Style.buttonBackgroundColor
        window: Style.backgroundColor
        base: Style.backgroundColor
        toolTipBase: Style.backgroundColor
        toolTipText: Style.ncTextColor
    }

    onClosing: function(close) {
        Systray.destroyDialog(self);
        close.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        anchors.topMargin: 20
        spacing: 15
        z: 2

        EnforcedPlainTextLabel {
            text: qsTr("Available Keys for end-to-end Encryption:")
            font.bold: true
            font.pixelSize: Style.bigFontPixelSizeResolveConflictsDialog
            Layout.fillWidth: true
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            clip: true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ListView {
                id: tokensListView

                currentIndex: -1

                model: DelegateModel {
                    model: certificatesInfo

                    delegate: ItemDelegate {
                        width: tokensListView.contentItem.width

                        text: modelData.subject

                        highlighted: tokensListView.currentIndex === index

                        onClicked: function()
                        {
                            tokensListView.currentIndex = index
                            selectedSerialNumber = modelData.serialNumber
                        }
                    }
                }
            }
        }

        DialogButtonBox {
            Layout.fillWidth: true

            Button {
                text: qsTr("Choose")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            onAccepted: function() {
                Systray.destroyDialog(encryptionKeyChooserDialog)
                certificateSelector.serialNumber = selectedSerialNumber
            }

            onRejected: function() {
                Systray.destroyDialog(encryptionKeyChooserDialog)
                certificateSelector.serialNumber = ''
            }
        }
    }

    Rectangle {
        color: Style.backgroundColor
        anchors.fill: parent
        z: 1
    }
}
