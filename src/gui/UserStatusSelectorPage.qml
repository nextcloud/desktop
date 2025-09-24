/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import Style

import com.nextcloud.desktopclient as NC

Page {
    id: page

    signal finished
    signal openStatusMessageRequested

    property NC.UserStatusSelectorModel userStatusSelectorModel
    property string pageType: "status"

    readonly property bool showStatusButtons: pageType !== "message"
    readonly property bool showStatusMessageControls: pageType !== "status"
    readonly property bool showStatusMessageNavigation: pageType === "status"

    padding: Style.standardSpacing * 2
    implicitContentWidth: contentContainer ? contentContainer.implicitWidth : 0
    implicitContentHeight: contentContainer ? contentContainer.implicitHeight : 0

    background: Rectangle {
        color: palette.base
        radius: Style.trayWindowRadius
    }

    contentItem: Item {
        id: contentContainer

        implicitWidth: userStatusSelector.implicitWidth
        implicitHeight: userStatusSelector.implicitHeight
        width: parent ? parent.width : implicitWidth
        height: implicitHeight

        UserStatusSelector {
            id: userStatusSelector
            anchors.left: parent.left
            anchors.right: parent.right
            userStatusSelectorModel: page.userStatusSelectorModel
            showStatusButtons: page.showStatusButtons
            showStatusMessageControls: page.showStatusMessageControls
            showStatusMessageNavigation: page.showStatusMessageNavigation
            onFinished: page.finished()
            onOpenStatusMessageRequested: page.openStatusMessageRequested()
        }
    }

    Connections {
        target: page.userStatusSelectorModel
        enabled: !!target
        ignoreUnknownSignals: true

        function onFinished() {
            page.finished();
        }
    }
}
