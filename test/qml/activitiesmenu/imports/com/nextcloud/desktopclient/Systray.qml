/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma Singleton

import QtQuick

QtObject {
    id: root

    property bool enableAddAccount: true
    property bool canPauseSync: false
    property bool canResumeSync: false

    property int activitiesWindowUserIndex: -1
    property int setSyncIsPausedCallCount: 0
    property bool syncIsPaused: false

    signal openSettings()
    signal shutdown()

    function setSyncIsPaused(paused) {
        root.syncIsPaused = paused
        root.setSyncIsPausedCallCount += 1
    }

    function showActivitiesWindow(userIndex) {
        root.activitiesWindowUserIndex = userIndex
    }

    function reset() {
        root.enableAddAccount = true
        root.canPauseSync = false
        root.canResumeSync = false
        root.activitiesWindowUserIndex = -1
        root.setSyncIsPausedCallCount = 0
        root.syncIsPaused = false
    }
}
