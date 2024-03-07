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

ApplicationWindow {
    id: root

    signal reloadMaterialisedItems(string accountUserIdAtHost)

    property var materialisedItemsModel: null
    property string accountUserIdAtHost: ""

    title: qsTr("Evict materialised files")
    color: Style.backgroundColor
    flags: Qt.Dialog | Qt.WindowStaysOnTopHint
    width: 640
    height: 480

    Component.onCompleted: reloadMaterialisedItems(accountUserIdAtHost)

    ColumnLayout {
        anchors.fill: parent

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Style.standardSpacing

            EnforcedPlainTextLabel {
                text: qsTr("Materialised items")
                font.bold: true
                font.pointSize: Style.headerFontPtSize
                Layout.fillWidth: true
            }

            CustomButton {
                padding: Style.smallSpacing
                textColor: Style.ncTextColor
                textColorHovered: Style.ncHeaderTextColor
                contentsFont.bold: true
                bgColor: Style.ncBlue
                text: qsTr("Reload")
                onClicked: reloadMaterialisedItems(accountUserIdAtHost)
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Layout.leftMargin: Style.standardSpacing
            Layout.rightMargin: Style.standardSpacing

            clip: true
            model: root.materialisedItemsModel
            delegate: FileProviderFileDelegate {
                width: parent.width
                height: 60
                onEvictItem: root.materialisedItemsModel.evictItem(identifier, domainIdentifier)
            }
        }
    }
}
