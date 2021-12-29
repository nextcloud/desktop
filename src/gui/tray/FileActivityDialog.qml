import QtQuick.Window 2.12

import com.nextcloud.desktopclient 1.0 as NC

Window {
    id: dialog

    property alias model: activityModel

    NC.FileActivityListModel {
        id: activityModel
    }   

    width: 500
    height: 500

    ActivityList {
        anchors.fill: parent
        model: dialog.model
    }
}
