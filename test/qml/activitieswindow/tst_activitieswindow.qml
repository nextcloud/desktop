/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest
import QtQuick.Window

import "../../../src/gui/activity/qml"

Item {
    id: testRoot

    width: 200
    height: 100

    Window {
        id: focusStealingWindow

        width: 100
        height: 100
        flags: Qt.Tool
    }

    QtObject {
        id: fakeActivityModel

        readonly property int count: 40
        readonly property int maxActionButtons: 3
        readonly property bool hasSyncConflicts: false
        readonly property var allConflicts: []

        signal interactiveActivityReceived()
    }

    QtObject {
        id: fakeSyncStatusModel

        readonly property url syncIcon: ""
        readonly property string syncStatusString: ""
        readonly property string syncStatusDetailString: ""
        readonly property bool syncing: false
        readonly property int totalFiles: 0
        readonly property real syncProgress: 0
        readonly property bool needsSandboxReapproval: false
    }

    Component {
        id: activitiesWindowComponent

        ActivitiesWindow {
            activityModel: fakeActivityModel
            syncStatusModel: fakeSyncStatusModel
        }
    }

    TestCase {
        id: testCase

        name: "ActivitiesWindow"
        when: windowShown

        property ActivitiesWindow activitiesWindow
        property var activityList
        property var activityListView
        property var activityHighlight
        property var newActivitiesButtonLoader

        function init()
        {
            activitiesWindow = createTemporaryObject(activitiesWindowComponent, testRoot);
            verify(activitiesWindow);

            activityList = findChild(activitiesWindow, "activityList");
            activityListView = findChild(activitiesWindow, "activityListView");
            newActivitiesButtonLoader = findChild(activitiesWindow, "newActivitiesButtonLoader");
            verify(activityList);
            verify(activityListView);
            verify(newActivitiesButtonLoader);

            activitiesWindow.show();
            tryCompare(activitiesWindow, "visible", true);
            tryVerify(() => activityListView.contentHeight > activityListView.height);
        }

        function cleanup()
        {
            focusStealingWindow.hide();
        }

        function positionAtEnd()
        {
            activityListView.currentIndex = activityListView.count - 1;
            activityListView.positionViewAtEnd();
            compare(activityListView.currentIndex, activityListView.count - 1);
            tryCompare(activityList, "atYBeginning", false);
        }

        function test_reopeningResetsViewport()
        {
            positionAtEnd();

            activitiesWindow.hide();
            tryCompare(activitiesWindow, "visible", false);
            activitiesWindow.show();
            tryCompare(activitiesWindow, "visible", true);
            compare(activityListView.currentIndex, -1);
            tryCompare(activityList, "atYBeginning", true);
        }

        function test_activationResetsViewport()
        {
            positionAtEnd();

            focusStealingWindow.show();
            tryCompare(focusStealingWindow, "visible", true);
            tryCompare(focusStealingWindow, "active", true);

            if (activitiesWindow.active) {
                skip("The test platform does not transfer activation between windows");
                return;
            }

            tryCompare(activitiesWindow, "active", false);

            activitiesWindow.requestActivate();
            tryCompare(activitiesWindow, "active", true);
            compare(activityListView.currentIndex, -1);
            tryCompare(activityList, "atYBeginning", true);
        }

        function test_highlightIsHiddenWithoutCurrentItem()
        {
            activityListView.currentIndex = 0;
            tryVerify(() => activityListView.currentItem !== null);
            activityHighlight = activityListView.highlightItem;
            verify(activityHighlight);

            activitiesWindow.requestActivate();
            activityListView.forceActiveFocus();
            tryVerify(() => activityListView.activeFocus);

            activitiesWindow.resetActivityList();

            compare(activityListView.currentItem, null);
            compare(activityHighlight.visible, false);
        }

        function test_liveActivityDoesNotResetViewport()
        {
            positionAtEnd();

            fakeActivityModel.interactiveActivityReceived();

            tryCompare(activityListView, "atYEnd", true);
            tryCompare(activityList, "atYBeginning", false);
            tryCompare(newActivitiesButtonLoader, "active", true);
        }
    }
}
