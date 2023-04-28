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

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtQml.Models 2.15
import Style 1.0
import com.nextcloud.desktopclient 1.0
import "./tray"

Window {
    id: root

    flags: Qt.Dialog
    visible: true

    width: 600
    height: 800
    minimumWidth: 600
    minimumHeight: 800
    title: qsTr('Solve sync conflicts')

    onClosing: function() {
        Systray.destroyDialog(root);
    }

    Component.onCompleted: {
        Systray.forceWindowInit(root);
        Systray.positionNotificationWindow(root);

        root.show();
        root.raise();
        root.requestActivate();
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
            text: qsTr("%1 files in conflict").arg(12)
            font.bold: true
            font.pixelSize: 20
            Layout.fillWidth: true
        }

        EnforcedPlainTextLabel {
            text: qsTr("Choose if you want to keep the local version, server version, or both? If you choose both, the local file will have a number added to its name.")
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            font.pixelSize: 15
            Layout.fillWidth: true
            Layout.topMargin: -15
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 15

            CheckBox {
                id: selectExisting

                Layout.fillWidth: true

                text: qsTr('All local versions')

                leftPadding: 0
                implicitWidth: 100

                font.pixelSize: 15
            }

            CheckBox {
                id: selectConflict

                Layout.fillWidth: true

                text: qsTr('All server versions')

                leftPadding: 0
                implicitWidth: 100

                font.pixelSize: 15
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 5
            Layout.rightMargin: 5
            color: Style.menuBorder
            height: 1
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ListView {
                id: conflictListView

                model: DelegateModel {
                    model: ListModel {
                        ListElement {
                            existingFileName: 'Text File.txt'
                            conflictFileName: 'Text File.txt'
                            existingSize: '2 B'
                            conflictSize: '15 B'
                            existingDate: '28 avril 2023 09:53'
                            conflictDate: '28 avril 2023 09:53'
                            existingSelected: false
                            conflictSelected: false
                            existingPreviewUrl: 'https://nextcloud.local/index.php/apps/theming/img/core/filetypes/text.svg?v=b9feb2d6'
                            conflictPreviewUrl: 'https://nextcloud.local/index.php/apps/theming/img/core/filetypes/text.svg?v=b9feb2d6'
                        }

                        ListElement {
                            existingFileName: 'Text File.txt'
                            conflictFileName: 'Text File.txt'
                            existingSize: '2 B'
                            conflictSize: '15 B'
                            existingDate: '28 avril 2023 09:53'
                            conflictDate: '28 avril 2023 09:53'
                            existingSelected: false
                            conflictSelected: false
                            existingPreviewUrl: 'https://nextcloud.local/index.php/apps/theming/img/core/filetypes/text.svg?v=b9feb2d6'
                            conflictPreviewUrl: 'https://nextcloud.local/index.php/apps/theming/img/core/filetypes/text.svg?v=b9feb2d6'
                        }

                        ListElement {
                            existingFileName: 'Text File.txt'
                            conflictFileName: 'Text File.txt'
                            existingSize: '2 B'
                            conflictSize: '15 B'
                            existingDate: '28 avril 2023 09:53'
                            conflictDate: '28 avril 2023 09:53'
                            existingSelected: false
                            conflictSelected: false
                            existingPreviewUrl: 'https://nextcloud.local/index.php/apps/theming/img/core/filetypes/text.svg?v=b9feb2d6'
                            conflictPreviewUrl: 'https://nextcloud.local/index.php/apps/theming/img/core/filetypes/text.svg?v=b9feb2d6'
                        }
                    }

                    delegate: ConflictDelegate {
                        width: conflictListView.contentItem.width
                        height: 100
                    }
                }
            }
        }

        DialogButtonBox {
            Layout.fillWidth: true

            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel

            onAccepted: function() {
                console.log("Ok clicked")
                Systray.destroyDialog(root)
            }

            onRejected: function() {
                console.log("Cancel clicked")
                Systray.destroyDialog(root)
            }
        }
    }

    Rectangle {
        color: Theme.systemPalette.window
        anchors.fill: parent
        z: 1
    }
}
