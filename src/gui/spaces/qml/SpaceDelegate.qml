/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: spaceDelegate
    property alias title: title.text
    property alias description: description.text
    property alias descriptionWrapMode: description.wrapMode
    property alias imageSource: image.source
    property alias statusSource: statusIcon.source

    default property alias content: colLayout.children

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true

            Pane {
                Accessible.ignored: true
                Layout.preferredHeight: normalSize - 20
                Layout.preferredWidth: normalSize - 20
                Layout.alignment: Qt.AlignTop
                background: Rectangle {
                    color: spaceDelegate.palette.alternateBase
                }
                Image {
                    id: image
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    sourceSize.width: width
                    sourceSize.height: height
                }
            }
            ColumnLayout {
                id: colLayout
                spacing: 6
                Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                Layout.fillWidth: true

                RowLayout {
                    Layout.fillWidth: true
                    Image {
                        id: statusIcon
                        Layout.preferredHeight: 16
                        Layout.preferredWidth: 16
                        visible: statusSource
                        sourceSize.width: width
                        sourceSize.height: height
                    }
                    Label {
                        id: title
                        Accessible.ignored: true
                        Layout.fillWidth: true
                        font.bold: true
                        font.pointSize: 15
                        elide: Text.ElideRight
                    }
                }
                Label {
                    id: description
                    Accessible.ignored: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }
    }
}
