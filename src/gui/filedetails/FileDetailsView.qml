// SPDX-FileCopyrightText: 2022 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

StackView {
    id: root

    signal closeButtonClicked

    property alias accountState: fileDetailsPage.accountState
    property alias localPath: fileDetailsPage.localPath
    property alias showCloseButton: fileDetailsPage.showCloseButton
    property alias accentColor: fileDetailsPage.accentColor
    property bool backgroundsVisible: true

    background: Rectangle {
        color: palette.base
        visible: root.backgroundsVisible
    }

    initialItem: FileDetailsPage {
        id: fileDetailsPage
        width: root.width
        height: root.height
        backgroundsVisible: root.backgroundsVisible
        rootStackView: root
        onCloseButtonClicked: root.closeButtonClicked()
    }
}
