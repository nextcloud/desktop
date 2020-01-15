import QtQml 2.1
import QtQml.Models 2.1
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
            userModelBackend.fetchCurrentActivityModel();
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
                            syncPauseButton.text = systrayBackend.syncIsPaused() ? qsTr("Resume sync for all") : qsTr("Pause sync for all")
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
                                height: 50

                                RowLayout {
                                    width: addAccountButton.width
                                    height: addAccountButton.height
                                    spacing: 0

                                    Image {
                                        Layout.leftMargin: 14
                                        verticalAlignment: Qt.AlignCenter
                                        source: "qrc:///client/theme/black/add.svg"
                                        sourceSize.width: openLocalFolderButton.icon.width
                                        sourceSize.height: openLocalFolderButton.icon.height
                                    }
                                    Label {
                                        Layout.leftMargin: 14
                                        text: qsTr("Add account")
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
                                font.pixelSize: 12
                                onClicked: systrayBackend.pauseResumeSync()
                            }

                            MenuItem {
                                text: qsTr("Open settings")
                                font.pixelSize: 12
                                onClicked: systrayBackend.openSettings()
                            }

                            MenuItem {
                                text: qsTr("Help")
                                font.pixelSize: 12
                                onClicked: systrayBackend.openHelp()
                            }

                            MenuItem {
                                text: qsTr("Quit Nextcloud")
                                font.pixelSize: 12
                                onClicked: systrayBackend.shutdown()
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
                            /*
                            // The count() property was introduced in QtQuick.Controls 2.3 (Qt 5.10)
                            // so we handle this with userModelBackend.openCurrentAccountServer()
                            //
                            // See UserModel::openCurrentAccountServer() to disable this workaround
                            // in the future for Qt >= 5.10

                            if(appsMenu.count() > 0) {
                                appsMenu.popup();
                            } else {
                                userModelBackend.openCurrentAccountServer();
                            }
                            */

                            appsMenu.open();
                            userModelBackend.openCurrentAccountServer();
                        }

                        Menu {
                            id: appsMenu
                            x: (trayWindowAppsButton.x + 2)
                            y: (trayWindowAppsButton.y + trayWindowAppsButton.height + 2)
                            width: (trayWindowAppsButton.width - 2)
                            closePolicy: "CloseOnPressOutside"

                            background: Rectangle {
                                border.color: "#0082c9"
                                radius: 2
                            }

                            Instantiator {
                                id: appsMenuInstantiator
                                model: appsMenuModelBackend
                                onObjectAdded: appsMenu.insertItem(index, object)
                                onObjectRemoved: appsMenu.removeItem(object)
                                delegate: MenuItem {
                                    text: appName
                                    onTriggered: appsMenuModelBackend.openAppUrl(appUrl)
                                }
                            }
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
            ScrollBar.vertical: ScrollBar {
                id: listViewScrollbar
            }

            model: activityModel

            delegate: RowLayout {
                id: activityItem
                width: activityListView.width
                height: trayWindowHeaderLayout.height
                spacing: 0
                visible: (activityListView.model.rowCount() > 0)

                Image {
                    id: activityIcon
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    Layout.preferredWidth: activityButton1.icon.width
                    Layout.preferredHeight: activityButton1.icon.height
                    verticalAlignment: Qt.AlignCenter
                    cache: true
                    source: icon
                    sourceSize.height: activityButton1.icon.height
                    sourceSize.width: activityButton1.icon.width
                }
                Column {
                    id: activityTextColumn
                    spacing: 4
                    Layout.alignment: Qt.AlignLeft
                    Text {
                        id: activityTextTitle
                        text: (type === "Activity") ? subject : message
                        width: 236
                        elide: Text.ElideRight
                        font.pixelSize: 12
                    }

                    Text {
                        id: activityTextInfo
                        text: displaypath
                        height: (displaypath === "") ? 0 : activityTextTitle.height
                        width: 236
                        elide: Text.ElideRight
                        font.pixelSize: 10
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
                    visible: (path !== "") ? true : false
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:///client/resources/files.svg"
                    icon.color: "transparent"

                    onClicked: {
                         Qt.openUrlExternally(path)
                    }
                }
                Button {
                    Layout.preferredWidth: activityItem.height
                    Layout.preferredHeight: activityItem.height
                    Layout.alignment: Qt.AlignRight
                    flat: true
                    hoverEnabled: false
                    visible: (link !== "") ? true : false
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:///client/resources/public.svg"
                    icon.color: "transparent"

                    onClicked: {
                        Qt.openUrlExternally(link)
                    }
                }
            }

            populate: Transition {
                // prevent animations on initial list population
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
        }

    }       // Rectangle trayWindowBackground
}
