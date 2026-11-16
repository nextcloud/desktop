/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import Style

Text {
    id: root
    objectName: "assistantConversationTitle"

    required property string fullTitle
    readonly property string singleLineTitle: root.fullTitle.replace(/\s+/g, " ").trim()
    readonly property var titleCharacters: Array.from(root.singleLineTitle)

    text: root.titleCharacters.length > Style.assistantConversationTitleMaximumLength
        ? root.titleCharacters.slice(0, Style.assistantConversationTitleMaximumLength - 1).join("").trim() + "…"
        : root.singleLineTitle
    textFormat: Text.PlainText
    wrapMode: Text.NoWrap
    maximumLineCount: 1
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
}
