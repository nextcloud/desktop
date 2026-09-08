/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import Style
import "qrc:/qml/src/gui/tray"

ItemDelegate {
    id: root

    required property string userId
    required property string displayName
    required property string avatarUrl

    signal personChosen(string userId, string displayName, string avatarUrl)

    height: Style.unifiedSearchProviderHeaderHeight
    text: displayName
    hoverEnabled: true
    Accessible.description: userId

    background: Rectangle {
        color: root.hovered || root.down
            ? Style.listItemHoverBackground
            : "transparent"
        radius: Style.mediumRoundedButtonRadius
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    contentItem: RowLayout {
        Image {
            Layout.preferredWidth: Style.accountAvatarSize
            Layout.preferredHeight: Style.accountAvatarSize
            sourceSize.width: Style.accountAvatarSize
            sourceSize.height: Style.accountAvatarSize
            asynchronous: true
            source: root.avatarUrl.length > 0
                ? "image://tray-image-provider/" + root.avatarUrl
                : ""
            Accessible.ignored: true
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            text: root.displayName
            elide: Text.ElideRight
        }
    }

    onClicked: root.personChosen(root.userId, root.displayName, root.avatarUrl)
}
