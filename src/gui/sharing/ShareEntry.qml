/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
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
        return names.join(", ")
    }

    text: {
        const recipients = root.recipientNames()
        return recipients.length > 0
            ? qsTr("Shared with %1").arg(recipients)
            : qsTr("New share")
    }

    contentItem: Label {
        text: root.text
        color: root.highlighted ? root.palette.highlightedText : root.palette.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
