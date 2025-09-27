/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient as NC
import Style

import "./tray"

ColumnLayout {
    id: root

    property NC.UserStatusSelectorModel userStatusSelectorModel

    signal finished
    signal showStatusMessageRequested

    spacing: Style.smallSpacing
    Layout.fillWidth: true

    function handleStatusClick(status) {
        if (!userStatusSelectorModel) {
            return;
        }

        if (userStatusSelectorModel.onlineStatus === status) {
            finished();
        } else {
            userStatusSelectorModel.onlineStatus = status;
        }
    }

    EnforcedPlainTextLabel {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        font.bold: true
        text: qsTr("Online status")
        Layout.bottomMargin: Style.smallSpacing
    }

    UserStatusSelectorButton {
        checked: userStatusSelectorModel && userStatusSelectorModel.onlineStatus === NC.UserStatus.Online
        checkable: true
        icon.source: userStatusSelectorModel ? userStatusSelectorModel.onlineIcon : ""
        icon.color: "transparent"
        text: qsTr("Online")
        Layout.fillWidth: true
        onClicked: handleStatusClick(NC.UserStatus.Online)
    }

    UserStatusSelectorButton {
        checked: userStatusSelectorModel && userStatusSelectorModel.onlineStatus === NC.UserStatus.Away
        checkable: true
        icon.source: userStatusSelectorModel ? userStatusSelectorModel.awayIcon : ""
        icon.color: "transparent"
        text: qsTr("Away")
        Layout.fillWidth: true
        onClicked: handleStatusClick(NC.UserStatus.Away)
    }

    UserStatusSelectorButton {
        visible: userStatusSelectorModel && userStatusSelectorModel.busyStatusSupported
        checked: userStatusSelectorModel && userStatusSelectorModel.onlineStatus === NC.UserStatus.Busy
        checkable: true
        icon.source: userStatusSelectorModel ? userStatusSelectorModel.busyIcon : ""
        icon.color: "transparent"
        text: qsTr("Busy")
        Layout.fillWidth: true
        onClicked: handleStatusClick(NC.UserStatus.Busy)
    }

    UserStatusSelectorButton {
        checked: userStatusSelectorModel && userStatusSelectorModel.onlineStatus === NC.UserStatus.DoNotDisturb
        checkable: true
        icon.source: userStatusSelectorModel ? userStatusSelectorModel.dndIcon : ""
        icon.color: "transparent"
        text: qsTr("Do not disturb")
        secondaryText: qsTr("Mute all notifications")
        Layout.fillWidth: true
        onClicked: handleStatusClick(NC.UserStatus.DoNotDisturb)
    }

    UserStatusSelectorButton {
        checked: userStatusSelectorModel && (userStatusSelectorModel.onlineStatus === NC.UserStatus.Invisible
            || userStatusSelectorModel.onlineStatus === NC.UserStatus.Offline)
        checkable: true
        icon.source: userStatusSelectorModel ? userStatusSelectorModel.invisibleIcon : ""
        icon.color: "transparent"
        text: qsTr("Invisible")
        secondaryText: qsTr("Appear offline")
        Layout.fillWidth: true
        onClicked: handleStatusClick(NC.UserStatus.Invisible)
    }

    Item {
        Layout.fillHeight: true
    }

    Button {
        Layout.fillWidth: true
        text: qsTr("Status message")
        onClicked: showStatusMessageRequested()
    }
}
