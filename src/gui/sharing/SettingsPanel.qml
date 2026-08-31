/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick

import Style

Rectangle {
    id: root

    default property alias contents: content.data

    color: Style.wizardRowBackground
    radius: Style.mediumRoundedButtonRadius
    clip: true

    Item {
        id: content

        anchors.fill: parent
    }
}
