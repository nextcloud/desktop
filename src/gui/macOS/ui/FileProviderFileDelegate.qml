/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

        spacing: Style.standardSpacing

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
                    color: palette.dark
                }
            }
        }

        Button {
            id: deleteButton

            Layout.minimumWidth: implicitWidth
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

            text: qsTr("Delete")
            onClicked: root.evictItem(root.identifier, root.domainIdentifier)
        }
    }
}
