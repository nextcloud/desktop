/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls

import Style
import com.nextcloud.desktopclient
import "qrc:/qml/src/gui/wizard/qml"

WizardMenu {
    id: root

    readonly property bool showAccounts: UserModel.count > 1

    // Emitted when this window is replaced by another one, for example after switching account.
    signal closeWindowRequested()

    function switchToAccount(userIndex) {
        if (userIndex === UserModel.currentUserId) {
            return
        }

        UserModel.currentUserId = userIndex
        Systray.showActivitiesWindow(userIndex)
        root.closeWindowRequested()
    }

    function toggleUnder(item) {
        if (opened) {
            close()
            return
        }

        // Keep the menu inside the window by aligning it with the edge the
        // account area sits at instead of dropping it below its left side.
        const menuX = item.LayoutMirroring.enabled ? 0 : item.width - width
        popup(item, menuX, item.height + Style.smallSpacing)
    }

    objectName: "activitiesApplicationMenu"
    width: Style.activitiesWindowMenuWidth

    Instantiator {
        model: root.showAccounts ? UserModel : null

        delegate: WizardMenuItem {
            objectName: "activitiesAccountMenuItem"
            // UserModel does not report IsCurrentUserRole changes to its views,
            // so the current account is derived from currentUserId instead.
            text: (model.index === UserModel.currentUserId ? "✓ " : "")
                + "%1 (%2)".arg(model.name).arg(model.server)
            onTriggered: root.switchToAccount(model.index)
        }

        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }

    BasicControls.MenuSeparator {
        objectName: "activitiesAccountsSeparator"
        height: visible ? implicitHeight : 0
        visible: root.showAccounts
    }

    WizardMenuItem {
        objectName: "activitiesAddAccountMenuItem"
        height: visible ? implicitHeight : 0
        visible: Systray.enableAddAccount
        text: qsTr("Add account")
        onTriggered: UserModel.addAccount()
    }

    WizardMenuItem {
        objectName: "activitiesPauseSyncMenuItem"
        height: visible ? implicitHeight : 0
        visible: Systray.canPauseSync
        text: qsTr("Pause sync for all")
        onTriggered: Systray.setSyncIsPaused(true)
    }

    WizardMenuItem {
        objectName: "activitiesResumeSyncMenuItem"
        height: visible ? implicitHeight : 0
        visible: Systray.canResumeSync
        text: qsTr("Resume sync for all")
        onTriggered: Systray.setSyncIsPaused(false)
    }

    WizardMenuItem {
        objectName: "activitiesSettingsMenuItem"
        text: qsTr("Settings")
        onTriggered: Systray.openSettings()
    }

    WizardMenuItem {
        objectName: "activitiesQuitMenuItem"
        text: qsTr("Quit")
        onTriggered: Systray.shutdown()
    }
}
