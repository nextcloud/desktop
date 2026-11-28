/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts

import Style

ColumnLayout {
    id: root

    default property alias contents: body.data
    property alias footer: footerLayout.data
    property bool footerVisible: true
    property bool footerSeparatorVisible: false
    property int footerTopPadding: 0
    readonly property int windowMargin: Style.wizardWindowMargin
    readonly property int footerButtonHeight: Style.wizardFooterButtonHeight

    spacing: 0

    Item {
        id: body

        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    RowLayout {
        id: footerLayout

        Layout.fillWidth: true
        Layout.preferredHeight: root.footerVisible
            ? root.footerButtonHeight + root.windowMargin + root.footerTopPadding + (root.footerSeparatorVisible ? Style.normalBorderWidth : 0)
            : 0
        Layout.leftMargin: root.windowMargin
        Layout.rightMargin: root.windowMargin
        Layout.topMargin: root.footerTopPadding + (root.footerSeparatorVisible ? Style.normalBorderWidth : 0)
        Layout.bottomMargin: root.windowMargin

        visible: root.footerVisible

        spacing: Style.wizardFooterSpacing
    }
}
