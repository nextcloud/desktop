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
    property bool footerVisible: true
    property bool footerSeparatorVisible: false
    property int footerTopPadding: 0
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
            Layout.preferredHeight: root.footerVisible
                ? root.footerButtonHeight + root.windowMargin + root.footerTopPadding + (root.footerSeparatorVisible ? Style.normalBorderWidth : 0)
                : 0
            visible: root.footerVisible

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: Style.normalBorderWidth
                color: Style.wizardRowBorder
                visible: root.footerSeparatorVisible
            }

            RowLayout {
                id: footerLayout
                anchors.fill: parent
                anchors.leftMargin: root.windowMargin
                anchors.rightMargin: root.windowMargin
                anchors.topMargin: root.footerTopPadding + (root.footerSeparatorVisible ? Style.normalBorderWidth : 0)
                anchors.bottomMargin: root.windowMargin
                spacing: Style.wizardFooterSpacing
            }
        }
    }
}
