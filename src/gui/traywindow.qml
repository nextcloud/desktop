import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

Window {
    id: trayWindow
    visible: true
    width: 420
    height: 500
    color: "transparent"
    flags: Qt.FramelessWindowHint

    Component.onCompleted: {
            // desktopAvailableWidth and Height doesn't include the system tray bar
            setX(Screen.desktopAvailableWidth - width);
            setY(Screen.desktopAvailableHeight + height);
    }

    Rectangle {
        id: trayWindowBackground
        anchors.fill: parent
        radius: 10

        Rectangle {
            id: trayWindowHeaderBackground
            anchors.left: trayWindowBackground.left
            anchors.top: trayWindowBackground.top
            height: 60
            width: parent.width
            radius: 10
            color: "#0082c9"

            Rectangle {
                anchors.left: trayWindowHeaderBackground.left
                anchors.bottom: trayWindowHeaderBackground.bottom
                height: 30
                width: parent.width
                color: "#0082c9"
            }

            RowLayout {
                id: trayWindowHeaderLayout
                spacing: 2
                anchors.fill: parent

                Item {
                    id: avatarButtonContainer
                    Layout.alignment: Qt.AlignLeft
                    width: (trayWindowHeaderBackground.height - 12)
                    height: (trayWindowHeaderBackground.height - 12)
                    Layout.margins: 4
                    Image {
                        id: currentAvatarButton
                        width: (trayWindowHeaderBackground.height - 12)
                        height: (trayWindowHeaderBackground.height - 12)
                        antialiasing: true
                        Layout.margins: 4
                        source: "../avatar.png"
                    }

                    Button {
                        id: currentAccountButton
                        width: (trayWindowHeaderBackground.height + 4)
                        height: (trayWindowHeaderBackground.height)
                        display: AbstractButton.IconOnly
                        flat: true

                        MouseArea {
                            id: accountBtnMouseArea
                            width: currentAccountButton.width + accountLabels.width + 8
                            height: trayWindowHeaderBackground.height - 6
                            onClicked:
                            {
                                accountMenu.popup()
                            }

                            Menu {
                                id: accountMenu
                                background: Rectangle {
                                    id: menubackground
                                    implicitWidth: 200
                                    implicitHeight: 40
                                    anchors.fill: parent
                                    radius: 10
                                }

                                MenuItem { text: "test" }
                            }
                         }

                         background:
                            Rectangle {
                                color: "transparent"
                            }
                    }
                }

                Column {
                    id: accountLabels
                    Layout.leftMargin: 0
                    spacing: 4
                    Layout.alignment: Qt.AlignLeft
                    //anchors.left: currentAvatarButton.right
                    Label {
                        id: syncStatusLabel
                        text: "Everything up to date"
                        color: "white"
                        font.pointSize: 9
                        font.bold: true
                    }
                    Label {
                        id: currentUserLabel
                        text: "freddie@nextcloud.com"
                        color: "white"
                        font.pointSize: 8
                    }
                }

                Label {
                    text: "\u25BC"
                    Layout.bottomMargin: 10
                    Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                    font.pointSize: 9
                    color: "white"
                    verticalAlignment: Qt.AlignBottom
                }

                Item {
                    id: trayWindowHeaderSpacer
                    Layout.fillWidth: true
                }

                Button {
                    id: openLocalFolderButton
                    rightPadding: 2
                    leftPadding: 2
                    Layout.alignment: Qt.AlignRight
                    display: AbstractButton.IconOnly
                    flat: true
                    Layout.preferredWidth: (trayWindowHeaderBackground.height - 12)
                    Layout.preferredHeight: (trayWindowHeaderBackground.height - 12)

                    icon.source: "../files.png"
                    icon.color: "transparent"

                    MouseArea {
                        id: folderBtnMouseArea
                        anchors.fill: parent
                        onClicked:
                        {
                        }
                     }

                     background:
                        Rectangle {
                            color: "transparent"
                        }
                }

                Button {
                    id: trayWindowTalkButton
                    rightPadding: 2
                    leftPadding: 2
                    Layout.alignment: Qt.AlignRight
                    display: AbstractButton.IconOnly
                    Layout.preferredWidth: (trayWindowHeaderBackground.height - 12)
                    Layout.preferredHeight: (trayWindowHeaderBackground.height - 12)
                    flat: true
                    Layout.margins: 4

                    icon.source: "../talk.png"
                    icon.color: "transparent"

                    MouseArea {
                        id: talkBtnMouseArea
                        anchors.fill: parent
                        onClicked:
                        {
                        }
                     }

                     background:
                        Rectangle {
                            color: "transparent"
                        }
                }

                Button {
                    id: trayWindowAppsButton
                    rightPadding: 2
                    leftPadding: 2
                    Layout.alignment: Qt.AlignRight
                    display: AbstractButton.IconOnly
                    Layout.preferredWidth: (trayWindowHeaderBackground.height - 12)
                    Layout.preferredHeight: (trayWindowHeaderBackground.height - 12)
                    flat: true
                    Layout.margins: 4

                    icon.source: "../apps.png"
                    icon.color: "transparent"

                    MouseArea {
                        id: appsBtnMouseArea
                        anchors.fill: parent
                        onClicked:
                        {
                        }
                     }

                     background:
                        Rectangle {
                            color: "transparent"
                        }
                }
            }
        }   // Rectangle trayWindowHeaderBackground

        /*ListView {
            anchors.top: trayWindowHeaderBackground.bottom
            Layout.fillWidth: true
            Layout.fillHeight: true
        }*/

    }       // Rectangle trayWindowBackground
}
