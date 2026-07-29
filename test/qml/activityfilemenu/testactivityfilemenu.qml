/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest
import "../../../src/gui/activity/qml"

Item {
    id: testRoot

    width: 200
    height: 100

    TestCase {
        id: testCase

        name: "ActivityFileMenu"
        when: windowShown

        property ActivityFileMenuButton button

        SignalSpy {
            id: fileDetailsRequestedSpy
            signalName: "fileDetailsRequested"
        }

        SignalSpy {
            id: fileActionsRequestedSpy
            signalName: "fileActionsRequested"
        }

        Component {
            id: buttonComponent

            ActivityFileMenuButton {
                filePath: "/sync/folder/file.txt"
                serverHasIntegration: true
                itemFontPixelSize: 14
                buttonWidth: 44
                buttonHeight: 32
                buttonIconSize: 16
            }
        }

        function init()
        {
            button = createTemporaryObject(buttonComponent, testRoot);
            verify(button);

            fileDetailsRequestedSpy.target = button;
            fileActionsRequestedSpy.target = button;
        }

        function test_contentFitsWithoutScrolling()
        {
            button.menu.popup();
            tryCompare(button.menu, "opened", true);
            compare(button.menu.count, 2);
            const lastItem = button.menu.itemAt(button.menu.count - 1);
            verify(lastItem);
            verify(button.menu.availableHeight >= lastItem.y + lastItem.height);
        }

        function openMenu()
        {
            mouseClick(button);
            tryCompare(button.menu, "opened", true);
        }

        function test_buttonOpensMenu()
        {
            verify(!button.menu.opened);
            openMenu();
            verify(button.menu.opened);
        }

        function test_fileDetailsItemEmitsCapturedPath()
        {
            openMenu();

            const fileDetailsItem = button.menu.itemAt(0);
            verify(fileDetailsItem);
            mouseClick(fileDetailsItem);

            compare(fileDetailsRequestedSpy.count, 1);
            compare(fileDetailsRequestedSpy.signalArguments[0][0], "/sync/folder/file.txt");
        }

        function test_fileActionsItemEmitsCapturedPath()
        {
            openMenu();

            const fileActionsItem = button.menu.itemAt(1);
            verify(fileActionsItem);
            mouseClick(fileActionsItem);

            compare(fileActionsRequestedSpy.count, 1);
            compare(fileActionsRequestedSpy.signalArguments[0][0], "/sync/folder/file.txt");
        }
    }
}
