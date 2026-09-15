/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml

// Test replacement for the production C++ type, which SyncStatus instantiates even with an injected model.
QtObject {
    readonly property url syncIcon: ""
    readonly property string syncStatusString: ""
    readonly property string syncStatusDetailString: ""
    readonly property bool syncing: false
    readonly property int totalFiles: 0
    readonly property real syncProgress: 0
    readonly property bool needsSandboxReapproval: false
}
