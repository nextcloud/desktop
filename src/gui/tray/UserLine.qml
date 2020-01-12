import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

MenuItem {
    id: userLine
    height: 60

        RowLayout {
            id: userLineLayout
            spacing: 0
            width: 220
            height: 60

            Button {
                id: accountButton
                Layout.preferredWidth: (userLineLayout.width * (5/6))
                Layout.preferredHeight: (userLineLayout.height)
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
                        Layout.leftMargin: 4
                        verticalAlignment: Qt.AlignCenter
                        cache: false
                        source: ("image://avatars/" + index)
                        Layout.preferredHeight: (userLineLayout.height -16)
                        Layout.preferredWidth: (userLineLayout.height -16)
                        Image {
                            id: accountStateIndicator
                            source: userModelBackend.isUserConnected(index) ? "qrc:///client/theme/colored/state-ok.svg" : "qrc:///client/theme/colored/state-offline.svg"
                            cache: false
                            anchors.bottom: accountAvatar.bottom
                            anchors.right: accountAvatar.right
                            sourceSize.width: 16
                            sourceSize.height: 16
                        }
                    }

                    Column {
                        id: accountLabels
                        spacing: 4
                        Layout.alignment: Qt.AlignLeft
                        Layout.leftMargin: 6
                        Label {
                            id: accountUser
                            width: 128
                            text: name
                            elide: Text.ElideRight
                            color: "black"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Label {
                            id: accountServer
                            width: 128
                            text: server
                            elide: Text.ElideRight
                            color: "black"
                            font.pixelSize: 10
                        }
                    }
                }
            } // accountButton

            Button {
                id: userMoreButton
                Layout.preferredWidth: (userLineLayout.width * (1/6))
                Layout.preferredHeight: userLineLayout.height
                flat: true

                icon.source: "qrc:///client/resources/more.svg"
                icon.color: "transparent"

                MouseArea {
                    id: userMoreButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked:
                    {
                        userMoreButtonMenu.popup()
                    }
                }
                background:
                    Rectangle {
                    color: userMoreButtonMouseArea.containsMouse ? "grey" : "transparent"
                    opacity: 0.2
                    height: userMoreButton.height - 2
                    y: userMoreButton.y + 1
                }

                Menu {
                    id: userMoreButtonMenu
                    width: 120

                    background: Rectangle {
                        border.color: "#0082c9"
                        radius: 2
                    }

                    MenuItem {
                        text: userModelBackend.isUserConnected(index) ? "Log out" : "Log in"
                        onClicked: {
                            userModelBackend.isUserConnected(index) ? userModelBackend.logout(index) : userModelBackend.logout(index)
                        }
                    }

                    MenuItem {
                        text: "Remove Account"
                        onClicked: {
                            userModelBackend.removeAccount(index)
                        }
                    }
                }
            }
        }
}   // MenuItem userLine
