/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

Dialog {
    id: root

    required property SharingController sharingController
    required property Share share

    title: qsTr("Sharing settings")
    anchors.centerIn: parent
    width: Math.min(parent.width - Style.standardSpacing * 2, 420)
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    standardButtons: Dialog.Close

    contentItem: SettingsPage {
        sharingController: root.sharingController
        share: root.share
    }
}
