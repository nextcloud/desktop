/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Effects

Item {
    id: root

    property url source

    Rectangle {
        id: maskShape
        objectName: "recipientAvatarMaskShape"

        anchors.fill: parent
        radius: width / 2
        color: "black"
        visible: false
    }

    Image {
        id: avatarImage

        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
        visible: false
    }

    MultiEffect {
        objectName: "recipientAvatarEffect"

        anchors.fill: parent
        source: avatarImage
        maskEnabled: true
        maskSource: maskShape
    }
}
