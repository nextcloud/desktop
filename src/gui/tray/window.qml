import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0

Window {

    id: trayWindow
    visible: true
    width: 400
    height: 500
    color: "transparent"
    flags: Qt.FramelessWindowHint

    Component.onCompleted: {
            /* desktopAvailableWidth and Height doesn't include the system tray bar
               but breaks application anyway on windows when using multi monitor setup,
               will look for a better solution later, for now just get this thing complete */
            //setX(Screen.desktopAvailableWidth - width);
            //setY(Screen.desktopAvailableHeight + height);
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
            radius: 9
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
                spacing: 0
                anchors.fill: parent

                Button {
                    id: currentAccountButton
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: (trayWindowHeaderBackground.height)
                    display: AbstractButton.IconOnly
                    flat: true

                    MouseArea {
                        id: accountBtnMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked:
                        {
                            accountMenu.popup()
                        }

                        Menu {
                            id: accountMenu
                            background:
                                Rectangle {
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
                        Item {
                        id: leftHoverContainer
                        height: currentAccountButton.height
                        width: currentAccountButton.width
                            Rectangle {
                                width: currentAccountButton.width / 2
                                height: currentAccountButton.height / 2
                                color: "transparent"
                                clip: true
                                Rectangle {
                                    width: currentAccountButton.width
                                    height: currentAccountButton.height
                                    radius: 10
                                    color: "white"
                                    opacity: 0.2
                                    visible: accountBtnMouseArea.containsMouse
                                }
                            }
                            Rectangle {
                                width: currentAccountButton.width / 2
                                height: currentAccountButton.height / 2
                                anchors.bottom: leftHoverContainer.bottom
                                color: "white"
                                opacity: 0.2
                                visible: accountBtnMouseArea.containsMouse
                            }
                            Rectangle {
                                width: currentAccountButton.width / 2
                                height: currentAccountButton.height / 2
                                anchors.right: leftHoverContainer.right
                                color: "white"
                                opacity: 0.2
                                visible: accountBtnMouseArea.containsMouse
                            }
                            Rectangle {
                                width: currentAccountButton.width / 2
                                height: currentAccountButton.height / 2
                                anchors.right: leftHoverContainer.right
                                anchors.bottom: leftHoverContainer.bottom
                                color: "white"
                                opacity: 0.2
                                visible: accountBtnMouseArea.containsMouse
                            }
                        }

                    RowLayout {
                        id: accountControlRowLayout
                        height: currentAccountButton.height
                        width: currentAccountButton.width
                        spacing: 0
                        Image {
                            id: currentAccountAvatar
                            width: (trayWindowHeaderBackground.height - 12)
                            height: (trayWindowHeaderBackground.height - 12)
                            Layout.leftMargin: 6
                            verticalAlignment: Qt.AlignCenter
                            source: systrayBackend.currentAvatar()
                            Layout.preferredHeight: (trayWindowHeaderBackground.height -12)
                            Layout.preferredWidth: (trayWindowHeaderBackground.height -12)
                        }

                        Column {
                            id: accountLabels
                            spacing: 4
                            Layout.alignment: Qt.AlignLeft
                            Layout.leftMargin: 6
                            Label {
                                id: currentAccountUser
                                text: systrayBackend.currentAccountUser()
                                color: "white"
                                font.pointSize: 9
                                font.bold: true
                            }
                            Label {
                                id: currentAccountServer
                                text: systrayBackend.currentAccountServer()
                                color: "white"
                                font.pointSize: 8
                            }
                        }

                        /*Item {
                            Layout.preferredWidth: 6
                        }*/

                        Image {
                            Layout.alignment: Qt.AlignLeft
                            verticalAlignment: Qt.AlignCenter
                            Layout.margins: 12
                            //source: "../../theme/white/caret-down.svg"
                            source: "qrc:///client/theme/white/caret-down.svg"
                        }
                    }
                }

                Item {
                    id: trayWindowHeaderSpacer
                    Layout.fillWidth: true
                }

                Button {
                    id: openLocalFolderButton
                    Layout.alignment: Qt.AlignRight
                    display: AbstractButton.IconOnly
                    Layout.preferredWidth: (trayWindowHeaderBackground.height)
                    Layout.preferredHeight: (trayWindowHeaderBackground.height)
                    flat: true

                    //icon.source: "../../theme/white/folder.svg"
                    icon.source: "qrc:///client/theme/white/folder.svg"
                    icon.color: "transparent"

                    MouseArea {
                        id: folderBtnMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked:
                        {
                        }
                    }

                    background:
                       Rectangle {
                        color: folderBtnMouseArea.containsMouse ? "white" : "transparent"
                        opacity: 0.2
                       }
                }

                Button {
                    id: trayWindowTalkButton
                    Layout.alignment: Qt.AlignRight
                    display: AbstractButton.IconOnly
                    Layout.preferredWidth: (trayWindowHeaderBackground.height)
                    Layout.preferredHeight: (trayWindowHeaderBackground.height)
                    flat: true

                    //icon.source: "../../theme/white/talk-app.svg"
                    icon.source: "qrc:///client/theme/white/talk-app.svg"
                    icon.color: "transparent"

                    MouseArea {
                        id: talkBtnMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked:
                        {
                        }
                     }

                     background:
                        Rectangle {
                         color: talkBtnMouseArea.containsMouse ? "white" : "transparent"
                         opacity: 0.2
                        }
                }

                Button {
                    id: trayWindowAppsButton
                    Layout.alignment: Qt.AlignRight
                    display: AbstractButton.IconOnly
                    Layout.preferredWidth: (trayWindowHeaderBackground.height)
                    Layout.preferredHeight: (trayWindowHeaderBackground.height)
                    flat: true

                    //icon.source: "../../theme/white/more-apps.svg"
                    icon.source: "qrc:///client/theme/white/more-apps.svg"
                    icon.color: "transparent"

                    MouseArea {
                        id: appsBtnMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked:
                        {
                        }
                     }

                     background:
                        Item {
                        id: rightHoverContainer
                        height: trayWindowAppsButton.height
                        width: trayWindowAppsButton.width
                        Rectangle {
                            width: trayWindowAppsButton.width / 2
                            height: trayWindowAppsButton.height / 2
                            color: "white"
                            opacity: 0.2
                            visible: appsBtnMouseArea.containsMouse
                        }
                        Rectangle {
                            width: trayWindowAppsButton.width / 2
                            height: trayWindowAppsButton.height / 2
                            anchors.bottom: rightHoverContainer.bottom
                            color: "white"
                            opacity: 0.2
                            visible: appsBtnMouseArea.containsMouse
                        }
                        Rectangle {
                            width: trayWindowAppsButton.width / 2
                            height: trayWindowAppsButton.height / 2
                            anchors.bottom: rightHoverContainer.bottom
                            anchors.right: rightHoverContainer.right
                            color: "white"
                            opacity: 0.2
                            visible: appsBtnMouseArea.containsMouse
                        }
                            Rectangle {
                                id: rightHoverContainerClipper
                                anchors.right: rightHoverContainer.right
                                width: trayWindowAppsButton.width / 2
                                height: trayWindowAppsButton.height / 2
                                color: "transparent"
                                clip: true
                                Rectangle {
                                    width: trayWindowAppsButton.width
                                    height: trayWindowAppsButton.height
                                    anchors.right: rightHoverContainerClipper.right
                                    radius: 10
                                    color: "white"
                                    opacity: 0.2
                                    visible: appsBtnMouseArea.containsMouse
                                }
                            }
                        }
                }
            }
        }   // Rectangle trayWindowHeaderBackground

        ListModel {
            id: activityListModel
        }

        ListView {
            id: activityListView
            anchors.top: trayWindowHeaderBackground.bottom
            width:  trayWindowBackground.width
            height: trayWindowBackground.height - trayWindowHeaderBackground.height
            clip: true

            model: activityListModel

            delegate: RowLayout {
                id: activityItem
                width: activityListView.width
                height: trayWindowHeaderLayout.height
                spacing: 0
                Image {
                    id: activityIcon
                    Layout.leftMargin: 6
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    verticalAlignment: Qt.AlignCenter
                    source: "qrc:///client/theme/black/state-sync.svg"
                    sourceSize.height: 48
                    sourceSize.width: 48
                }
                Column {
                    Layout.leftMargin: 6
                    spacing: 4
                    Layout.alignment: Qt.AlignLeft
                    Text {
                        id: activityTextTitle
                        text: name
                        font.pointSize: 9
                    }
                    Text {
                        id: activityTextInfo
                        text: "Lorem ipsum dolor sit amet"
                        font.pointSize: 8
                    }
                }
                Item {
                    id: activityItemFiller
                    Layout.fillWidth: true
                }
                Button {
                    Layout.preferredWidth: activityItem.height
                    Layout.preferredHeight: activityItem.height
                    Layout.alignment: Qt.AlignRight
                    flat: true
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:///client/resources/files.svg"
                    icon.color: "transparent"
                }
                Button {
                    Layout.preferredWidth: activityItem.height
                    Layout.preferredHeight: activityItem.height
                    Layout.alignment: Qt.AlignRight
                    flat: true
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:///client/resources/public.svg"
                    icon.color: "transparent"
                }
            }

            add: Transition {
                NumberAnimation { properties: "y"; from: -60; duration: 100; easing.type: Easing.Linear }
            }

            remove: Transition {
                NumberAnimation { property: "opacity"; from: 1.0; to: 0; duration: 100 }
            }

            removeDisplaced: Transition {
                SequentialAnimation {
                    PauseAnimation { duration: 100}
                    NumberAnimation { properties: "y"; duration: 100; easing.type: Easing.Linear }
                }
            }

            displaced: Transition {
                NumberAnimation { properties: "y"; duration: 100; easing.type: Easing.Linear }
            }

            focus: true

            // For interactive ListView/Animation testing only
            //Keys.onSpacePressed: model.insert(0, { "name": "Item " + model.count })
            //Keys.onTabPressed: model.remove(3)
        }

    }       // Rectangle trayWindowBackground
}
