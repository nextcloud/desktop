/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

Page {
    id: root

    required property SharingController sharingController
    property Share share

    title: qsTr("Sharing settings")
    implicitWidth: 360
    implicitHeight: Math.min(settingsList.contentHeight, 320)

    ListView {
        id: settingsList

        anchors.fill: parent
        clip: true
        spacing: Style.standardSpacing

        model: PropertyModel {
            share: root.share
        }

        delegate: FieldDelegate {
            required property var model

            width: settingsList.width
        }

        ScrollBar.vertical: ScrollBar {}
    }

    Label {
        anchors.centerIn: parent
        width: parent.width

        text: qsTr("No additional sharing settings are available.")
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        visible: settingsList.count === 0
    }
}
