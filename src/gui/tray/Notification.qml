import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

// Custom qml modules are in /theme (and included by resources.qrc)
import Style 1.0

Item {
    id: notificationItem
    property alias text: notificationMessage.text
    signal dismissNotification()

    width: parent.width - Style.trayWindowBorderWidth*2
    height: Style.notificationHeight
    anchors.bottom: parent.bottom
    anchors.horizontalCenter: parent.horizontalCente

    Rectangle {
        id: roundedBackground
        width: parent.width
        height: parent.height
        anchors.bottom: parent.bottom
        color: Style.lightMessage
        radius: Style.trayWindowRadius
    }

    Rectangle {
        width: parent.width
        height: roundedBackground.radius
        anchors.top: parent.top
        color: Style.lightMessage
    }

    RowLayout {
        width: parent.width
        height: parent.height
        spacing: 4
        Layout.alignment: Qt.AlignVCenter

        Image {
            id: messageIcon
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Layout.preferredWidth: dismissButton.icon.width
            Layout.preferredHeight: dismissButton.icon.height
            verticalAlignment: Qt.AlignCenter
            cache: true
            source: "qrc:///client/theme/info.svg"
            sourceSize.height: 64
            sourceSize.width: 64
        }

        Text {
            id: notificationMessage
            anchors.left: messageIcon.right
            anchors.leftMargin: 8
            text: msgText
            font.pixelSize: Style.topLinePixelSize
            Layout.preferredWidth: Style.notificationLabelWidth
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        Button {
            id: dismissButton
            anchors.right: parent.right
            anchors.rightMargin: 4
            Layout.alignment: Qt.AlignRight
            flat: true
            hoverEnabled: true
            display: AbstractButton.IconOnly
            icon.source: "qrc:///client/theme/close.svg"
            icon.color: "transparent"
            background: Rectangle {
                color: parent.hovered ? Style.lightHover : "transparent"
            }
            ToolTip.visible: hovered
            ToolTip.delay: 1000
            ToolTip.text: qsTr("Dismiss this message")
            onClicked: notificationItem.dismissNotification()
        }
    }
}
