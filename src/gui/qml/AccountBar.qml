/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.ownCloud.gui 1.0
import org.ownCloud.libsync 1.0

Pane {
    id: bar
    Accessible.name: qsTr("Navigation bar")

    RowLayout {
        anchors.fill: parent

        Repeater {
            id: accountButtons

            model: AccountManager.accounts

            delegate: AccountButton {
                property AccountState accountState: modelData

                Layout.fillWidth: true
                Layout.maximumWidth: widthHint
                Accessible.role: Accessible.PageTab
                checked: settingsDialog.currentAccount === accountState.account
                icon.source: "image://ownCloud/core/account"
                text: accountState.account.displayName.replace("@", "\n")

                Keys.onBacktabPressed: event => {
                    if (index === 0) {
                        // We're the first button, handle the back-tab
                        settingsDialog.focusPrevious();
                    } else {
                        event.accepted = false;
                    }
                }
                onClicked: {
                    settingsDialog.currentAccount = accountState.account;
                }
            }
        }
        AccountButton {
            id: addAccountButton

            Layout.fillWidth: true
            Layout.maximumWidth: widthHint
            icon.source: "image://ownCloud/core/plus-solid"
            text: qsTr("Add Account")
            visible: Theme.multiAccount | !AccountManager.accounts

            Keys.onBacktabPressed: event => {
                // If there are no account buttons, we're the first button, so handle the back-tab
                if (accountButtons.count === 0) {
                    settingsDialog.focusPrevious();
                } else {
                    event.accepted = false;
                }
            }
            onClicked: {
                settingsDialog.addAccount();
            }
        }
        Item {
            // spacer
            Layout.fillWidth: true
        }
        AccountButton {
            id: logButton

            Layout.fillWidth: true
            Layout.maximumWidth: widthHint
            Accessible.role: Accessible.PageTab
            checked: settingsDialog.currentPage === SettingsDialog.Activity
            icon.source: "image://ownCloud/core/activity"
            text: qsTr("Activity")

            onClicked: {
                settingsDialog.currentPage = SettingsDialog.Activity;
            }
        }
        AccountButton {
            id: settingsButton

            Layout.fillWidth: true
            Layout.maximumWidth: widthHint
            Accessible.role: Accessible.PageTab
            checked: settingsDialog.currentPage === SettingsDialog.Settings
            icon.source: "image://ownCloud/core/settings"
            text: qsTr("Settings")

            onClicked: {
                settingsDialog.currentPage = SettingsDialog.Settings;
            }
        }
        Repeater {
            // branded buttons with an url
            model: Theme.urlButtons

            delegate: AccountButton {
                property UrlButtonData urlButton: modelData

                Layout.fillWidth: true
                Layout.maximumWidth: widthHint
                icon.source: urlButton.icon
                text: urlButton.name

                onClicked: {
                    Qt.openUrlExternally(urlButton.url);
                }
            }
        }
        AccountButton {
            id: quitButton

            Layout.fillWidth: true
            Layout.maximumWidth: widthHint
            icon.source: "image://ownCloud/core/quit"
            text: qsTr("Quit")

            Keys.onTabPressed: {
                settingsDialog.focusNext();
            }
            onClicked: {
                Qt.quit();
            }
        }
    }
}
