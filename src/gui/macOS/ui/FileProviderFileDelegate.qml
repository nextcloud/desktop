/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Style 1.0
import "../../filedetails"
import "../../tray"

import com.nextcloud.desktopclient 1.0

Item {
    id: root

    signal evictItem(string identifier, string domainIdentifier)

    // Match with model rolenames for automagic setting of properties
    required property string identifier
    required property string domainIdentifier
    required property string fileName
    required property string userVisiblePath
    required property string fileTypeString

    required property string fileSizeString

    RowLayout {
        id: internalLayout

        anchors.fill: parent

        Image {
            id: fileIconImage
            Layout.fillHeight: true
            verticalAlignment: Image.AlignVCenter
            horizontalAlignment: Image.AlignHCenter
            source: "image://tray-image-provider/:/fileicon/" + root.userVisiblePath
            sourceSize.width: Style.trayListItemIconSize
            sourceSize.height: Style.trayListItemIconSize
            fillMode: Image.PreserveAspectFit
        }

        Column {
            Layout.fillWidth: true

            EnforcedPlainTextLabel {
                id: fileNameLabel
                width: parent.width
                text: root.fileName
            }

            EnforcedPlainTextLabel {
                id: pathLabel
                width: parent.width
                text: root.userVisiblePath
                elide: Text.ElideLeft
            }

            Row {
                width: parent.width
                spacing: Style.smallSpacing

                EnforcedPlainTextLabel {
                    id: fileSizeLabel
                    text: root.fileSizeString
                    font.bold: true
                }

                EnforcedPlainTextLabel {
                    id: fileTypeLabel
                    text: root.fileTypeString
                    color: Style.ncSecondaryTextColor
                }
            }
        }

        CustomButton {
            id: deleteButton

            Layout.minimumWidth: implicitWidth
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

            text: qsTr("Delete")
            textColorHovered: Style.ncHeaderTextColor
            bgColor: Style.errorBoxBackgroundColor
            contentsFont.bold: true
            onClicked: root.evictItem(root.identifier, root.domainIdentifier)
        }
    }
}
