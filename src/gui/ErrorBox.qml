import QtQuick 2.15
import QtQuick.Controls 2.15

import Style 1.0

Item {
    id: errorBox
    
    property var text: ""

    property color color: Style.errorBoxTextColor
    property color backgroundColor: Style.errorBoxBackgroundColor
    property color borderColor: Style.errorBoxBorderColor
    
    implicitHeight: errorMessage.implicitHeight + 2 * 8

    Rectangle {
        anchors.fill: parent
        color: errorBox.backgroundColor
        border.color: errorBox.borderColor
    }

    Label {
        id: errorMessage
        
        anchors.fill: parent
        anchors.margins: 8
        width: parent.width
        color: errorBox.color
        wrapMode: Text.WordWrap
        text: errorBox.text
    }
}
