/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
