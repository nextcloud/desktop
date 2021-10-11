import QtQuick 2.15

Item {
    id: errorBox
    
    property var text: ""

    property string color: "white"
    property string backgroundColor: "red"
    property string borderColor: "black"
    
    implicitHeight: errorMessage.implicitHeight + 2 * 8

    Rectangle {
        anchors.fill: parent
        color: errorBox.backgroundColor
        border.color: errorBox.borderColor
    }

    Text {
        id: errorMessage
        
        anchors.fill: parent
        anchors.margins: 8
        width: parent.width
        color: errorBox.color
        wrapMode: Text.WordWrap
        text: errorBox.text
    }
}
