// SPDX-FileCopyrightText: 2023 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    title: qsTr("Remove local copies")
    color: palette.base
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
                text: qsTr("Local copies")
                font.bold: true
                font.pointSize: Style.headerFontPtSize
                Layout.fillWidth: true
            }

            Button {
                padding: Style.smallSpacing
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
