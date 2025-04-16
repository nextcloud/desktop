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

    property int userIndex: -1
    property NC.UserStatusSelectorModel model: NC.UserStatusSelectorModel {
        userIndex: page.userIndex
        onFinished: page.finished()
    }

    padding: Style.standardSpacing * 2

    background: Rectangle {
        color: palette.base
        radius: Style.trayWindowRadius
    }
    
    contentItem: UserStatusSelector {
        id: userStatusSelector
        userStatusSelectorModel: model
        spacing: Style.standardSpacing
    }
}
