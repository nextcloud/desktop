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
            tryCompare(activityListView, "count", fakeActivityModel.count);
            activityListView.forceLayout();
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

        function wheelTarget()
        {
            verify(waitForPolish(activitiesWindow), "Activities window layout did not settle");
            const x = activityListView.width / 2;
            const y = activityListView.height / 2;
            tryVerify(() => activityListView.indexAt(activityListView.contentX + x, activityListView.contentY + y) >= 0);
            return Qt.point(x, y);
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
            tryVerify(() => activityListView.highlightItem !== null);

            activitiesWindow.requestActivate();
            activityListView.forceActiveFocus();
            tryVerify(() => activityListView.activeFocus);

            activitiesWindow.resetActivityList();

            compare(activityListView.currentItem, null);
            tryVerify(() => activityListView.highlightItem === null || !activityListView.highlightItem.visible);
        }

        function test_mouseWheelScrollsActivityList()
        {
            const target = wheelTarget();
            tryCompare(activityListView, "atYBeginning", true);

            const startY = activityListView.contentY;
            mouseWheel(activityListView, target.x, target.y, 0, -120);
            tryVerify(() => activityListView.contentY > startY);
        }

        function test_mouseWheelCancelsScrollToTopAtEnd()
        {
            // Activating the window resets the activity list to the beginning.
            activitiesWindow.requestActivate();
            tryCompare(activitiesWindow, "active", true);
            verify(waitForPolish(activitiesWindow), "Activities window layout did not settle");
            activityListView.positionViewAtEnd();
            const target = wheelTarget();
            tryCompare(activityListView, "atYEnd", true);

            activityList.scrollToTop();
            compare(activityList.scrollingToTop, true);
            mouseWheel(activityListView, target.x, target.y, 0, -120);
            compare(activityList.scrollingToTop, false);
            tryCompare(activityListView, "atYEnd", true);
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
