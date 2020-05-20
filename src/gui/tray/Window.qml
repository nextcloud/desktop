import QtQml 2.1
import QtQml.Models 2.1
import QtQuick 2.9
import QtQuick.Window 2.3
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.0

// Custom qml modules are in /theme (and included by resources.qrc)
import Style 1.0

Window {
    id:         trayWindow

    width:      Style.trayWindowWidth
    height:     Style.trayWindowHeight
    color:      "transparent"
    flags:      Qt.Dialog | Qt.FramelessWindowHint

    // Close tray window when focus is lost (e.g. click somewhere else on the screen)
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
        openLocalFolderButton.visible = userModelBackend.currentUserHasLocalFolder();
        trayWindowTalkButton.visible = userModelBackend.currentServerHasTalk() ? true : false;
        currentAccountStateIndicator.source = ""
        currentAccountStateIndicator.source = userModelBackend.isUserConnected(userModelBackend.currentUserId()) ? "qrc:///client/theme/colored/state-ok.svg" : "qrc:///client/theme/colored/state-offline.svg"

        // HACK: reload account Instantiator immediately by restting it - could be done better I guess
        // see also id:accountMenu below
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
            openLocalFolderButton.visible = userModelBackend.currentUserHasLocalFolder();
            trayWindowTalkButton.visible = userModelBackend.currentServerHasTalk() ? true : false;
        }
    }

    Connections {
        target: systrayBackend
        onShowWindow: {
            accountMenu.close();

            trayWindow.screen = systrayBackend.currentScreenVar();

            var position = systrayBackend.computeWindowPosition(trayWindow.width, trayWindow.height)
            trayWindow.x = position.x
            trayWindow.y = position.y

            trayWindow.show();
            trayWindow.raise();
            trayWindow.requestActivate();

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

        anchors.fill:   parent
        radius:         Style.trayWindowRadius
        border.width:   Style.trayWindowBorderWidth
        border.color:   Style.ncBlue

        Rectangle {
            id: trayWindowHeaderBackground

            anchors.left:   trayWindowBackground.left
            anchors.top:    trayWindowBackground.top
            height:         Style.trayWindowHeaderHeight
            width:          Style.trayWindowWidth
            radius:         (Style.trayWindowRadius > 0) ? (Style.trayWindowRadius - 1) : 0
            color:          Style.ncBlue

            // The overlay rectangle below eliminates the rounded corners from the bottom of the header
            // as Qt only allows setting the radius for all corners right now, not specific ones
            Rectangle {
                id: trayWindowHeaderButtomHalfBackground

                anchors.left:   trayWindowHeaderBackground.left
                anchors.bottom: trayWindowHeaderBackground.bottom
                height:         Style.trayWindowHeaderHeight / 2
                width:          Style.trayWindowWidth
                color:          Style.ncBlue
            }

            RowLayout {
                id: trayWindowHeaderLayout

                spacing:        0
                anchors.fill:   parent

                Button {
                    id: currentAccountButton

                    Layout.preferredWidth:  Style.currentAccountButtonWidth
                    Layout.preferredHeight: Style.trayWindowHeaderHeight
                    display:                AbstractButton.IconOnly
                    flat:                   true

                    MouseArea {
                        id: accountBtnMouseArea

                        anchors.fill:   parent
                        hoverEnabled:   Style.hoverEffectsEnabled

                        // HACK: Imitate Qt hover effect brightness (which is not accessible as property)
                        // so that indicator background also flicks when hovered
                        onContainsMouseChanged: {
                            currentAccountStateIndicatorBackground.color = (containsMouse ? Style.ncBlueHover : Style.ncBlue)
                        }

                        // We call open() instead of popup() because we want to position it
                        // exactly below the dropdown button, not the mouse
                        onClicked:
                        {
                            syncPauseButton.text = systrayBackend.syncIsPaused() ? qsTr("Resume sync for all") : qsTr("Pause sync for all")
                            accountMenu.open()
                        }

                        Menu {
                            id: accountMenu

                            // x coordinate grows towards the right
                            // y coordinate grows towards the bottom
                            x: (currentAccountButton.x + 2)
                            y: (currentAccountButton.y + Style.trayWindowHeaderHeight + 2)

                            width: (Style.currentAccountButtonWidth - 2)
                            closePolicy: "CloseOnPressOutside"

                            background: Rectangle {
                                border.color: Style.ncBlue
                                radius: Style.currentAccountButtonRadius
                            }

                            onClosed: {
                                // HACK: reload account Instantiator immediately by restting it - could be done better I guess
                                // see also onVisibleChanged above
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
                                height: Style.addAccountButtonHeight

                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 0

                                    Image {
                                        Layout.leftMargin: 12
                                        verticalAlignment: Qt.AlignCenter
                                        source: "qrc:///client/theme/black/add.svg"
                                        sourceSize.width: Style.headerButtonIconSize
                                        sourceSize.height: Style.headerButtonIconSize
                                    }
                                    Label {
                                        Layout.leftMargin: 14
                                        text: qsTr("Add account")
                                        color: "black"
                                        font.pixelSize: Style.topLinePixelSize
                                    }
                                    // Filler on the right
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
                                font.pixelSize: Style.topLinePixelSize
                                onClicked: systrayBackend.pauseResumeSync()
                            }

                            MenuItem {
                                text: qsTr("Open settings")
                                font.pixelSize: Style.topLinePixelSize
                                onClicked: systrayBackend.openSettings()
                            }

                            MenuItem {
                                text: qsTr("Help")
                                font.pixelSize: Style.topLinePixelSize
                                onClicked: systrayBackend.openHelp()
                            }

                            MenuItem {
                                text: qsTr("Quit Nextcloud")
                                font.pixelSize: Style.topLinePixelSize
                                onClicked: systrayBackend.shutdown()
                            }
                        }
                    }

                    background:
                        Item {
                        id: leftHoverContainer

                        height: Style.trayWindowHeaderHeight
                        width:  Style.currentAccountButtonWidth
                        Rectangle {
                            width: Style.currentAccountButtonWidth / 2
                            height: Style.trayWindowHeaderHeight / 2
                            color: "transparent"
                            clip: true
                            Rectangle {
                                width: Style.currentAccountButtonWidth
                                height: Style.trayWindowHeaderHeight
                                radius: Style.trayWindowRadius
                                color: "white"
                                opacity: 0.2
                                visible: accountBtnMouseArea.containsMouse
                            }
                        }
                        Rectangle {
                            width: Style.currentAccountButtonWidth / 2
                            height: Style.trayWindowHeaderHeight / 2
                            anchors.bottom: leftHoverContainer.bottom
                            color: "white"
                            opacity: 0.2
                            visible: accountBtnMouseArea.containsMouse
                        }
                        Rectangle {
                            width: Style.currentAccountButtonWidth / 2
                            height: Style.trayWindowHeaderHeight / 2
                            anchors.right: leftHoverContainer.right
                            color: "white"
                            opacity: 0.2
                            visible: accountBtnMouseArea.containsMouse
                        }
                        Rectangle {
                            width: Style.currentAccountButtonWidth / 2
                            height: Style.trayWindowHeaderHeight / 2
                            anchors.right: leftHoverContainer.right
                            anchors.bottom: leftHoverContainer.bottom
                            color: "white"
                            opacity: 0.2
                            visible: accountBtnMouseArea.containsMouse
                        }
                    }

                    RowLayout {
                        id: accountControlRowLayout

                        height: Style.trayWindowHeaderHeight
                        width:  Style.currentAccountButtonWidth
                        spacing: 0
                        Image {
                            id: currentAccountAvatar

                            Layout.leftMargin: 8
                            verticalAlignment: Qt.AlignCenter
                            cache: false
                            source: "image://avatars/currentUser"
                            Layout.preferredHeight: Style.accountAvatarSize
                            Layout.preferredWidth: Style.accountAvatarSize

                            Rectangle {
                                id: currentAccountStateIndicatorBackground
                                width: Style.accountAvatarStateIndicatorSize + 2
                                height: width
                                anchors.bottom: currentAccountAvatar.bottom
                                anchors.right: currentAccountAvatar.right
                                color: Style.ncBlue
                                radius: width*0.5
                            }

                            Image {
                                id: currentAccountStateIndicator
                                source: userModelBackend.isUserConnected(userModelBackend.currentUserId()) ? "qrc:///client/theme/colored/state-ok.svg" : "qrc:///client/theme/colored/state-offline.svg"
                                cache: false
                                x: currentAccountStateIndicatorBackground.x + 1
                                y: currentAccountStateIndicatorBackground.y + 1
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
                                id: currentAccountUser
                                width: Style.currentAccountLabelWidth
                                text: userModelBackend.currentUserName()
                                elide: Text.ElideRight
                                color: "white"
                                font.pixelSize: Style.topLinePixelSize
                                font.bold: true
                            }
                            Label {
                                id: currentAccountServer
                                width: Style.currentAccountLabelWidth
                                text: userModelBackend.currentUserServer()
                                elide: Text.ElideRight
                                color: "white"
                                font.pixelSize: Style.subLinePixelSize
                            }
                        }

                        Image {
                            Layout.alignment: Qt.AlignRight
                            verticalAlignment: Qt.AlignCenter
                            Layout.margins: Style.accountDropDownCaretMargin
                            source: "qrc:///client/theme/white/caret-down.svg"
                            sourceSize.width: Style.accountDropDownCaretSize
                            sourceSize.height: Style.accountDropDownCaretSize
                        }
                    }
                }

                // Filler between account dropdown and header app buttons
                Item {
                    id: trayWindowHeaderSpacer
                    Layout.fillWidth: true
                }

                Button {
                    id: openLocalFolderButton

                    Layout.alignment: Qt.AlignRight
                    display: AbstractButton.IconOnly
                    Layout.preferredWidth:  Style.trayWindowHeaderHeight
                    Layout.preferredHeight: Style.trayWindowHeaderHeight
                    flat: true
                    visible: userModelBackend.currentUserHasLocalFolder()

                    icon.source: "qrc:///client/theme/white/folder.svg"
                    icon.width: Style.headerButtonIconSize
                    icon.height: Style.headerButtonIconSize
                    icon.color: "transparent"

                    MouseArea {
                        id: folderBtnMouseArea

                        anchors.fill: parent
                        hoverEnabled: Style.hoverEffectsEnabled
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
                    Layout.preferredWidth:  Style.trayWindowHeaderHeight
                    Layout.preferredHeight: Style.trayWindowHeaderHeight
                    flat: true
                    visible: userModelBackend.currentServerHasTalk() ? true : false

                    icon.source: "qrc:///client/theme/white/talk-app.svg"
                    icon.width: Style.headerButtonIconSize
                    icon.height: Style.headerButtonIconSize
                    icon.color: "transparent"

                    MouseArea {
                        id: talkBtnMouseArea

                        anchors.fill: parent
                        hoverEnabled: Style.hoverEffectsEnabled
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
                    Layout.preferredWidth:  Style.trayWindowHeaderHeight
                    Layout.preferredHeight: Style.trayWindowHeaderHeight
                    flat: true

                    icon.source: "qrc:///client/theme/white/more-apps.svg"
                    icon.width: Style.headerButtonIconSize
                    icon.height: Style.headerButtonIconSize
                    icon.color: "transparent"

                    MouseArea {
                        id: appsBtnMouseArea

                        anchors.fill: parent
                        hoverEnabled: Style.hoverEffectsEnabled
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
                            y: (trayWindowAppsButton.y + trayWindowAppsButton.height + 2)
                            width: Math.min(contentItem.childrenRect.width + 4, Style.trayWindowWidth / 2)
                            closePolicy: "CloseOnPressOutside"

                            background: Rectangle {
                                border.color: Style.ncBlue
                                radius: 2
                            }

                            Instantiator {
                                id: appsMenuInstantiator
                                model: appsMenuModelBackend
                                onObjectAdded: appsMenu.insertItem(index, object)
                                onObjectRemoved: appsMenu.removeItem(object)
                                delegate: MenuItem {
                                    text: appName
                                    font.pixelSize: Style.topLinePixelSize
                                    icon.source: appIconUrl
                                    width: contentItem.implicitWidth + leftPadding + rightPadding
                                    onTriggered: appsMenuModelBackend.openAppUrl(appUrl)
                                }
                            }
                        }
                    }

                    background:
                        Item {
                        id: rightHoverContainer
                        height: Style.trayWindowHeaderHeight
                        width: Style.trayWindowHeaderHeight
                        Rectangle {
                            width: Style.trayWindowHeaderHeight / 2
                            height: Style.trayWindowHeaderHeight / 2
                            color: "white"
                            opacity: 0.2
                            visible: appsBtnMouseArea.containsMouse
                        }
                        Rectangle {
                            width: Style.trayWindowHeaderHeight / 2
                            height: Style.trayWindowHeaderHeight / 2
                            anchors.bottom: rightHoverContainer.bottom
                            color: "white"
                            opacity: 0.2
                            visible: appsBtnMouseArea.containsMouse
                        }
                        Rectangle {
                            width: Style.trayWindowHeaderHeight / 2
                            height: Style.trayWindowHeaderHeight / 2
                            anchors.bottom: rightHoverContainer.bottom
                            anchors.right: rightHoverContainer.right
                            color: "white"
                            opacity: 0.2
                            visible: appsBtnMouseArea.containsMouse
                        }
                        Rectangle {
                            id: rightHoverContainerClipper
                            anchors.right: rightHoverContainer.right
                            width: Style.trayWindowHeaderHeight / 2
                            height: Style.trayWindowHeaderHeight / 2
                            color: "transparent"
                            clip: true
                            Rectangle {
                                width: Style.trayWindowHeaderHeight
                                height: Style.trayWindowHeaderHeight
                                anchors.right: rightHoverContainerClipper.right
                                radius: Style.trayWindowRadius
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
            anchors.horizontalCenter: trayWindowBackground.horizontalCenter
            width:  Style.trayWindowWidth - Style.trayWindowBorderWidth
            height: Style.trayWindowHeight - Style.trayWindowHeaderHeight
            clip: true
            ScrollBar.vertical: ScrollBar {
                id: listViewScrollbar
            }

            model: activityModel

            delegate: RowLayout {
                id: activityItem

                width: parent.width
                height: Style.trayWindowHeaderHeight
                spacing: 0

                MouseArea {
                    enabled: (path !== "")
                    anchors.left: activityItem.left
                    anchors.right: ((shareButton.visible) ? shareButton.left : activityItem.right)
                    height: parent.height
                    anchors.margins: 2
                    hoverEnabled: true
                    onClicked: Qt.openUrlExternally(path)
                    ToolTip.visible: hovered
                    ToolTip.delay: 1000
                    ToolTip.text: qsTr("Open sync item locally")
                    Rectangle {
                        anchors.fill: parent
                        color: (parent.containsMouse ? Style.lightHover : "transparent")
                    }
                }

                Image {
                    id: activityIcon
                    anchors.left: activityItem.left
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Layout.preferredWidth: shareButton.icon.width
                    Layout.preferredHeight: shareButton.icon.height
                    verticalAlignment: Qt.AlignCenter
                    cache: true
                    source: icon
                    sourceSize.height: 64
                    sourceSize.width: 64
                }

                Column {
                    id: activityTextColumn
                    anchors.left: activityIcon.right
                    anchors.leftMargin: 8
                    spacing: 4
                    Layout.alignment: Qt.AlignLeft
                    Text {
                        id: activityTextTitle
                        text: (type === "Activity" || type === "Notification") ? subject : message
                        width: Style.activityLabelBaseWidth + ((path === "") ? activityItem.height : 0) + ((link === "") ? activityItem.height : 0) - 8
                        elide: Text.ElideRight
                        font.pixelSize: Style.topLinePixelSize
                        color: activityTextTitleColor
                    }

                    Text {
                        id: activityTextInfo
                        text: (type === "Activity" || type === "Sync") ? displayPath
                            : (type === "File") ? subject
                            : message
                        height: (text === "") ? 0 : activityTextTitle.height
                        width: Style.activityLabelBaseWidth + ((path === "") ? activityItem.height : 0) + ((link === "") ? activityItem.height : 0) - 8
                        elide: Text.ElideRight
                        font.pixelSize: Style.subLinePixelSize
                    }

                    Text {
                        id: activityTextDateTime
                        text: dateTime
                        height: (text === "") ? 0 : activityTextTitle.height
                        width: Style.activityLabelBaseWidth + ((path === "") ? activityItem.height : 0) + ((link === "") ? activityItem.height : 0) - 8
                        elide: Text.ElideRight
                        font.pixelSize: Style.subLinePixelSize
                        color: "#808080"
                    }
                }
                Button {
                    id: shareButton
                    anchors.right: activityItem.right

                    Layout.preferredWidth: (path === "") ? 0 : parent.height
                    Layout.preferredHeight: parent.height
                    Layout.alignment: Qt.AlignRight
                    flat: true
                    hoverEnabled: true
                    visible: (path === "") ? false : true
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:///client/theme/share.svg"
                    icon.color: "transparent"
                    background: Rectangle {
                        color: parent.hovered ? Style.lightHover : "transparent"
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 1000
                    ToolTip.text: qsTr("Open share dialog")
                    onClicked: systrayBackend.openShareDialog(displayPath,absolutePath)
                }
            }

            /*add: Transition {
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
            }*/
        }

    }       // Rectangle trayWindowBackground
}
