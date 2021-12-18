import QtQuick 2.15
import QtQuick.Controls 2.15

import Style 1.0

import com.nextcloud.desktopclient 1.0 as NC

ScrollView {
    id: controlRoot
    property alias model: activityList.model

    signal showFileActivity(string displayPath, string absolutePath)
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

        delegate: ActivityItem {
            width: activityList.contentWidth
            height: Style.trayWindowHeaderHeight
            flickable: activityList
            onClicked: activityItemClicked(model.index)
            onFileActivityButtonClicked: showFileActivity(displayPath, absolutePath)
        }
    }
}
