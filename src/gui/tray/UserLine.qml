import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

MenuItem {

    id: userLine
    visible: true
    width: 220
    height: 60
    //color: "transparent"

    Rectangle {
        id: userLineBackground
        height: 60
        anchors.fill: parent
        color: "transparent"

        RowLayout {
            id: userLineLayout
            spacing: 0
            anchors.fill: parent

            Button {
                id: currentAccountButton
                Layout.preferredWidth: 220
                Layout.preferredHeight: (userLineBackground.height)
                display: AbstractButton.IconOnly
                flat: true

                MouseArea {
                    id: accountBtnMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked:
                    {
                        //
                    }
                }

                RowLayout {
                    id: accountControlRowLayout
                    height: currentAccountButton.height
                    width: currentAccountButton.width
                    spacing: 0
                    Image {
                        id: currentAccountAvatar
                        Layout.leftMargin: 6
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
                            id: currentAccountUser
                            text: name
                            color: "black"
                            font.pointSize: 9
                            font.bold: true
                        }
                        Label {
                            id: currentAccountServer
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
