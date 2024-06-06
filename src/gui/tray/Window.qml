/*
 * Copyright (C) 2020 by Dominique Fuchs <32204802+DominiqueFuchs@users.noreply.github.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15
import Qt.labs.platform 1.1 as NativeDialogs

import "../"
import "../filedetails/"

// Custom qml modules are in /theme (and included by resources.qrc)
import Style 1.0

import com.nextcloud.desktopclient 1.0

ApplicationWindow {
    id:         trayWindow

    title:      Systray.windowTitle
    // If the main dialog is displayed as a regular window we want it to be quadratic
    width:      Systray.useNormalWindow ? Style.trayWindowHeight : Style.trayWindowWidth
    height:     Style.trayWindowHeight
    color:      "transparent"
    flags:      Systray.useNormalWindow ? Qt.Window : Qt.Dialog | Qt.FramelessWindowHint

    // TODO: Rather than setting all these palette colours manually,
    // create a custom style and do it for all components globally
    palette {
        text: Style.ncTextColor
        windowText: Style.ncTextColor
        buttonText: Style.ncTextColor
        brightText: Style.ncTextBrightColor
        highlight: Style.lightHover
        highlightedText: Style.ncTextColor
        light: Style.lightHover
        midlight: Style.ncSecondaryTextColor
        mid: Style.darkerHover
        dark: Style.menuBorder
        button: Style.buttonBackgroundColor
        window: Style.backgroundColor
        base: Style.backgroundColor
        toolTipBase: Style.backgroundColor
        toolTipText: Style.ncTextColor
    }

    readonly property int maxMenuHeight: Style.trayWindowHeight - Style.trayWindowHeaderHeight - 2 * Style.trayWindowBorderWidth

    Component.onCompleted: Systray.forceWindowInit(trayWindow)

    // Close tray window when focus is lost (e.g. click somewhere else on the screen)
    onActiveChanged: {
        if (!Systray.useNormalWindow && !active) {
            hide();
            Systray.isOpen = false;
        }
    }

    onClosing: Systray.isOpen = false

    onVisibleChanged: {
        // HACK: reload account Instantiator immediately by restting it - could be done better I guess
        // see also id:accountMenu below
        userLineInstantiator.active = false;
        userLineInstantiator.active = true;
        syncStatus.model.load();
    }

    background: Rectangle {
        radius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius
        border.width: Style.trayWindowBorderWidth
        border.color: palette.dark
        color: palette.window
    }

    Connections {
        target: UserModel
        function onCurrentUserChanged() {
            accountMenu.close();
            syncStatus.model.load();
        }
    }

    Component {
        id: errorMessageDialog

        NativeDialogs.MessageDialog {
            id: dialog

            title: Systray.windowTitle

            onAccepted: destroy()
            onRejected: destroy()
        }
    }

    Connections {
        target: Systray

        function onIsOpenChanged() {
            userStatusDrawer.close()
            fileDetailsDrawer.close();

            if(Systray.isOpen) {
                accountMenu.close();
                appsMenu.close();
                openLocalFolderButton.closeMenu()
            }
        }

        function onShowErrorMessageDialog(error) {
            var newErrorDialog = errorMessageDialog.createObject(trayWindow)
            newErrorDialog.text = error
            newErrorDialog.open()
        }

        function onShowFileDetails(accountState, localPath, fileDetailsPage) {
            fileDetailsDrawer.openFileDetails(accountState, localPath, fileDetailsPage);
        }
    }

    OpacityMask {
        anchors.fill: parent
        anchors.margins: Style.trayWindowBorderWidth
        source: ShaderEffectSource {
            sourceItem: trayWindowMainItem
            hideSource: true
        }
        maskSource: Rectangle {
            width: trayWindow.width
            height: trayWindow.height
            radius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius
        }
    }

    Drawer {
        id: userStatusDrawer
        width: parent.width
        height: parent.height - Style.trayDrawerMargin
        padding: 0
        edge: Qt.BottomEdge
        modal: true
        visible: false

        background: Rectangle {
            radius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius
            border.width: Style.trayWindowBorderWidth
            border.color: palette.dark
            color: palette.window
        }

        property int userIndex: 0

        function openUserStatusDrawer(index) {
            console.log(`About to show dialog for user with index ${index}`);
            userIndex = index;
            open();
        }

        Loader {
            id: userStatusContents
            anchors.fill: parent
            active: userStatusDrawer.visible
            sourceComponent: UserStatusSelectorPage {
                anchors.fill: parent
                userIndex: userStatusDrawer.userIndex
                onFinished: userStatusDrawer.close()
            }
        }
    }

    Drawer {
        id: fileDetailsDrawer
        width: parent.width - Style.trayDrawerMargin
        height: parent.height
        padding: 0
        edge: Qt.RightEdge
        modal: true
        visible: false
        clip: true

        background: Rectangle {
            radius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius
            border.width: Style.trayWindowBorderWidth
            border.color: palette.dark
            color: palette.window
        }

        property var folderAccountState: ({})
        property string fileLocalPath: ""
        property var pageToShow: Systray.FileDetailsPage.Activity

        function openFileDetails(accountState, localPath, fileDetailsPage) {
            console.log(`About to show file details view in tray for ${localPath}`);
            folderAccountState = accountState;
            fileLocalPath = localPath;
            pageToShow = fileDetailsPage;

            if(!opened) {
                open();
            }
        }

        Loader {
            id: fileDetailsContents
            anchors.fill: parent
            active: fileDetailsDrawer.visible
            onActiveChanged: {
                if (active) {
                    Systray.showFileDetailsPage(fileDetailsDrawer.fileLocalPath,
                                                fileDetailsDrawer.pageToShow);
                }
            }
            sourceComponent: FileDetailsView {
                id: fileDetails

                width: parent.width
                height: parent.height

                backgroundsVisible: false
                accentColor: Style.currentUserHeaderColor
                accountState: fileDetailsDrawer.folderAccountState
                localPath: fileDetailsDrawer.fileLocalPath
                showCloseButton: true

                onCloseButtonClicked: fileDetailsDrawer.close()
            }
        }
    }

    Item {
        id: trayWindowMainItem

        property bool isUnifiedSearchActive: unifiedSearchResultsListViewSkeletonLoader.active
                                             || unifiedSearchResultNothingFound.visible
                                             || unifiedSearchResultsErrorLabel.visible
                                             || unifiedSearchResultsListView.visible

        anchors.fill: parent
        anchors.margins: Style.trayWindowBorderWidth
        clip: true

        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Nextcloud desktop main dialog")

        Rectangle {
            id: trayWindowHeaderBackground

            anchors.left:   trayWindowMainItem.left
            anchors.right:  trayWindowMainItem.right
            anchors.top:    trayWindowMainItem.top
            height:         Style.trayWindowHeaderHeight
            color:          Style.currentUserHeaderColor

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

                    Accessible.role: Accessible.ButtonMenu
                    Accessible.name: qsTr("Current account")
                    Accessible.onPressAction: currentAccountButton.clicked()

                    // We call open() instead of popup() because we want to position it
                    // exactly below the dropdown button, not the mouse
                    onClicked: {
                        syncPauseButton.text = Systray.syncIsPaused ? qsTr("Resume sync for all") : qsTr("Pause sync for all")
                        if (accountMenu.visible) {
                            accountMenu.close()
                        } else {
                            accountMenu.open()
                        }
                    }

                    Menu {
                        id: accountMenu

                        // x coordinate grows towards the right
                        // y coordinate grows towards the bottom
                        x: (currentAccountButton.x + 2)
                        y: (currentAccountButton.y + Style.trayWindowHeaderHeight + 2)

                        width: (Style.currentAccountButtonWidth - 2)
                        height: Math.min(implicitHeight, maxMenuHeight)
                        closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                        background: Rectangle {
                            border.color: palette.dark
                            color: palette.base
                            radius: Style.currentAccountButtonRadius
                        }

                        contentItem: ScrollView {
                            id: accMenuScrollView
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            data: WheelHandler {
                                target: accMenuScrollView.contentItem
                            }
                            ListView {
                                implicitHeight: contentHeight
                                model: accountMenu.contentModel
                                interactive: true
                                clip: true
                                currentIndex: accountMenu.currentIndex
                            }
                        }

                        onClosed: {
                            // HACK: reload account Instantiator immediately by restting it - could be done better I guess
                            // see also onVisibleChanged above
                            userLineInstantiator.active = false;
                            userLineInstantiator.active = true;
                        }

                        Instantiator {
                            id: userLineInstantiator
                            model: UserModel
                            delegate: UserLine {
                                onShowUserStatusSelector: {
                                    userStatusDrawer.openUserStatusDrawer(model.index);
                                    accountMenu.close();
                                }
                                onClicked: UserModel.currentUserId = model.index;
                            }
                            onObjectAdded: accountMenu.insertItem(index, object)
                            onObjectRemoved: accountMenu.removeItem(object)
                        }

                        MenuItem {
                            id: addAccountButton
                            height: Systray.enableAddAccount ? Style.addAccountButtonHeight : 0
                            hoverEnabled: true
                            visible: Systray.enableAddAccount

                            background: Item {
                                height: parent.height
                                width: parent.menu.width
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    color: parent.parent.hovered || parent.parent.visualFocus ? palette.highlight : "transparent"
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                spacing: 0

                                Image {
                                    Layout.leftMargin: 12
                                    verticalAlignment: Qt.AlignCenter
                                    source: Theme.darkMode ? "qrc:///client/theme/white/add.svg" : "qrc:///client/theme/black/add.svg"
                                    sourceSize.width: Style.headerButtonIconSize
                                    sourceSize.height: Style.headerButtonIconSize
                                }
                                EnforcedPlainTextLabel {
                                    Layout.leftMargin: 14
                                    text: qsTr("Add account")
                                    font.pixelSize: Style.topLinePixelSize
                                }
                                // Filler on the right
                                Item {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                }
                            }
                            onClicked: UserModel.addAccount()

                            Accessible.role: Accessible.MenuItem
                            Accessible.name: qsTr("Add new account")
                            Accessible.onPressAction: addAccountButton.clicked()
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            implicitHeight: 1
                            color: palette.dark
                        }

                        MenuItem {
                            id: syncPauseButton
                            font.pixelSize: Style.topLinePixelSize
                            hoverEnabled: true
                            onClicked: Systray.syncIsPaused = !Systray.syncIsPaused

                            background: Item {
                                height: parent.height
                                width: parent.menu.width
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    color: parent.parent.hovered || parent.parent.visualFocus ? palette.highlight : "transparent"
                                }
                            }

                            Accessible.role: Accessible.MenuItem
                            Accessible.name: Systray.syncIsPaused ? qsTr("Resume sync for all") : qsTr("Pause sync for all")
                            Accessible.onPressAction: syncPauseButton.clicked()
                        }

                        MenuItem {
                            id: settingsButton
                            text: qsTr("Settings")
                            font.pixelSize: Style.topLinePixelSize
                            hoverEnabled: true
                            onClicked: Systray.openSettings()

                            background: Item {
                                height: parent.height
                                width: parent.menu.width
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    color: parent.parent.hovered || parent.parent.visualFocus ? palette.highlight : "transparent"
                                }
                            }

                            Accessible.role: Accessible.MenuItem
                            Accessible.name: text
                            Accessible.onPressAction: settingsButton.clicked()
                        }

                        MenuItem {
                            id: exitButton
                            text: qsTr("Exit");
                            font.pixelSize: Style.topLinePixelSize
                            hoverEnabled: true
                            onClicked: Systray.shutdown()

                            background: Item {
                                height: parent.height
                                width: parent.menu.width
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    color: parent.parent.hovered || parent.parent.visualFocus ? palette.highlight : "transparent"
                                }
                            }

                            Accessible.role: Accessible.MenuItem
                            Accessible.name: text
                            Accessible.onPressAction: exitButton.clicked()
                        }
                    }

                    background: Rectangle {
                        color: parent.hovered || parent.visualFocus ? Style.currentUserHeaderTextColor : "transparent"
                        opacity: 0.2
                    }

                    RowLayout {
                        id: accountControlRowLayout

                        height: Style.trayWindowHeaderHeight
                        width:  Style.currentAccountButtonWidth
                        spacing: 0

                        Image {
                            id: currentAccountAvatar

                            Layout.leftMargin: Style.trayHorizontalMargin
                            verticalAlignment: Qt.AlignCenter
                            cache: false
                            source: UserModel.currentUser.avatar != "" ? UserModel.currentUser.avatar : "image://avatars/fallbackWhite"
                            Layout.preferredHeight: Style.accountAvatarSize
                            Layout.preferredWidth: Style.accountAvatarSize

                            Accessible.role: Accessible.Graphic
                            Accessible.name: qsTr("Current account avatar")

                            Rectangle {
                                id: currentAccountStatusIndicatorBackground
                                visible: UserModel.currentUser.isConnected
                                         && UserModel.currentUser.serverHasUserStatus
                                width: Style.accountAvatarStateIndicatorSize +  + Style.trayFolderStatusIndicatorSizeOffset
                                height: width
                                anchors.bottom: currentAccountAvatar.bottom
                                anchors.right: currentAccountAvatar.right
                                color: Style.currentUserHeaderColor
                                radius: width * Style.trayFolderStatusIndicatorRadiusFactor
                            }

                            Rectangle {
                                id: currentAccountStatusIndicatorMouseHover
                                visible: UserModel.currentUser.isConnected
                                         && UserModel.currentUser.serverHasUserStatus
                                width: Style.accountAvatarStateIndicatorSize +  + Style.trayFolderStatusIndicatorSizeOffset
                                height: width
                                anchors.bottom: currentAccountAvatar.bottom
                                anchors.right: currentAccountAvatar.right
                                color: currentAccountButton.hovered ? Style.currentUserHeaderTextColor : "transparent"
                                opacity: Style.trayFolderStatusIndicatorMouseHoverOpacityFactor
                                radius: width * Style.trayFolderStatusIndicatorRadiusFactor
                            }

                            Image {
                                id: currentAccountStatusIndicator
                                visible: UserModel.currentUser.isConnected
                                         && UserModel.currentUser.serverHasUserStatus
                                source: UserModel.currentUser.statusIcon
                                cache: false
                                x: currentAccountStatusIndicatorBackground.x + 1
                                y: currentAccountStatusIndicatorBackground.y + 1
                                sourceSize.width: Style.accountAvatarStateIndicatorSize
                                sourceSize.height: Style.accountAvatarStateIndicatorSize

                                Accessible.role: Accessible.Indicator
                                Accessible.name: UserModel.desktopNotificationsAllowed ? qsTr("Current account status is online") : qsTr("Current account status is do not disturb")
                            }
                        }

                        Column {
                            id: accountLabels
                            spacing: 0
                            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                            Layout.leftMargin: Style.userStatusSpacing
                            Layout.fillWidth: true
                            Layout.maximumWidth: parent.width

                            EnforcedPlainTextLabel {
                                id: currentAccountUser
                                Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                                width: Style.currentAccountLabelWidth
                                text: UserModel.currentUser.name
                                elide: Text.ElideRight
                                color: Style.currentUserHeaderTextColor

                                font.pixelSize: Style.topLinePixelSize
                                font.bold: true
                            }

                            EnforcedPlainTextLabel {
                                id: currentAccountServer
                                Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                                width: Style.currentAccountLabelWidth
                                text: UserModel.currentUser.server
                                elide: Text.ElideRight
                                color: Style.currentUserHeaderTextColor
                                visible: UserModel.numUsers() > 1
                            }

                            RowLayout {
                                id: currentUserStatus
                                visible: UserModel.currentUser.isConnected &&
                                         UserModel.currentUser.serverHasUserStatus
                                spacing: Style.accountLabelsSpacing
                                width: parent.width

                                EnforcedPlainTextLabel {
                                    id: emoji
                                    visible: UserModel.currentUser.statusEmoji !== ""
                                    width: Style.userStatusEmojiSize
                                    text: UserModel.currentUser.statusEmoji
                                }
                                EnforcedPlainTextLabel {
                                    id: message
                                    Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                                    Layout.fillWidth: true
                                    visible: UserModel.currentUser.statusMessage !== ""
                                    width: Style.currentAccountLabelWidth
                                    text: UserModel.currentUser.statusMessage !== ""
                                          ? UserModel.currentUser.statusMessage
                                          : UserModel.currentUser.server
                                    elide: Text.ElideRight
                                    color: Style.currentUserHeaderTextColor
                                    font.pixelSize: Style.subLinePixelSize
                                }
                            }
                        }

                        ColorOverlay {
                            cached: true
                            color: Style.currentUserHeaderTextColor
                            width: source.width
                            height: source.height
                            source: Image {
                                Layout.alignment: Qt.AlignRight
                                verticalAlignment: Qt.AlignCenter
                                Layout.margins: Style.accountDropDownCaretMargin
                                source: "qrc:///client/theme/white/caret-down.svg"
                                sourceSize.width: Style.accountDropDownCaretSize
                                sourceSize.height: Style.accountDropDownCaretSize
                                Accessible.role: Accessible.PopupMenu
                                Accessible.name: qsTr("Account switcher and settings menu")
                            }
                        }
                    }
                }

                // Add space between items
                Item {
                    Layout.fillWidth: true
                }

                TrayFoldersMenuButton {
                    id: openLocalFolderButton

                    visible: currentUser.hasLocalFolder
                    currentUser: UserModel.currentUser


                    onClicked: openLocalFolderButton.userHasGroupFolders ? openLocalFolderButton.toggleMenuOpen() : UserModel.openCurrentAccountLocalFolder()

                    onFolderEntryTriggered: isGroupFolder ? UserModel.openCurrentAccountFolderFromTrayInfo(fullFolderPath) : UserModel.openCurrentAccountLocalFolder()

                    Accessible.role: Accessible.Graphic
                    Accessible.name: qsTr("Open local or group folders")
                    Accessible.onPressAction: openLocalFolderButton.userHasGroupFolders ? openLocalFolderButton.toggleMenuOpen() : UserModel.openCurrentAccountLocalFolder()

                    Layout.alignment: Qt.AlignRight
                    Layout.preferredWidth:  Style.trayWindowHeaderHeight
                    Layout.preferredHeight: Style.trayWindowHeaderHeight
                }

                HeaderButton {
                    id: trayWindowTalkButton

                    visible: UserModel.currentUser && UserModel.currentUser.serverHasTalk
                    icon.source: "image://svgimage-custom-color/talk-app.svg" + "/" + Style.currentUserHeaderTextColor
      
                    onClicked: UserModel.openCurrentAccountTalk()

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Open Nextcloud Talk in browser")
                    Accessible.onPressAction: trayWindowTalkButton.clicked()

                    Layout.alignment: Qt.AlignRight
                    Layout.preferredWidth:  Style.trayWindowHeaderHeight
                    Layout.preferredHeight: Style.trayWindowHeaderHeight

                }

                HeaderButton {
                    id: trayWindowAppsButton
                    icon.source: "image://svgimage-custom-color/more-apps.svg" + "/" + Style.currentUserHeaderTextColor

                    onClicked: {
                        if(appsMenuListView.count <= 0) {
                            UserModel.openCurrentAccountServer()
                        } else if (appsMenu.visible) {
                            appsMenu.close()
                        } else {
                            appsMenu.open()
                        }
                    }

                    Accessible.role: Accessible.ButtonMenu
                    Accessible.name: qsTr("More apps")
                    Accessible.onPressAction: trayWindowAppsButton.clicked()

                    Menu {
                        id: appsMenu
                        x: Style.trayWindowMenuOffsetX
                        y: (trayWindowAppsButton.y + trayWindowAppsButton.height + Style.trayWindowMenuOffsetY)
                        width: Style.trayWindowWidth * Style.trayWindowMenuWidthFactor
                        height: implicitHeight + y > Style.trayWindowHeight ? Style.trayWindowHeight - y : implicitHeight
                        closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                        background: Rectangle {
                            border.color: palette.dark
                            color: palette.base
                            radius: 2
                        }

                        contentItem: ScrollView {
                            id: appsMenuScrollView
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            data: WheelHandler {
                                target: appsMenuScrollView.contentItem
                            }
                            ListView {
                                id: appsMenuListView
                                implicitHeight: contentHeight
                                model: UserAppsModel
                                interactive: true
                                clip: true
                                currentIndex: appsMenu.currentIndex
                                delegate: MenuItem {
                                    id: appEntry
                                    anchors.left: parent.left
                                    anchors.right: parent.right

                                    text: model.appName
                                    font.pixelSize: Style.topLinePixelSize
                                    icon.source: model.appIconUrl
                                    icon.color: palette.buttonText
                                    onTriggered: UserAppsModel.openAppUrl(appUrl)
                                    hoverEnabled: true

                                    background: Item {
                                        height: parent.height
                                        width: parent.width
                                        Rectangle {
                                            anchors.fill: parent
                                            anchors.margins: 1
                                            color: parent.parent.hovered || parent.parent.visualFocus ? palette.highlight : "transparent"
                                        }
                                    }

                                    Accessible.role: Accessible.MenuItem
                                    Accessible.name: qsTr("Open %1 in browser").arg(model.appName)
                                    Accessible.onPressAction: appEntry.triggered()
                                }
                            }
                        }
                    }
                }
            }
        }   // Rectangle trayWindowHeaderBackground

        UnifiedSearchInputContainer {
            id: trayWindowUnifiedSearchInputContainer
            height: Style.trayWindowHeaderHeight * 0.65

            anchors {
                top: trayWindowHeaderBackground.bottom
                left: trayWindowMainItem.left
                right: trayWindowMainItem.right

                topMargin: Style.trayHorizontalMargin + controlRoot.padding
                leftMargin: Style.trayHorizontalMargin + controlRoot.padding
                rightMargin: Style.trayHorizontalMargin + controlRoot.padding
            }

            text: UserModel.currentUser.unifiedSearchResultsListModel.searchTerm
            readOnly: !UserModel.currentUser.isConnected || UserModel.currentUser.unifiedSearchResultsListModel.currentFetchMoreInProgressProviderId
            isSearchInProgress: UserModel.currentUser.unifiedSearchResultsListModel.isSearchInProgress
            onTextEdited: { UserModel.currentUser.unifiedSearchResultsListModel.searchTerm = trayWindowUnifiedSearchInputContainer.text }
            onClearText: { UserModel.currentUser.unifiedSearchResultsListModel.searchTerm = "" }
        }

        ErrorBox {
            id: unifiedSearchResultsErrorLabel
            visible:  UserModel.currentUser.unifiedSearchResultsListModel.errorString && !unifiedSearchResultsListView.visible && ! UserModel.currentUser.unifiedSearchResultsListModel.isSearchInProgress && ! UserModel.currentUser.unifiedSearchResultsListModel.currentFetchMoreInProgressProviderId
            text:  UserModel.currentUser.unifiedSearchResultsListModel.errorString
            anchors.top: trayWindowUnifiedSearchInputContainer.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
            anchors.margins: Style.trayHorizontalMargin
        }

        UnifiedSearchResultNothingFound {
            id: unifiedSearchResultNothingFound

            anchors.top: trayWindowUnifiedSearchInputContainer.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
            anchors.topMargin: Style.trayHorizontalMargin

            text: UserModel.currentUser.unifiedSearchResultsListModel.searchTerm

            property bool isSearchRunning: UserModel.currentUser.unifiedSearchResultsListModel.isSearchInProgress
            property bool waitingForSearchTermEditEnd: UserModel.currentUser.unifiedSearchResultsListModel.waitingForSearchTermEditEnd
            property bool isSearchResultsEmpty: unifiedSearchResultsListView.count === 0
            property bool nothingFound: text && isSearchResultsEmpty && !UserModel.currentUser.unifiedSearchResultsListModel.errorString

            visible: !isSearchRunning && !waitingForSearchTermEditEnd && nothingFound
        }

        Loader {
            id: unifiedSearchResultsListViewSkeletonLoader

            anchors.top: trayWindowUnifiedSearchInputContainer.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
            anchors.bottom: trayWindowMainItem.bottom
            anchors.margins: controlRoot.padding

            active: !unifiedSearchResultNothingFound.visible &&
                    !unifiedSearchResultsListView.visible &&
                    !UserModel.currentUser.unifiedSearchResultsListModel.errorString &&
                    UserModel.currentUser.unifiedSearchResultsListModel.searchTerm

            sourceComponent: UnifiedSearchResultItemSkeletonContainer {
                anchors.fill: parent
                spacing: unifiedSearchResultsListView.spacing
                animationRectangleWidth: trayWindow.width
            }
        }

        ScrollView {
            id: controlRoot
            contentWidth: availableWidth

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            data: WheelHandler {
                target: controlRoot.contentItem
            }
            visible: unifiedSearchResultsListView.count > 0

            anchors.top: trayWindowUnifiedSearchInputContainer.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
            anchors.bottom: trayWindowMainItem.bottom

            ListView {
                id: unifiedSearchResultsListView
                spacing: 4
                clip: true

                keyNavigationEnabled: true

                reuseItems: true

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Unified search results list")

                model: UserModel.currentUser.unifiedSearchResultsListModel

                delegate: UnifiedSearchResultListItem {
                    width: unifiedSearchResultsListView.width
                    isSearchInProgress:  unifiedSearchResultsListView.model.isSearchInProgress
                    currentFetchMoreInProgressProviderId: unifiedSearchResultsListView.model.currentFetchMoreInProgressProviderId
                    fetchMoreTriggerClicked: unifiedSearchResultsListView.model.fetchMoreTriggerClicked
                    resultClicked: unifiedSearchResultsListView.model.resultClicked
                    ListView.onPooled: isPooled = true
                    ListView.onReused: isPooled = false
                }

                section.property: "providerName"
                section.criteria: ViewSection.FullString
                section.delegate: UnifiedSearchResultSectionItem {
                    width: unifiedSearchResultsListView.width
                }
            }
        }

        SyncStatus {
            id: syncStatus

            visible: !trayWindowMainItem.isUnifiedSearchActive

            anchors.top: trayWindowUnifiedSearchInputContainer.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
        }

        Loader {
            id: newActivitiesButtonLoader

            anchors.top: activityList.top
            anchors.topMargin: 5
            anchors.horizontalCenter: activityList.horizontalCenter

            width: Style.newActivitiesButtonWidth
            height: Style.newActivitiesButtonHeight

            z: 1

            active: false

            sourceComponent: CustomButton {
                id: newActivitiesButton
                hoverEnabled: true
                padding: Style.smallSpacing

                textColor: Style.currentUserHeaderTextColor
                textColorHovered: Style.currentUserHeaderTextColor
                contentsFont.bold: true
                bgNormalColor: Qt.lighter(bgHoverColor, 1.25)
                bgHoverColor: Style.currentUserHeaderColor
                bgNormalOpacity: Style.newActivitiesBgNormalOpacity
                bgHoverOpacity: Style.newActivitiesBgHoverOpacity

                anchors.fill: parent

                text: qsTr("New activities")

                icon.source: "image://svgimage-custom-color/expand-less-black.svg" + "/" + Style.currentUserHeaderTextColor
                icon.width: Style.activityLabelBaseWidth
                icon.height: Style.activityLabelBaseWidth

                onClicked: {
                    activityList.scrollToTop();
                    newActivitiesButtonLoader.active = false
                }

                Timer {
                    id: newActivitiesButtonDisappearTimer
                    interval: Style.newActivityButtonDisappearTimeout
                    running: newActivitiesButtonLoader.active && !newActivitiesButton.hovered
                    repeat: false
                    onTriggered: fadeoutActivitiesButtonDisappear.running = true
                }

                OpacityAnimator {
                    id: fadeoutActivitiesButtonDisappear
                    target: newActivitiesButton;
                    from: 1;
                    to: 0;
                    duration: Style.newActivityButtonDisappearFadeTimeout
                    loops: 1
                    running: false
                    onFinished: newActivitiesButtonLoader.active = false
                }
            }
        }

        ActivityList {
            id: activityList
            visible: !trayWindowMainItem.isUnifiedSearchActive
            anchors.top: syncStatus.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
            anchors.bottom: trayWindowMainItem.bottom

            activeFocusOnTab: true
            model: activityModel
            onOpenFile: Qt.openUrlExternally(filePath);
            onActivityItemClicked: {
                model.slotTriggerDefaultAction(index)
            }
            Connections {
                target: activityModel
                onInteractiveActivityReceived: {
                    if (!activityList.atYBeginning) {
                        newActivitiesButtonLoader.active = true;
                    }
                }
            }
        }
    } // Item trayWindowMainItem
}
