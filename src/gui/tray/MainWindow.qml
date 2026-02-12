/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import Qt.labs.platform as NativeDialogs

import "../"
import "../filedetails/"

// Custom qml modules are in /theme (and included by resources.qrc)
import Style

import com.nextcloud.desktopclient

ApplicationWindow {
    id:         trayWindow

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    title:      Systray.windowTitle
    // If the main dialog is displayed as a regular window we want it to be quadratic
    width:      Systray.useNormalWindow ? Style.trayWindowHeight : Style.trayWindowWidth
    height:     Style.trayWindowHeight
    flags:      Systray.useNormalWindow ? Qt.Window : Qt.Dialog | Qt.FramelessWindowHint
    color: "transparent"

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
        // see also id:trayWindowHeader.currentAccountHeaderButton.accountMenu below
        trayWindowHeader.currentAccountHeaderButton.userLineInstantiator.active = false;
        trayWindowHeader.currentAccountHeaderButton.userLineInstantiator.active = true;
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
            trayWindowHeader.currentAccountHeaderButton.accountMenu.close();
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

            if (Systray.isOpen) {
                trayWindowHeader.currentAccountHeaderButton.accountMenu.close();
                trayWindowHeader.appsMenu.close();
                trayWindowHeader.openLocalFolderButton.closeMenu()
                UserModel.refreshSyncErrorUsers()
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
            color: Style.colorWithoutTransparency(palette.base)
        }

        property int userIndex: 0
        property string modeSetStatus: "setStatus"
        property string modeStatusMessage: "statusMessage"
        property string initialMode: modeSetStatus

        function openUserStatusDrawer(index, mode) {
            console.log(`About to show dialog for user with index ${index}`);
            userIndex = index;
            initialMode = mode ? mode : modeSetStatus;
            open();
        }

        function openUserStatusMessageDrawer(index) {
            openUserStatusDrawer(index, modeStatusMessage);
        }

        Loader {
            id: userStatusContents
            anchors.fill: parent
            active: userStatusDrawer.visible
            sourceComponent: UserStatusSelectorPage {
                anchors.fill: parent
                userIndex: userStatusDrawer.userIndex
                mode: userStatusDrawer.initialMode
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
            color: Style.colorWithoutTransparency(palette.base)
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
                accentColor: Style.accentColor
                accountState: fileDetailsDrawer.folderAccountState
                localPath: fileDetailsDrawer.fileLocalPath
                showCloseButton: true

                onCloseButtonClicked: fileDetailsDrawer.close()
            }
        }
    }

    Rectangle {
        id: trayWindowMainItem

        property bool isUnifiedSearchActive: unifiedSearchResultsListViewSkeletonLoader.active
                                             || unifiedSearchResultNothingFound.visible
                                             || unifiedSearchResultsErrorLabel.visible
                                             || unifiedSearchResultsListView.visible
                                             || trayWindowUnifiedSearchInputContainer.activateSearchFocus

        anchors.fill: parent
        anchors.margins: Style.trayWindowBorderWidth
        clip: true

        radius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius
        color: Style.colorWithoutTransparency(palette.base)

        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Main content")

        MouseArea {
            anchors.fill: parent
            onClicked: forceActiveFocus()
        }

        TrayWindowHeader {
            id: trayWindowHeader

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Style.trayWindowHeaderHeight
        }

        Button {
            id: trayWindowSyncWarning

            readonly property color warningIconColor: Style.errorBoxBackgroundColor

            anchors.top: trayWindowHeader.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
            anchors.topMargin: Style.trayHorizontalMargin
            anchors.leftMargin: Style.trayHorizontalMargin
            anchors.rightMargin: Style.trayHorizontalMargin

            visible: UserModel.hasSyncErrors
                     && !(UserModel.syncErrorUserCount === 1
                          && UserModel.firstSyncErrorUserId === UserModel.currentUserId)
            padding: 0
            background: Rectangle {
                radius: Style.slightlyRoundedButtonRadius
                color: Qt.rgba(trayWindowSyncWarning.warningIconColor.r,
                               trayWindowSyncWarning.warningIconColor.g,
                               trayWindowSyncWarning.warningIconColor.b,
                               0.2)
                border.width: Style.normalBorderWidth
                border.color: Qt.rgba(trayWindowSyncWarning.warningIconColor.r,
                                      trayWindowSyncWarning.warningIconColor.g,
                                      trayWindowSyncWarning.warningIconColor.b,
                                      0.6)
            }

            Accessible.name: syncWarningText.text
            Accessible.role: Accessible.Button

            contentItem: RowLayout {
                anchors.fill: parent
                spacing: 0

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    Layout.topMargin: 4
                    Layout.leftMargin: Style.trayHorizontalMargin
                    Layout.rightMargin: Style.trayHorizontalMargin
                    Layout.bottomMargin: 4

                    EnforcedPlainTextLabel {
                        id: syncWarningText

                        Layout.fillWidth: true
                        font.pixelSize: Style.topLinePixelSize
                        font.bold: true
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        text: {
                            if (UserModel.syncErrorUserCount <= 1) {
                                return qsTr("Issue with account %1").arg(UserModel.firstSyncErrorUser ? UserModel.firstSyncErrorUser.name : "");
                            }
                            return qsTr("Issues with several accounts");
                        }
                    }
                }
            }

            onClicked: {
                if (UserModel.firstSyncErrorUserId >= 0) {
                    UserModel.currentUserId = UserModel.firstSyncErrorUserId
                }
            }
        }

        UnifiedSearchInputContainer {
            id: trayWindowUnifiedSearchInputContainer

            property bool activateSearchFocus: activeFocus

            anchors.top: trayWindowSyncWarning.visible
                         ? trayWindowSyncWarning.bottom
                         : trayWindowHeader.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
            anchors.topMargin: Style.trayHorizontalMargin
            anchors.leftMargin: Style.trayHorizontalMargin
            anchors.rightMargin: Style.trayHorizontalMargin

            text: UserModel.currentUser.unifiedSearchResultsListModel.searchTerm
            readOnly: !UserModel.currentUser.isConnected || UserModel.currentUser.unifiedSearchResultsListModel.currentFetchMoreInProgressProviderId
            isSearchInProgress: UserModel.currentUser.unifiedSearchResultsListModel.isSearchInProgress
            onTextEdited: { UserModel.currentUser.unifiedSearchResultsListModel.searchTerm = trayWindowUnifiedSearchInputContainer.text }
            onClearText: { UserModel.currentUser.unifiedSearchResultsListModel.searchTerm = "" }
            onActiveFocusChanged: activateSearchFocus = activeFocus && focusReason !== Qt.TabFocusReason && focusReason !== Qt.BacktabFocusReason
            Keys.onEscapePressed: activateSearchFocus = false
        }

        Rectangle {
            id: bottomUnifiedSearchInputSeparator

            anchors.top: trayWindowUnifiedSearchInputContainer.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: Style.trayHorizontalMargin

            height: 1
            color: palette.dark
            visible: trayWindowMainItem.isUnifiedSearchActive
        }

        ErrorBox {
            id: unifiedSearchResultsErrorLabel
            visible:  UserModel.currentUser.unifiedSearchResultsListModel.errorString && !unifiedSearchResultsListView.visible && ! UserModel.currentUser.unifiedSearchResultsListModel.isSearchInProgress && ! UserModel.currentUser.unifiedSearchResultsListModel.currentFetchMoreInProgressProviderId
            text:  UserModel.currentUser.unifiedSearchResultsListModel.errorString
            anchors.top: bottomUnifiedSearchInputSeparator.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
            anchors.margins: Style.trayHorizontalMargin
        }

        UnifiedSearchPlaceholderView {
            id: unifiedSearchPlaceholderView

            anchors.top: bottomUnifiedSearchInputSeparator.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
            anchors.bottom: trayWindowMainItem.bottom
            anchors.topMargin: Style.trayHorizontalMargin

            visible: trayWindowUnifiedSearchInputContainer.activateSearchFocus && !UserModel.currentUser.unifiedSearchResultsListModel.searchTerm
        }

        UnifiedSearchResultNothingFound {
            id: unifiedSearchResultNothingFound

            anchors.top: bottomUnifiedSearchInputSeparator.bottom
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

            anchors.top: bottomUnifiedSearchInputSeparator.bottom
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

            anchors.top: bottomUnifiedSearchInputSeparator.bottom
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

            accentColor: Style.accentColor
            visible: !trayWindowMainItem.isUnifiedSearchActive

            anchors.top: trayWindowUnifiedSearchInputContainer.bottom
            anchors.left: trayWindowMainItem.left
            anchors.right: trayWindowMainItem.right
        }

        Rectangle {
            id: syncStatusSeparator
            anchors.left: syncStatus.left
            anchors.right: syncStatus.right
            anchors.bottom: syncStatus.bottom
            height: 1
            color: palette.dark
            visible: !trayWindowMainItem.isUnifiedSearchActive
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

            sourceComponent: Button {
                id: newActivitiesButton
                hoverEnabled: true
                padding: Style.smallSpacing

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
                    target: newActivitiesButton
                    from: 1
                    to: 0
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
                function onInteractiveActivityReceived() {
                    if (!activityList.atYBeginning) {
                        newActivitiesButtonLoader.active = true;
                    }
                }
            }
        }
    } // Item trayWindowMainItem
}
