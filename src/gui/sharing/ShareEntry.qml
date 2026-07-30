/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

ItemDelegate {
    id: root

    required property Share share
    property bool selected: false

    highlighted: root.selected

    function recipientNames(): string {
        const names = []
        for (const recipient of root.share.recipients) {
            if (recipient.displayName) {
                names.push(recipient.displayName)
            }
        }
        return names.length > 0 ? names.join(", ") : qsTr("No recipients")

    background: Rectangle {
        color: root.selected
            ? palette.midlight
            : root.hovered
                ? palette.alternateBase
                : "transparent"
        radius: Style.smallSpacing
    }

    contentItem: RowLayout {
        ColumnLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true

                text: root.recipientNames()
                elide: Text.ElideRight
            }
        }

    }
}
