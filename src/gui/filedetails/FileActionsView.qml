/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "../tray"
import "../"

StackView {
    id: root

    signal closeButtonClicked

    property string localPath: ""
    property var accountState: ({})
    property FileDetails fileDetails: FileDetails {}
    property int iconSize: 32
    property bool backgroundsVisible: true
    property color accentColor: Style.ncBlue
    property StackView rootStackView: StackView {}

    background: Rectangle {
        color: palette.base
        visible: root.backgroundsVisible
    }

    initialItem: FileActionsPage {
        id: fileDetailsPage
        width: root.width
        height: root.height
        backgroundsVisible: root.backgroundsVisible
        rootStackView: root
    }
}
