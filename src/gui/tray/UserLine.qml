import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

MenuItem {

    Connections {
        target: userModelBackend
        onRefreshUserMenu: {
        }
    }

    id: userLine
    width: 216
    height: 60

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
                    userModelBackend.switchCurrentUser(index)
                }

                RowLayout {
                    id: accountControlRowLayout
                    height: accountButton.height
                    width: accountButton.width
                    spacing: 0
                    Image {
                        id: accountAvatar
                        Layout.leftMargin: 2
                        verticalAlignment: Qt.AlignCenter
                        source: ("image://avatars/" + index)
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
                            width: 120
                            text: name
                            elide: Text.ElideRight
                            color: "black"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Label {
                            id: accountServer
                            width: 120
                            text: server
                            elide: Text.ElideRight
                            color: "black"
                            font.pixelSize: 10
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
