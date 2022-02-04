import QtQml 2.15
import QtQuick 2.15
import QtQuick.Window 2.15

import Style 1.0
import com.nextcloud.desktopclient 1.0 as NC

Window {
    id: dialog

    property alias model: activityModel

    NC.FileActivityListModel {
        id: activityModel
    }   

    width: 500
    height: 500

    Rectangle {
        id: background
        anchors.fill: parent
        color: Style.backgroundColor
    }

    ActivityList {
        isFileActivityList: true
        anchors.fill: parent
        model: dialog.model
    }

    Component.onCompleted: {
        // Set this explicitly, otherwise on macOS it will appear behind the tray
        x = screen.width / 2 - width / 2
        y = screen.height / 2 - height / 2
    }
}
