/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Style

Pane {
    id: root

    default property alias contents: body.data
    property alias footer: footerLayout.data
    readonly property int windowMargin: Style.wizardWindowMargin
    readonly property int footerButtonHeight: Style.wizardFooterButtonHeight

    padding: 0

    background: Rectangle {
        color: "transparent"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.footerButtonHeight + root.windowMargin

            RowLayout {
                id: footerLayout
                anchors.fill: parent
                anchors.leftMargin: root.windowMargin
                anchors.rightMargin: root.windowMargin
                anchors.topMargin: 0
                anchors.bottomMargin: root.windowMargin
                spacing: Style.wizardFooterSpacing
            }
        }
    }
}
