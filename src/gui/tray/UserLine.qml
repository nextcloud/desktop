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

    Accessible.role: Accessible.MenuItem
    Accessible.name: qsTr("Account entry")

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

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Switch to account") + " " + name

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onContainsMouseChanged: {
                        accountStateIndicatorBackground.color = (containsMouse ? Style.activePalette.button : Style.activePalette.window)
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
                        color: parent.parent.hovered ? Style.activePalette.button : "transparent"
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
                        source: model.avatar != "" ? model.avatar : "image://avatars/fallbackBlack"
                        Layout.preferredHeight: (userLineLayout.height -16)
                        Layout.preferredWidth: (userLineLayout.height -16)
                        Rectangle {
                            id: accountStateIndicatorBackground
                            width: accountStateIndicator.sourceSize.width + 2
                            height: width
                            anchors.bottom: accountAvatar.bottom
                            anchors.right: accountAvatar.right
                            color: Style.activePalette.window
                            radius: width*0.5
                        }
                        Image {
                            id: accountStateIndicator
                            source: model.isConnected
                                    ? Style.stateOnlineImageSource
                                    : Style.stateOfflineImageSource
                            cache: false
                            x: accountStateIndicatorBackground.x + 1
                            y: accountStateIndicatorBackground.y + 1
                            sourceSize.width: Style.accountAvatarStateIndicatorSize
                            sourceSize.height: Style.accountAvatarStateIndicatorSize

                            Accessible.role: Accessible.Indicator
                            Accessible.name: model.isConnected ? qsTr("Account connected") : qsTr("Account not connected")
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
                            color: Style.activePalette.windowText
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Label {
                            id: accountServer
                            width: 128
                            text: server
                            elide: Text.ElideRight
                            color: Style.activePalette.windowText
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

                icon.source: "image://colorsvgimages/client/theme/more.svg"
                icon.color: "transparent"

                Accessible.role: Accessible.ButtonMenu
                Accessible.name: qsTr("Account actions")
                Accessible.onPressAction: userMoreButtonMouseArea.clicked()

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
                    color: userMoreButtonMouseArea.containsMouse ? Style.activePalette.alternateBase : "transparent"
                    height: userMoreButton.height - 2
                    y: userMoreButton.y + 1
                }

                Menu {
                    id: userMoreButtonMenu
                    width: 120
                    closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                    background: Rectangle {
                        border.color: Style.activePalette.alternateBase
                        color: Style.activePalette.window
                        radius: 2
                    }

                    MenuItem {
                        id: logOutButton
                        text: model.isConnected ? qsTr("Log out") : qsTr("Log in")
                        font.pixelSize: Style.topLinePixelSize
                        hoverEnabled: true
                        onClicked: {
                            model.isConnected ? UserModel.logout(index) : UserModel.login(index)
                            accountMenu.close()
                        }

                        contentItem: Label {
                            text: logOutButton.text
                            font.pixelSize: Style.topLinePixelSize
                            color: Style.activePalette.windowText
                        }

                        background: Item {
                            height: parent.height
                            width: parent.menu.width
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                color: parent.parent.hovered ? Style.activePalette.alternateBase : "transparent"
                            }
                        }

                        Accessible.role: Accessible.Button
                        Accessible.name: model.isConnected ? qsTr("Log out") : qsTr("Log in")

                        onPressed: {
                            if (model.isConnected) {
                                UserModel.logout(index)
                            } else {
                                UserModel.login(index)
                            }
                            accountMenu.close()
                        }
                    }

                    MenuItem {
                        id: removeAccountButton
                        text: qsTr("Remove account")
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
                                color: parent.parent.hovered ? Style.activePalette.alternateBase : "transparent"
                            }
                        }

                        contentItem: Label {
                            text: removeAccountButton.text
                            font.pixelSize: Style.topLinePixelSize
                            color: Style.activePalette.windowText
                        }


                        Accessible.role: Accessible.Button
                        Accessible.name: text
                        Accessible.onPressAction: removeAccountButton.clicked()
                    }
                }
            }
        }

        Connections {
            target: UserModel
            onRefreshCurrentUserGui: {
                accountStateIndicator.source = model.isConnected
                        ? Style.stateOnlineImageSource
                        : Style.stateOfflineImageSource
            }
        }
}   // MenuItem userLine
