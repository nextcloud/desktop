import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

MenuItem {

    Connections {
        target: systrayBackend
        onRefreshUserMenu: {
            userLine.visible = isCurrentUser ? false : true
            userLine.height = isCurrentUser ? 0 : 60
        }
    }

    id: userLine
    visible: isCurrentUser ? false : true
    width: 216
    height: isCurrentUser ? 0 : 60
    //color: "transparent"

    Rectangle {
        id: userLineBackground
        height: userLine.height
        anchors.fill: parent
        color: "transparent"

        RowLayout {
            id: userLineLayout
            spacing: 0
            anchors.fill: parent

            Button {
                id: accountButton
                anchors.centerIn: parent
                Layout.preferredWidth: (userLine.width - 4)
                Layout.preferredHeight: (userLineBackground.height - 2)
                display: AbstractButton.IconOnly
                flat: true

                background: Rectangle {
                    color: "transparent"
                }

                onClicked: {
                    systrayBackend.switchCurrentUser(index)
                }

                RowLayout {
                    id: accountControlRowLayout
                    height: accountButton.height
                    width: accountButton.width
                    spacing: 0
                    Image {
                        id: accountAvatar
                        Layout.leftMargin: 4
                        verticalAlignment: Qt.AlignCenter
                        source: avatar
                        Layout.preferredHeight: (userLineBackground.height -16)
                        Layout.preferredWidth: (userLineBackground.height -16)
                    }

                    Column {
                        id: accountLabels
                        spacing: 4
                        Layout.alignment: Qt.AlignLeft
                        Layout.leftMargin: 12
                        Label {
                            id: accountUser
                            text: name
                            color: "black"
                            font.pointSize: 9
                            font.bold: true
                        }
                        Label {
                            id: accountServer
                            text: server
                            color: "black"
                            font.pointSize: 8
                        }
                    }

                    Item {
                        id: userLineSpacer
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }   // Rectangle userLineBackground
}   // MenuItem userLine
