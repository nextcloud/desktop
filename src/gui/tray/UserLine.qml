import QtQuick 2.9
import QtQuick.Window 2.3
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

// Custom qml modules are in /theme (and included by resources.qrc)
import Style 1.0
import com.nextcloud.desktopclient 1.0

MenuItem {
    id: userLine
    height: Style.trayWindowHeaderHeight

        RowLayout {
            id: userLineLayout
            spacing: 0
            width: Style.currentAccountButtonWidth
            height: parent.height

            Button {
                id: accountButton
                Layout.preferredWidth: (userLineLayout.width * (5/6))
                Layout.preferredHeight: (userLineLayout.height)
                display: AbstractButton.IconOnly
                hoverEnabled: true
                flat: true

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onContainsMouseChanged: {
                        accountStateIndicatorBackground.color = (containsMouse ? "#f6f6f6" : "white")
                    }
                    onClicked: {
                        if (!isCurrentUser) {
                            UserModel.switchCurrentUser(id)
                        } else {
                            accountMenu.close()
                        }
                    }
                }


                background: Item {
                    height: parent.height
                    width: userLine.menu ? userLine.menu.width : 0
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        color: parent.parent.hovered ? Style.lightHover : "transparent"
                    }
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
                        source: ("image://avatars/" + id)
                        Layout.preferredHeight: (userLineLayout.height -16)
                        Layout.preferredWidth: (userLineLayout.height -16)
                        Rectangle {
                            id: accountStateIndicatorBackground
                            width: accountStateIndicator.sourceSize.width + 2
                            height: width
                            anchors.bottom: accountAvatar.bottom
                            anchors.right: accountAvatar.right
                            color: "white"
                            radius: width*0.5
                        }
                        Image {
                            id: accountStateIndicator
                            source: isConnected ? "qrc:///client/theme/colored/state-ok.svg" : "qrc:///client/theme/colored/state-offline.svg"
                            cache: false
                            x: accountStateIndicatorBackground.x + 1
                            y: accountStateIndicatorBackground.y + 1
                            sourceSize.width: Style.accountAvatarStateIndicatorSize
                            sourceSize.height: Style.accountAvatarStateIndicatorSize
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

                icon.source: "qrc:///client/theme/more.svg"
                icon.color: "transparent"

                MouseArea {
                    id: userMoreButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (userMoreButtonMenu.visible) {
                            userMoreButtonMenu.close()
                        } else {
                            userMoreButtonMenu.popup()
                        }
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
                    closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                    background: Rectangle {
                        border.color: Style.menuBorder
                        radius: 2
                    }

                    MenuItem {
                        text: isConnected ? qsTr("Log out") : qsTr("Log in")
                        font.pixelSize: Style.topLinePixelSize
                        hoverEnabled: true
                        onClicked: {
                            isConnected ? UserModel.logout(index) : UserModel.login(index)
                            accountMenu.close()
                        }

                        background: Item {
                            height: parent.height
                            width: parent.menu.width
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                color: parent.parent.hovered ? Style.lightHover : "transparent"
                            }
                        }
                    }

                    MenuItem {
                        text: qsTr("Remove Account")
                        font.pixelSize: Style.topLinePixelSize
                        hoverEnabled: true
                        onClicked: {
                            UserModel.removeAccount(index)
                            accountMenu.close()
                        }

                        background: Item {
                            height: parent.height
                            width: parent.menu.width
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                color: parent.parent.hovered ? Style.lightHover : "transparent"
                            }
                        }
                    }
                }
            }
        }
}   // MenuItem userLine
