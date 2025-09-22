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
    signal showStatusMessageRequested

    property int userIndex: -1
    property bool showOnlineStatusSection: true
    property bool showStatusMessageSection: true
    property NC.UserStatusSelectorModel model: NC.UserStatusSelectorModel {
        userIndex: page.userIndex
        onFinished: page.finished()
    }

    padding: Style.standardSpacing * 2
    implicitHeight: (userStatusSelector.implicitHeight || 0) + topPadding + bottomPadding
    implicitWidth: (userStatusSelector.implicitWidth || 0) + leftPadding + rightPadding

    background: Rectangle {
        color: palette.base
        radius: Style.trayWindowRadius
    }

    contentItem: UserStatusSelector {
        id: userStatusSelector
        userStatusSelectorModel: model
        spacing: Style.standardSpacing
        showOnlineStatusSection: page.showOnlineStatusSection
        showStatusMessageSection: page.showStatusMessageSection
        width: page.availableWidth

        onCloseRequested: page.finished()
        onShowStatusMessageRequested: page.showStatusMessageRequested()
    }
}
