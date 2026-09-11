/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest

import "qrc:/qml/src/gui" as Gui
import "qrc:/qml/src/gui/filedetails" as FileDetails

Item {
    id: testRoot

    width: 200
    height: 100

    Component {
        id: wizardWindowComponent

        Gui.WizardStyledWindow {
            width: 100
            height: 100
        }
    }

    Component {
        id: fileDetailsWindowComponent

        FileDetails.FileDetailsWindow {
            accountState: ({})
            localPath: ""
        }
    }


    TestCase {
        name: "WindowClose"
        when: windowShown

        property var windowUnderTest: null

        function closeWindow(component) {
            windowUnderTest = component.createObject(null, {visible: true})
            verify(windowUnderTest !== null)
            windowUnderTest.requestActivate()
            verify(windowCloseTestHelper.closeShortcut(windowUnderTest))
            tryCompare(windowUnderTest, "visible", false)
        }

        function cleanup() {
            if (windowUnderTest) {
                windowUnderTest.close()
                windowUnderTest.destroy()
                windowUnderTest = null
            }
        }

        function test_sharedWizardWindowClosesWithCloseShortcut() {
            closeWindow(wizardWindowComponent)
        }

        function test_fileDetailsWindowClosesWithCloseShortcut() {
            closeWindow(fileDetailsWindowComponent)
        }

    }
}
