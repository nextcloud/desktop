/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

ApplicationWindow {
    id: root
    width: 400
    height: 500
    minimumWidth: 300
    minimumHeight: 300
    LayoutMirroring.childrenInherit: true
    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft

    property var accountState: ({})
    property string localPath: ""

    title: qsTr("Declarative UI for %1").arg(root.localPath)

    Component {
        id: declarativeUiDelegate

        Item {
            id: declarativeUiItem
            width: parent.width
            height: 40

            required property string name
            required property string type
            required property string label
            required property string url
            required property string text

            Row {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 5
                height: implicitHeight

                Text {
                    text: declarativeUiItem.text
                    color: Style.accentColor
                    font.pixelSize: Style.pixelSize
                    verticalAlignment: Text.AlignVCenter
                    visible: declarativeUiItem.name == "Text"
                }

                Image {
                    source: declarativeUiItem.url
                    width: 50
                    height: 50
                    verticalAlignment: Text.AlignVCenter
                    visible: declarativeUiItem.name == "Image"
                }

                Button {
                    text: declarativeUiItem.label
                    width: 120
                    height: 30
                    visible: declarativeUiItem.name == "Button"
                }
            }
        }
    }

    DeclarativeUi {
        id: declarativeUi
        accountState: root.accountState
        localPath: root.localPath
    }

    ListView {
        id: declarativeUiView
        model: declarativeUi.declarativeUiModel
        delegate: declarativeUiDelegate

        anchors.fill: parent
        anchors.margins: 10
    }


}
