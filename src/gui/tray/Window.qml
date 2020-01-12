import QtQml 2.0
import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.0

Window {

    id: trayWindow
    visible: true
    width: 400
    height: 500
    color: "transparent"
    flags: Qt.FramelessWindowHint

    onActiveChanged: {
        if(!active) {
            trayWindow.hide();
            systrayBackend.setClosed();
        }
    }

    onVisibleChanged: {
        currentAccountAvatar.source = ""
        currentAccountAvatar.source = "image://avatars/currentUser"
        currentAccountUser.text = userModelBackend.currentUserName();
        currentAccountServer.text = userModelBackend.currentUserServer();
        trayWindowTalkButton.visible = userModelBackend.currentServerHasTalk() ? true : false;
        currentAccountStateIndicator.source = ""
        currentAccountStateIndicator.source = userModelBackend.isUserConnected(userModelBackend.currentUserId()) ? "qrc:///client/theme/colored/state-ok.svg" : "qrc:///client/theme/colored/state-offline.svg"

        userLineInstantiator.active = false;
        userLineInstantiator.active = true;
    }

    Connections {
        target: userModelBackend
        onRefreshCurrentUserGui: {
            currentAccountAvatar.source = ""
            currentAccountAvatar.source = "image://avatars/currentUser"
            currentAccountUser.text = userModelBackend.currentUserName();
            currentAccountServer.text = userModelBackend.currentUserServer();
            currentAccountStateIndicator.source = ""
            currentAccountStateIndicator.source = userModelBackend.isUserConnected(userModelBackend.currentUserId()) ? "qrc:///client/theme/colored/state-ok.svg" : "qrc:///client/theme/colored/state-offline.svg"
        }
        onNewUserSelected: {
            accountMenu.close();
            trayWindowTalkButton.visible = userModelBackend.currentServerHasTalk() ? true : false;
        }
    }

    Connections {
        target: systrayBackend
        onShowWindow: {
            accountMenu.close();
            trayWindow.show();
            trayWindow.raise();
            trayWindow.requestActivate();
            trayWindow.setX( systrayBackend.calcTrayWindowX());
            trayWindow.setY( systrayBackend.calcTrayWindowY());
            systrayBackend.setOpened();
        }
        onHideWindow: {
            trayWindow.hide();
            systrayBackend.setClosed();
        }
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
                            syncPauseButton.text = systrayBackend.syncIsPaused() ? "Resume sync for all" : "Pause sync for all"
                            accountMenu.open()
                        }

                        Menu {
                            id: accountMenu
                            x: (currentAccountButton.x + 2)
                            y: (currentAccountButton.y + currentAccountButton.height + 2)
                            width: (currentAccountButton.width - 2)
                            closePolicy: "CloseOnPressOutside"

                            background: Rectangle {
                                border.color: "#0082c9"
                                radius: 2
                            }

                            onClosed: {
                                userLineInstantiator.active = false;
                                userLineInstantiator.active = true;
                            }

                            Instantiator {
                                id: userLineInstantiator
                                model: userModelBackend
                                delegate: UserLine {}
                                onObjectAdded: accountMenu.insertItem(index, object)
                                onObjectRemoved: accountMenu.removeItem(object)
                            }

                            MenuItem {
                                id: addAccountButton
                                height: 60

                                RowLayout {
                                    width: addAccountButton.width
                                    height: addAccountButton.height
                                    spacing: 0

                                    Image {
                                        Layout.leftMargin: 8
                                        verticalAlignment: Qt.AlignCenter
                                        source: "qrc:///client/theme/black/add.svg"
                                        sourceSize.width: addAccountButton.height - 24
                                        sourceSize.height: addAccountButton.height - 24
                                    }
                                    Label {
                                        Layout.leftMargin: 10
                                        text: "Add account"
                                        color: "black"
                                        font.pixelSize: 12
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                    }
                                }
                                onClicked: userModelBackend.addAccount()
                            }

                            MenuSeparator { id: accountMenuSeparator }

                            MenuItem {
                                id: syncPauseButton
                                onClicked: systrayBackend.pauseResumeSync()
                            }

                            MenuItem {
                                text: "Open settings"
                                onClicked: systrayBackend.openSettings()
                            }

                            MenuItem {
                                text: "Help"
                                onClicked: systrayBackend.openHelp()
                            }

                            MenuItem {
                                text: "Quit Nextcloud"
                                onClicked: systrayBackend.shutdown()
                            }

                            Component.onCompleted: {/*
                                if(userModelBackend.numUsers() === 1) {
                                    accountMenuSeparator.height = 0
                                } else {
                                    accountMenuSeparator.height = 13
                                }*/
                            }
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
                            Layout.leftMargin: 8
                            verticalAlignment: Qt.AlignCenter
                            cache: false
                            source: "image://avatars/currentUser"
                            Layout.preferredHeight: (trayWindowHeaderBackground.height -16)
                            Layout.preferredWidth: (trayWindowHeaderBackground.height -16)
                            Image {
                                id: currentAccountStateIndicator
                                source: userModelBackend.isUserConnected(userModelBackend.currentUserId()) ? "qrc:///client/theme/colored/state-ok.svg" : "qrc:///client/theme/colored/state-offline.svg"
                                cache: false
                                anchors.bottom: currentAccountAvatar.bottom
                                anchors.right: currentAccountAvatar.right
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
                                id: currentAccountUser
                                width: 128
                                text: userModelBackend.currentUserName()
                                elide: Text.ElideRight
                                color: "white"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Label {
                                id: currentAccountServer
                                width: 128
                                text: userModelBackend.currentUserServer()
                                elide: Text.ElideRight
                                color: "white"
                                font.pixelSize: 10
                            }
                        }

                        Image {
                            Layout.alignment: Qt.AlignRight
                            verticalAlignment: Qt.AlignCenter
                            Layout.margins: 8
                            source: "qrc:///client/theme/white/caret-down.svg"
                            sourceSize.width: 20
                            sourceSize.height: 20
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

                    icon.source: "qrc:///client/theme/white/folder.svg"
                    icon.color: "transparent"

                    MouseArea {
                        id: folderBtnMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked:
                        {
                            userModelBackend.openCurrentAccountLocalFolder();
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
                    visible: userModelBackend.currentServerHasTalk() ? true : false

                    icon.source: "qrc:///client/theme/white/talk-app.svg"
                    icon.color: "transparent"

                    MouseArea {
                        id: talkBtnMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked:
                        {
                            userModelBackend.openCurrentAccountTalk();
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

                    icon.source: "qrc:///client/theme/white/more-apps.svg"
                    icon.color: "transparent"

                    MouseArea {
                        id: appsBtnMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked:
                        {
                            userModelBackend.openCurrentAccountServer();
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

        ListView {
            id: activityListView
            anchors.top: trayWindowHeaderBackground.bottom
            width:  trayWindowBackground.width
            height: trayWindowBackground.height - trayWindowHeaderBackground.height
            clip: true

            model: activityModel

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
                    id: activityTextColumn
                    Layout.leftMargin: 6
                    spacing: 4
                    Layout.alignment: Qt.AlignLeft
                    Text {
                        id: activityTextTitle
                        text: subject
                        width: 220
                        elide: Text.ElideRight
                        font.pointSize: 9
                    }
                    Text {
                        id: activityTextInfo
                        text: path
                        width: 220
                        elide: Text.ElideRight
                        font.pointSize: 8
                    }
                }
                Item {
                    id: activityItemFiller
                    Layout.fillWidth: true
                }
                Button {
                    id: activityButton1
                    Layout.preferredWidth: activityItem.height
                    Layout.preferredHeight: activityItem.height
                    Layout.alignment: Qt.AlignRight
                    flat: true
                    hoverEnabled: false
                    visible: (path === "") ? false : true
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:///client/resources/files.svg"
                    icon.color: "transparent"

                    onClicked:
                    {
                         Qt.openUrlExternally(path)
                    }
                }
                Button {
                    Layout.preferredWidth: activityItem.height
                    Layout.preferredHeight: activityItem.height
                    Layout.alignment: Qt.AlignRight
                    flat: true
                    hoverEnabled: false
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
