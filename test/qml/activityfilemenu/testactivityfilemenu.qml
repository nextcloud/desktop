/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest
import "../../../src/gui/activity/qml"

TestCase {
    id: testCase

    name: "ActivityFileMenu"
    when: windowShown

    property ActivityFileMenu menu

    SignalSpy {
        id: fileDetailsRequestedSpy
        signalName: "fileDetailsRequested"
    }

    SignalSpy {
        id: fileActionsRequestedSpy
        signalName: "fileActionsRequested"
    }

    Component {
        id: menuComponent

        ActivityFileMenu {
            filePath: "/sync/folder/file.txt"
            serverHasIntegration: true
            itemFontPixelSize: 14
        }
    }

    function init()
    {
        menu = createTemporaryObject(menuComponent, testCase);
        verify(menu);

        fileDetailsRequestedSpy.target = menu;
        fileActionsRequestedSpy.target = menu;
    }

    function test_usesFullImplicitHeight()
    {
        compare(menu.height, menu.implicitHeight);
        verify(menu.height > 0);
    }

    function test_fileDetailsItemEmitsCapturedPath()
    {
        menu.popup();
        tryCompare(menu, "opened", true);

        const fileDetailsItem = menu.itemAt(0);
        verify(fileDetailsItem);
        mouseClick(fileDetailsItem);

        compare(fileDetailsRequestedSpy.count, 1);
        compare(fileDetailsRequestedSpy.signalArguments[0][0], "/sync/folder/file.txt");
    }

    function test_fileActionsItemEmitsCapturedPath()
    {
        menu.popup();
        tryCompare(menu, "opened", true);

        const fileActionsItem = menu.itemAt(1);
        verify(fileActionsItem);
        mouseClick(fileActionsItem);

        compare(fileActionsRequestedSpy.count, 1);
        compare(fileActionsRequestedSpy.signalArguments[0][0], "/sync/folder/file.txt");
    }
}
