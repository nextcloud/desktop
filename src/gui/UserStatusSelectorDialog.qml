import QtQuick.Window 2.15

import com.nextcloud.desktopclient 1.0 as NC

Window {
    id: dialog
    
    property NC.UserStatusSelectorModel model: NC.UserStatusSelectorModel {
        onFinished: {
            dialog.close()
        }
    }

    width: view.implicitWidth
    height: view.implicitHeight
    minimumWidth: view.implicitWidth
    minimumHeight: view.implicitHeight
    maximumWidth: view.implicitWidth
    maximumHeight: view.implicitHeight

    visible: true

    flags: Qt.Dialog
    
    UserStatusSelector {
        id: view
        userStatusSelectorModel: model
    }
}
