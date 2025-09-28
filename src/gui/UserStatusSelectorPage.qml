/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style

import com.nextcloud.desktopclient as NC

Page {
    id: page

    signal finished

    property int userIndex: -1
    property string mode: setStatusMode
    property NC.UserStatusSelectorModel model: NC.UserStatusSelectorModel {
        userIndex: page.userIndex
        onFinished: page.finished()
    }

    readonly property string setStatusMode: "setStatus"
    readonly property string statusMessageMode: "statusMessage"

    padding: Style.standardSpacing * 2

    background: Rectangle {
        color: palette.base
        radius: Style.trayWindowRadius
    }

    contentItem: StackLayout {
        currentIndex: page.mode === page.statusMessageMode ? 1 : 0

        UserStatusSetStatusView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            userStatusSelectorModel: model
            onFinished: page.finished()
            onShowStatusMessageRequested: page.mode = page.statusMessageMode
        }

        UserStatusMessageView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            userStatusSelectorModel: model
            onFinished: page.finished()
        }
    }
}
