import QtQuick 2.15
import QtQuick.Controls 2.15

import Style 1.0
import com.nextcloud.desktopclient 1.0 as NC
import Style 1.0

ScrollView {
    id: controlRoot
    property alias model: sortedActivityList.activityListModel

    property bool isFileActivityList: false
    property int iconSize: Style.trayListItemIconSize

    signal openFile(string filePath)
    signal activityItemClicked(int index)

    contentWidth: availableWidth
    padding: 1
    focus: false

    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    data: NC.WheelHandler {
        target: controlRoot.contentItem
    }

    ListView {
        id: activityList

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Activity list")

        clip: true
        spacing: 0
        currentIndex: -1
        interactive: true

        highlight: Rectangle {
            id: activityHover
            width: activityList.currentItem.width
            height: activityList.currentItem.height
            color: Style.lightHover
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
            activityListModel: controlRoot.model
        }

        delegate: ActivityItem {
            isFileActivityList: controlRoot.isFileActivityList
            iconSize: controlRoot.iconSize
            width: activityList.contentWidth
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
                source: "image://svgimage-custom-color/activity.svg/" + Style.ncSecondaryTextColor
            }

            Label {
               width: parent.width
               text: qsTr("No activities yet")
               color: Style.ncSecondaryTextColor
               font.bold: true
               wrapMode: Text.Wrap
               horizontalAlignment: Text.AlignHCenter
               verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
