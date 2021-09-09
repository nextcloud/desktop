import QtQuick 2.15

Item {
    id: errorBox
    
    property var text: ""
    
    implicitHeight: errorMessage.implicitHeight + 2 * 8

    Rectangle {
        anchors.fill: parent
        color: "red"
        border.color: "black"
    }

    Text {
        id: errorMessage
        
        anchors.fill: parent
        anchors.margins: 8
        width: parent.width
        color: "white"
        wrapMode: Text.WordWrap
        text: errorBox.text
    }
}
