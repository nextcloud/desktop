import QtQuick 2.15
import QtQuick.Controls 2.15

import com.nextcloud.desktopclient 1.0 as NC

ScrollView {
    id: controlRoot
    property alias model: activityList.model

    property bool isFileActivityList: false

    signal showFileActivity(string objectName, int objectId)
    signal activityItemClicked(int index)

    contentWidth: availableWidth
    padding: 1

    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    data: NC.WheelHandler {
        target: controlRoot.contentItem
    }

    ListView {
        id: activityList

        keyNavigationEnabled: true

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Activity list")

        clip: true

        spacing: 0

        delegate: ActivityItem {
            isFileActivityList: controlRoot.isFileActivityList
            width: activityList.contentWidth
            flickable: activityList
            onClicked: {
                if (model.isCurrentUserFileActivity) {
                    showFileActivity(model.objectName, model.objectId)
                } else {
                    activityItemClicked(model.index)
                }
            }
        }
    }
}
