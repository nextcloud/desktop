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

    function recipientNames(): string {
        const names = []
        for (const recipient of root.share.recipients) {
            if (recipient.displayName) {
                names.push(recipient.displayName)
            }
        }
        return names.length > 0 ? names.join(", ") : qsTr("No recipients")
    }

    contentItem: RowLayout {
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Style.extraSmallSpacing

            Label {
                Layout.fillWidth: true

                text: root.recipientNames()
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true

                text: root.share.permissionPreset
                color: palette.placeholderText
                elide: Text.ElideRight
                visible: text.length > 0
            }
        }

        Label {
            text: "›"
            font.pixelSize: Style.largeIconSize
            color: palette.placeholderText
        }
    }
}
