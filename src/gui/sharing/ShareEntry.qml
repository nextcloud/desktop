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
        return names.join(", ")
    }

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

            EnforcedPlainTextLabel {
                Layout.fillWidth: true

                text: {
                    const recipients = root.recipientNames()
                    return recipients.length > 0
                        ? qsTr("Shared with %1").arg(recipients)
                        : qsTr("Share without recipients")
                }
                elide: Text.ElideRight
            }
        }

    }
}
