/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest

import com.nextcloud.desktopclient
import "qrc:/qml/src/gui"
import "qrc:/qml/src/gui/activity/qml"

Item {
    id: testRoot

    width: 400
    height: 300

    Component {
        id: menuComponent

        ActivitiesWindowMenu {
            anchorItem: testRoot
        }
    }

    Component {
        id: accountHeaderComponent

        WindowAccountHeader {
            width: testRoot.width
            title: "Activities"
            user: ({ name: "alice", server: "localhost:8080", avatar: "" })
            accountMenuEnabled: true
        }
    }

    Component {
        id: inertAccountHeaderComponent

        WindowAccountHeader {
            width: testRoot.width
            title: "Activities"
            user: ({ name: "alice", server: "localhost:8080", avatar: "" })
            accountMenuEnabled: false
        }
    }

    Component {
        id: longNameHeaderComponent

        WindowAccountHeader {
            width: testRoot.width
            title: "Activities"
            accountMenuEnabled: true
            user: ({
                name: "a-very-long-account-display-name-that-should-be-elided",
                server: "an-equally-long-server-host-name.example.com",
                avatar: ""
            })
        }
    }

    TestCase {
        id: testCase

        name: "ActivitiesWindowMenu"
        when: windowShown

        property ActivitiesWindowMenu menu

        SignalSpy {
            id: closeWindowRequestedSpy
            signalName: "closeWindowRequested"
        }

        SignalSpy {
            id: accountMenuRequestedSpy
            signalName: "accountMenuRequested"
        }

        SignalSpy {
            id: openSettingsSpy
            target: Systray
            signalName: "openSettings"
        }

        SignalSpy {
            id: shutdownSpy
            target: Systray
            signalName: "shutdown"
        }

        SignalSpy {
            id: addAccountSpy
            target: UserModel
            signalName: "addAccount"
        }

        function accountItems()
        {
            const items = []
            for (let index = 0; index < menu.count; ++index) {
                const item = menu.itemAt(index)
                if (item && item.objectName === "activitiesAccountMenuItem") {
                    items.push(item)
                }
            }
            return items
        }

        function menuItem(objectName)
        {
            for (let index = 0; index < menu.count; ++index) {
                const item = menu.itemAt(index)
                if (item && item.objectName === objectName) {
                    return item
                }
            }
            return null
        }

        function init()
        {
            Systray.reset()
            UserModel.reset(2)

            menu = createTemporaryObject(menuComponent, testRoot)
            verify(menu)

            // Menu entries only report their effective visibility while the menu is shown.
            menu.open()
            tryCompare(menu, "opened", true)

            closeWindowRequestedSpy.target = menu
            closeWindowRequestedSpy.clear()
            openSettingsSpy.clear()
            shutdownSpy.clear()
            addAccountSpy.clear()
            accountMenuRequestedSpy.clear()
        }

        function test_accountsAreListedWithSeveralAccounts()
        {
            compare(accountItems().length, 2)
        }

        function test_checkmarkFollowsTheCurrentAccount()
        {
            const items = accountItems()
            compare(items.length, 2)

            verify(items[0].text.startsWith("✓ "))
            verify(!items[1].text.startsWith("✓ "))

            // UserModel does not notify views when the current user changes, so the
            // mark has to follow currentUserId rather than the isCurrentUser role.
            UserModel.currentUserId = 1

            verify(!items[0].text.startsWith("✓ "))
            verify(items[1].text.startsWith("✓ "))
        }

        function test_singleAccountHidesAccountEntries()
        {
            UserModel.reset(1)
            compare(accountItems().length, 0)
        }

        function test_selectingAnotherAccountOpensItsActivitiesWindow()
        {
            const items = accountItems()
            compare(items.length, 2)

            items[1].triggered()

            compare(UserModel.currentUserId, 1)
            compare(Systray.activitiesWindowUserIndex, 1)
            compare(closeWindowRequestedSpy.count, 1)
        }

        function test_selectingTheCurrentAccountKeepsTheWindow()
        {
            const items = accountItems()
            items[0].triggered()

            compare(Systray.activitiesWindowUserIndex, -1)
            compare(closeWindowRequestedSpy.count, 0)
        }

        function test_settingsAndQuitUseTheApplicationPaths()
        {
            menuItem("activitiesSettingsMenuItem").triggered()
            compare(openSettingsSpy.count, 1)

            menuItem("activitiesQuitMenuItem").triggered()
            compare(shutdownSpy.count, 1)
        }

        function test_addAccountFollowsSystray()
        {
            const addAccountItem = menuItem("activitiesAddAccountMenuItem")
            verify(addAccountItem.visible)

            Systray.enableAddAccount = false
            verify(!addAccountItem.visible)

            Systray.enableAddAccount = true
            verify(addAccountItem.visible)

            addAccountItem.triggered()
            compare(addAccountSpy.count, 1)
        }

        function test_syncControlItemsFollowSyncControlState()
        {
            const pauseItem = menuItem("activitiesPauseSyncMenuItem")
            const resumeItem = menuItem("activitiesResumeSyncMenuItem")

            // Unavailable: no classic sync folders are configured.
            verify(!pauseItem.visible)
            verify(!resumeItem.visible)

            // Pause: all folders are running.
            Systray.canPauseSync = true
            verify(pauseItem.visible)
            verify(!resumeItem.visible)

            // Resume: all folders are paused.
            Systray.canPauseSync = false
            Systray.canResumeSync = true
            verify(!pauseItem.visible)
            verify(resumeItem.visible)

            // PauseAndResume: some folders are paused and others are running.
            Systray.canPauseSync = true
            verify(pauseItem.visible)
            verify(resumeItem.visible)
        }

        function test_syncControlItemsPauseAndResumeAllFolders()
        {
            Systray.canPauseSync = true
            menuItem("activitiesPauseSyncMenuItem").triggered()
            compare(Systray.setSyncIsPausedCallCount, 1)
            compare(Systray.syncIsPaused, true)

            Systray.canResumeSync = true
            menuItem("activitiesResumeSyncMenuItem").triggered()
            compare(Systray.setSyncIsPausedCallCount, 2)
            compare(Systray.syncIsPaused, false)
        }

        function test_accountAreaRequestsTheMenu()
        {
            const header = createTemporaryObject(accountHeaderComponent, testRoot)
            verify(header)
            accountMenuRequestedSpy.target = header

            const caret = findChild(header, "windowAccountHeaderCaret")
            const headerButton = findChild(header, "windowAccountHeaderMenuButton")
            verify(caret.visible)
            verify(headerButton.enabled)

            headerButton.clicked()
            compare(accountMenuRequestedSpy.count, 1)
        }

        function test_longAccountNamesStayWithinTheHeader()
        {
            const header = createTemporaryObject(longNameHeaderComponent, testRoot)
            verify(header)

            // The account block, including the caret, keeps to its share of the header
            // so that a long name cannot push it over the title.
            const accountRow = findChild(header, "windowAccountHeaderAccountRow")
            verify(accountRow.width <= Math.round(header.width * 0.55) + 1)
            verify(findChild(header, "windowAccountHeaderCaret").visible)
        }

        function test_accountAreaIsInertWhenTheMenuIsDisabled()
        {
            const header = createTemporaryObject(inertAccountHeaderComponent, testRoot)
            verify(header)

            verify(!findChild(header, "windowAccountHeaderCaret").visible)
            verify(!findChild(header, "windowAccountHeaderMenuButton").enabled)
        }
    }
}
