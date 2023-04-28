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
    required property string conflictFileName
    required property string existingSize
    required property string conflictSize
    required property string existingDate
    required property string conflictDate
    required property bool existingSelected
    required property bool conflictSelected

    EnforcedPlainTextLabel {
        id: existingFileNameLabel

        anchors.top: parent.top
        anchors.left: parent.left

        text: root.existingFileName

        font.weight: Font.Light
        font.pixelSize: 15
    }

    RowLayout {
        anchors.top: existingFileNameLabel.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Image {
                id: existingPreview

                anchors.top: parent.top
                anchors.left: parent.left

                source: 'https://nextcloud.local/index.php/apps/theming/img/core/filetypes/text.svg?v=b9feb2d6'
                width: 64
                height: 64
                sourceSize.width: 64
                sourceSize.height: 64
            }

            ColumnLayout {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: existingPreview.right
                anchors.right: parent.right

                CheckBox {
                    id: selectExisting

                    Layout.alignment: Layout.TopLeft

                    checked: root.existingSelected
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
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Image {
                id: conflictPreview

                anchors.top: parent.top
                anchors.left: parent.left

                source: 'https://nextcloud.local/index.php/apps/theming/img/core/filetypes/text.svg?v=b9feb2d6'
                width: 64
                height: 64
                sourceSize.width: 64
                sourceSize.height: 64
            }

            ColumnLayout {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: conflictPreview.right
                anchors.right: parent.right

                CheckBox {
                    id: selectConflict

                    Layout.alignment: Layout.TopLeft

                    checked: root.conflictSelected
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
            }
        }
    }
}
