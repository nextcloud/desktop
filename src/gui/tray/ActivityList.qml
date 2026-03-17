/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls

import Style
import com.nextcloud.desktopclient as NC

ScrollView {
    id: controlRoot
    property alias model: sortedActivityList.sourceModel
    property alias count: activityList.count
    property alias atYBeginning : activityList.atYBeginning
    property bool isFileActivityList: false
    property int iconSize: Style.trayListItemIconSize
    property int delegateHorizontalPadding: 0

    property bool scrollingToTop: false

    function scrollToTop() {
        // Triggers activation of repeating upward flick timer
        scrollingToTop = true
    }

    signal openFile(string filePath)
    signal activityItemClicked(int index)

    contentWidth: availableWidth
    leftPadding: 0
    topPadding: 0
    bottomPadding: 0
    rightPadding: ScrollBar.vertical.width
    
    focus: false

    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AlwaysOn
    ScrollBar.vertical.width: Math.max(ScrollBar.vertical.implicitWidth, Style.minimumScrollBarWidth)
    ScrollBar.vertical.minimumSize: Style.minimumScrollBarThumbSize

    data: NC.WheelHandler {
        target: controlRoot.contentItem
        onWheel: {
            scrollingToTop = false
        }
    }

    ListView {
        id: activityList

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Activity list")

        keyNavigationEnabled: true
        clip: true
        spacing: 0
        currentIndex: -1
        interactive: true

        Timer {
            id: repeatUpFlickTimer
            interval: Style.activityListScrollToTopTimerInterval
            running: controlRoot.scrollingToTop
            repeat: true
            onTriggered: {
                if (!activityList.atYBeginning) {
                    activityList.flick(0, Style.activityListScrollToTopVelocity)
                } else {
                    controlRoot.scrollingToTop = false
                }
            }
        }

        highlight: Rectangle {
            id: activityHover
            anchors.fill: activityList.currentItem
            color: palette.highlight
            radius: Style.mediumRoundedButtonRadius
            visible: activityList.activeFocus
        }

        highlightFollowsCurrentItem: true
        highlightMoveDuration: 0
        highlightResizeDuration: 0
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: 0
        preferredHighlightEnd: controlRoot.height

        model: NC.SortedActivityListModel {
            id: sortedActivityList
        }

        delegate: ActivityItem {
            background: null
            width: activityList.contentItem.width

            isFileActivityList: controlRoot.isFileActivityList
            iconSize: controlRoot.iconSize
            flickable: activityList
            onHoveredChanged: if (hovered) {
                // When we set the currentIndex the list view will scroll...
                // unless we tamper with the preferred highlight points to stop this.
                const savedPreferredHighlightBegin = activityList.preferredHighlightBegin;
                const savedPreferredHighlightEnd = activityList.preferredHighlightEnd;
                // Set overkill values to make sure no scroll happens when we hover with mouse
                activityList.preferredHighlightBegin = -controlRoot.height;
                activityList.preferredHighlightEnd = controlRoot.height * 2;

                activityList.currentIndex = index

                // Reset original values so keyboard navigation makes list view scroll
                activityList.preferredHighlightBegin = savedPreferredHighlightBegin;
                activityList.preferredHighlightEnd = savedPreferredHighlightEnd;

                forceActiveFocus();
            }
            onClicked: {
                if (model.isCurrentUserFileActivity && model.openablePath) {
                    openFile("file://" + model.openablePath);
                } else {
                    activityItemClicked(model.activityIndex)
                }
            }
        }

        Button {
            id: scrollToTopButton

            anchors.bottom: parent.bottom
            anchors.right: parent.right

            hoverEnabled: true
            padding: Style.smallSpacing

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Scroll to top")
            Accessible.onPressAction: scrollToTopButton.clicked()

            icon.source: "image://svgimage-custom-color/chevron-double-up.svg/" + palette.buttonText
            icon.width: Style.activityListButtonIconSize
            icon.height: Style.activityListButtonIconSize

            onClicked: controlRoot.scrollToTop()

            visible: !controlRoot.atYBeginning && controlRoot.contentHeight > controlRoot.height
        }

        Column {
            id: placeholderColumn
            width: parent.width * 0.8
            anchors.centerIn: parent
            visible: activityList.count === 0
            spacing: Style.standardSpacing

            Image {
                width: parent.width
                verticalAlignment: Image.AlignVCenter
                horizontalAlignment: Image.AlignHCenter
                fillMode: Image.PreserveAspectFit
                source: "image://svgimage-custom-color/activity.svg/" + palette.windowText
            }

            EnforcedPlainTextLabel {
               width: parent.width
               text: qsTr("No activities yet")
               font.bold: true
               wrapMode: Text.Wrap
               horizontalAlignment: Text.AlignHCenter
               verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
