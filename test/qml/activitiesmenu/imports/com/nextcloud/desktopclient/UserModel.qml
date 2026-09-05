/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma Singleton

import QtQuick

ListModel {
    id: root

    property int currentUserId: 0

    signal addAccount()

    function reset(userCount) {
        root.clear()
        for (let index = 0; index < userCount; ++index) {
            root.append({
                name: "user" + index,
                server: "https://cloud" + index + ".example.com",
                isCurrentUser: index === 0
            })
        }
        root.currentUserId = 0
    }
}
