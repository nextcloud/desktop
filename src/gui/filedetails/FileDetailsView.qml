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

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import com.nextcloud.desktopclient 1.0
import Style 1.0

StackView {
    id: root

    signal closeButtonClicked

    property alias accountState: fileDetailsPage.accountState
    property alias localPath: fileDetailsPage.localPath
    property alias showCloseButton: fileDetailsPage.showCloseButton
    property alias accentColor: fileDetailsPage.accentColor
    property bool backgroundsVisible: true

    background: Rectangle {
        color: palette.window
        visible: root.backgroundsVisible
    }

    initialItem: FileDetailsPage {
        id: fileDetailsPage
        width: parent.width
        height: parent.height
        backgroundsVisible: root.backgroundsVisible
        rootStackView: root
        onCloseButtonClicked: root.closeButtonClicked()
    }
}
