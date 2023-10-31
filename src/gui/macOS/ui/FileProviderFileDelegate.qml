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
    required property string fileType

    required property size documentSize

    GridLayout {
        id: internalLayout

        anchors.fill: parent
        rows: 3
        columns: 4

        Image {
            id: fileIconImage

            Layout.fillHeight: true
            Layout.row: 0
            Layout.rowSpan: internalLayout.rows
            Layout.column: 0

            verticalAlignment: Image.AlignVCenter
            horizontalAlignment: Image.AlignHCenter
            source: "image://tray-image-provider/:/fileicon/" + root.userVisiblePath
            sourceSize.width: Style.trayListItemIconSize
            sourceSize.height: Style.trayListItemIconSize
            fillMode: Image.PreserveAspectFit
        }

        EnforcedPlainTextLabel {
            id: fileNameLabel

            Layout.fillWidth: true
            Layout.row: 0
            Layout.column: 1
            Layout.columnSpan: 2

            text: root.fileName
        }

        CustomButton {
            id: deleteButton

            Layout.preferredWidth: width
            Layout.minimumWidth: implicitWidth
            Layout.fillHeight: true
            Layout.row: 0
            Layout.rowSpan: internalLayout.rows
            Layout.column: 2

            text: qsTr("Delete")
            bgColor: Style.errorBoxBackgroundColor
            onClicked: root.evictItem(root.identifier, root.domainIdentifier)
        }

        EnforcedPlainTextLabel {
            id: pathLabel

            Layout.fillWidth: true
            Layout.row: 1
            Layout.column: 1
            Layout.columnSpan: 2

            text: root.userVisiblePath
        }

        EnforcedPlainTextLabel {
            id: fileSizeLabel

            Layout.row: 2
            Layout.column: 1

            text: root.documentSize
        }

        EnforcedPlainTextLabel {
            id: fileTypeLabel

            Layout.row: 2
            Layout.column: 2

            text: root.fileType
            color: Style.ncSecondaryTextColor
        }
    }
}
