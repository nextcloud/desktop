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
import "../SesComponents/"

// Custom qml modules are in /theme (and included by resources.qrc)
import Style 1.0

import com.ionos.hidrivenext.desktopclient 1.0

ApplicationWindow {
    id:         trayWindow

    title:      Systray.windowTitle
    // If the main dialog is displayed as a regular window we want it to be quadratic
    width:      Systray.useNormalWindow ? Style.trayWindowHeight : Style.sesTrayWindowWidth
    height:     Style.trayWindowHeight
    color:      "transparent"
    flags:      Systray.useNormalWindow ? Qt.Window : Qt.Dialog | Qt.FramelessWindowHint

    font.family: Style.sesOpenSansRegular
    font.pixelSize: Style.sesFontPixelSize
    font.weight: Style.sesFontBoldWeight

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

    onVisibleChanged: syncStatus.model.load()

    background: Rectangle {
        radius: 0.0
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
            radius: 0.0
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
            color: "red"//palette.window
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

        HeaderLogo {
            id: trayWindowLogoHeaderBackground
            height: Style.sesHeaderLogoHeigth
            width: parent.width
        }

        SesTrayHeader {
            id: trayWindowHeaderBackground
            anchors.left:   trayWindowLogoHeaderBackground.left
            anchors.right:  trayWindowLogoHeaderBackground.right
            anchors.top:    trayWindowLogoHeaderBackground.bottom
            anchors.topMargin: Style.sesHeaderTopMargin
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: trayWindowHeaderBackground.bottom
            anchors.topMargin: Style.sesHeaderTopMargin
            implicitHeight: 1
            color: Style.sesBorderColor
        }

        UnifiedSearchInputContainer {
            id: trayWindowUnifiedSearchInputContainer
            height: 0
            visible: false //SES-4 removed

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

        SesErrorBox {
            id: unifiedSearchResultsErrorLabel
            visible:    UserModel.currentUser.unifiedSearchResultsListModel.errorString &&
                        !unifiedSearchResultsListView.visible &&
                        !UserModel.currentUser.unifiedSearchResultsListModel.isSearchInProgress &&
                        !UserModel.currentUser.unifiedSearchResultsListModel.currentFetchMoreInProgressProviderId
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
