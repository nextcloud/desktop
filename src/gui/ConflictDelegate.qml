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
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Style 1.0
import com.nextcloud.desktopclient 1.0
import "./tray"

Item {
    id: root

    required property string existingFileName
    required property string existingSize
    required property string conflictSize
    required property string existingDate
    required property string conflictDate
    required property bool existingSelected
    required property bool conflictSelected
    required property url existingPreviewUrl
    required property url conflictPreviewUrl

    EnforcedPlainTextLabel {
        id: existingFileNameLabel

        anchors.top: parent.top
        anchors.left: parent.left

        text: root.existingFileName

        font.weight: Font.Bold
        font.pixelSize: 15
    }

    RowLayout {
        anchors.top: existingFileNameLabel.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottomMargin: 8

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            CheckBox {
                id: selectExisting

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter

                leftPadding: 0
                spacing: 0

                checked: root.existingSelected
            }

            Image {
                id: existingPreview

                anchors.left: selectExisting.right
                anchors.verticalCenter: parent.verticalCenter

                source: root.existingPreviewUrl
                width: 48
                height: 48
                sourceSize.width: 48
                sourceSize.height: 48
            }

            ColumnLayout {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: existingPreview.right
                anchors.right: parent.right
                anchors.leftMargin: 10

                spacing: 0

                Item {
                    Layout.fillHeight: true
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true

                    text: qsTr('Local version')

                    font.pixelSize: 15
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true

                    text: root.existingDate

                    font.pixelSize: 15
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true

                    text: existingSize

                    font.pixelSize: 15
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            CheckBox {
                id: selectConflict

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 0

                leftPadding: 0
                spacing: 0

                checked: root.conflictSelected
            }

            Image {
                id: conflictPreview

                anchors.left: selectConflict.right
                anchors.verticalCenter: parent.verticalCenter

                source: root.conflictPreviewUrl
                width: 48
                height: 48
                sourceSize.width: 48
                sourceSize.height: 48
            }

            ColumnLayout {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: conflictPreview.right
                anchors.right: parent.right
                anchors.leftMargin: 10

                spacing: 0

                Item {
                    Layout.fillHeight: true
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true

                    text: qsTr('Server version')

                    font.pixelSize: 15
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true

                    text: root.conflictDate

                    font.pixelSize: 15
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true

                    text: conflictSize

                    font.pixelSize: 15
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
