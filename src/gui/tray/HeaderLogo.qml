import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as NativeDialogs


// Custom qml modules are in /theme (and included by resources.qrc)
import Style

import com.ionos.hidrivenext.desktopclient

Rectangle {
    Image{
            id: trayWindowLogo
            cache: false
            source: Style.sesIonosLogoIcon
            sourceSize: Qt.size(width, height)
            fillMode: Image.PreserveAspectFit
            anchors{
                top: parent.top
                left: parent.left
                bottom: parent.bottom
                topMargin: Style.sesHeaderLogoTopMargin
                leftMargin: Style.sesHeaderLogoTopMargin
                bottomMargin: Style.sesHeaderLogoTopMargin

            }
    }

    color: Style.sesSelectedColor
}