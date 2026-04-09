/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.ionos.hidrivenext.desktopclient
import Style

ApplicationWindow {
    id: root

    property var accountState
    property string localPath: ""

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
