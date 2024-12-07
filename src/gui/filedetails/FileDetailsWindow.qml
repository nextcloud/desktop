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
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

ApplicationWindow {
    id: root

    property var accountState
    property string localPath: ""

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    width: 400
    height: 500
    minimumWidth: 300
    minimumHeight: 300

    title: qsTr("File details of %1 · %2").arg(fileDetailsPage.fileDetails.name).arg(Systray.windowTitle)

    FileDetailsView {
        id: fileDetailsPage
        anchors.fill: parent
        accountState: root.accountState
        localPath: root.localPath
    }
}
