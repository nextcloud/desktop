/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml.Models

ListModel {
    id: root

    property var sourceModel: null

    function populate()
    {
        clear();
        if (!sourceModel) {
            return;
        }

        for (let index = 0; index < sourceModel.count; ++index) {
            append({
                activity: {},
                activityIndex: index,
                conversationToken: "",
                dateTime: "",
                displayActions: false,
                displayLocation: "",
                displayPath: "",
                icon: "image://svgimage-custom-color/activity.svg",
                isCurrentUserFileActivity: false,
                isDismissable: false,
                linksContextMenu: [],
                linksForActionButtons: [],
                message: "",
                messageId: "",
                messageSent: "",
                objectType: "",
                openablePath: "",
                path: "",
                serverHasIntegration: false,
                showFileDetails: false,
                subject: "Activity " + index,
                type: "Notification"
            });
        }
    }

    onSourceModelChanged: populate()
}
