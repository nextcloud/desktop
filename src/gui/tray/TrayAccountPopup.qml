/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Qt5Compat.GraphicalEffects

import Style
import com.nextcloud.desktopclient
import com.nextcloud.desktopclient as NC

// Keep behavior and layout aligned with src/gui/macOS/trayaccountpopup_mac.mm.

Window {
    id: root

    readonly property bool hasAccounts: UserModel && UserModel.count > 0
    readonly property color rowHoverColor: Style.darkMode
                                               ? Qt.rgba(1, 1, 1, Style.trayAccountPopupRowHoverOpacity)
                                               : Qt.rgba(0, 0, 0, Style.trayAccountPopupRowHoverOpacity)

    width: Style.trayAccountPopupWidth
    height: contentColumn.height
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint

    property bool _closing: false
    property bool _hadFocusSinceShow: false
    property var activeAccountActionsMenu: null

    onVisibleChanged: {
        if (visible) {
            _hadFocusSinceShow = false
        }
    }

    onActiveChanged: {
        if (active) {
            _hadFocusSinceShow = true
        } else if (_hadFocusSinceShow && !_closing) {
            Systray.hideWindow()
        }
        _closing = false
    }

    function closeActiveAccountActionsMenu() {
        if (activeAccountActionsMenu && activeAccountActionsMenu.opened) {
            activeAccountActionsMenu.close()
        }
        activeAccountActionsMenu = null
    }

    function translatedAskAssistantText() {
        return qsTranslate("MainWindow", "Ask Assistant\u00A0…")
    }

    Rectangle {
        id: popupContainer
        anchors.fill: parent
        radius: Style.trayWindowRadius
        color: palette.window
        border.width: Style.trayWindowBorderWidth
        border.color: palette.dark
        clip: true
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: popupContainer.width
                height: popupContainer.height
                radius: popupContainer.radius
                visible: false
            }
        }

        Column {
            id: contentColumn
            width: parent.width
            spacing: 0

            Item {
                width: parent.width
                height: Style.trayAccountPopupTopPadding
            }

            Repeater {
                model: UserModel

                delegate: ItemDelegate {
                    id: accountRow
                    readonly property int userId: model.id
                    readonly property int onlineStatus: model.status
                    readonly property bool onlineStatusEnabled: model.isConnected && model.serverHasUserStatus
                    readonly property string statusIcon: model.statusIcon
                    readonly property string statusMessage: model.statusMessage
                    readonly property bool menuHighlighted: hovered || accountActionsMenu.opened

                    width: root.width
                    height: Style.trayAccountPopupRowHeight
                    hoverEnabled: true
                    topInset: 0
                    leftInset: 0
                    rightInset: 0
                    bottomInset: 0
                    padding: 0
                    leftPadding: Style.trayAccountPopupRowPadding
                    rightPadding: Style.trayAccountPopupRowPadding

                    function openActivities() {
                        root._closing = true
                        UserModel.currentUserId = accountRow.userId
                        Systray.showQMLWindow()
                    }

                    function openLocalFolder() {
                        root._closing = true
                        UserModel.currentUserId = accountRow.userId
                        Systray.hideWindow()
                        if (UserModel.currentUser && UserModel.currentUser.hasLocalFolder) {
                            UserModel.openCurrentAccountLocalFolder()
                        } else if (Qt.platform.os === "osx"
                                   && UserModel.currentUser
                                   && UserModel.currentUser.hasFileProvider) {
                            UserModel.openCurrentAccountFileProviderDomain()
                        }
                    }

                    function currentStatusText() {
                        switch (onlineStatus) {
                        case NC.userStatus.Away:
                            return qsTranslate("UserStatusSetStatusView", "Away")
                        case NC.userStatus.Busy:
                            return qsTranslate("UserStatusSetStatusView", "Busy")
                        case NC.userStatus.DoNotDisturb:
                            return qsTranslate("UserStatusSetStatusView", "Do not disturb")
                        case NC.userStatus.Invisible:
                            return qsTranslate("UserStatusSetStatusView", "Invisible")
                        case NC.userStatus.Offline:
                            return qsTranslate("OCC::SyncStatusSummary", "Offline")
                        case NC.userStatus.Online:
                        default:
                            return qsTranslate("UserStatusSetStatusView", "Online")
                        }
                    }

                    function currentStatusLabelText() {
                        var message = statusMessage.trim()
                        return message !== "" ? message : currentStatusText()
                    }

                    function openAccountActionsMenu() {
                        TrayAccountAppsModel.setUserId(accountRow.userId)
                        root.closeActiveAccountActionsMenu()

                        var rightAlignedX = Math.max(Style.trayAccountPopupHoverMargin,
                                                     accountRow.width - accountActionsMenu.width - Style.trayAccountPopupHoverMargin)
                        var leftAlignedX = Style.trayAccountPopupHoverMargin
                        var rowPosition = accountRow.mapToItem(popupContainer, 0, 0)
                        var screenLeft = root.screen && root.screen.virtualX !== undefined ? root.screen.virtualX : root.x
                        var screenWidth = root.screen && root.screen.width !== undefined ? root.screen.width : root.width
                        var screenRight = screenLeft + screenWidth
                        var rightAlignedScreenRight = root.x + rowPosition.x + rightAlignedX + accountActionsMenu.width

                        var menuX = rightAlignedScreenRight > screenRight - Style.trayAccountPopupHoverMargin
                                    && root.x + rowPosition.x + leftAlignedX >= screenLeft + Style.trayAccountPopupHoverMargin
                                    ? leftAlignedX
                                    : rightAlignedX

                        accountActionsMenu.popup(accountRow,
                                                 menuX,
                                                 Style.trayAccountPopupAccountHoverVerticalMargin)
                        root.activeAccountActionsMenu = accountActionsMenu
                    }

                    onHoveredChanged: {
                        if (hovered && !accountActionsMenu.opened) {
                            openAccountActionsMenu()
                        }
                    }

                    background: Item {
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: Style.trayAccountPopupHoverMargin
                            anchors.rightMargin: Style.trayAccountPopupHoverMargin
                            anchors.topMargin: Style.trayAccountPopupAccountHoverVerticalMargin
                            anchors.bottomMargin: Style.trayAccountPopupAccountHoverVerticalMargin
                            radius: Style.trayAccountPopupHoverRadius
                            color: accountRow.menuHighlighted ? root.rowHoverColor : "transparent"
                            Behavior on color { ColorAnimation { duration: Style.trayAccountPopupHoverAnimationDuration } }
                        }
                    }

                    AutoSizingMenu {
                        id: accountActionsMenu

                        closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape
                        height: implicitHeight
                        onClosed: {
                            appsMenu.close()
                            if (root.activeAccountActionsMenu === accountActionsMenu) {
                                root.activeAccountActionsMenu = null
                            }
                        }

                        function closeAppsMenu() {
                            if (appsMenu.opened) {
                                appsMenu.close()
                            }
                        }

                        MenuItem {
                            id: statusButton

                            enabled: accountRow.onlineStatusEnabled
                            text: accountRow.currentStatusLabelText()
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: true
                            onHoveredChanged: {
                                if (hovered) {
                                    accountActionsMenu.closeAppsMenu()
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 8

                                Image {
                                    Layout.preferredWidth: Style.trayAccountPopupSyncIconSize + Style.trayFolderStatusIndicatorSizeOffset
                                    Layout.preferredHeight: Style.trayAccountPopupSyncIconSize + Style.trayFolderStatusIndicatorSizeOffset
                                    visible: statusButton.enabled
                                    source: statusButton.enabled ? accountRow.statusIcon : ""
                                    sourceSize.width: Style.trayAccountPopupSyncIconSize + Style.trayFolderStatusIndicatorSizeOffset
                                    sourceSize.height: Style.trayAccountPopupSyncIconSize + Style.trayFolderStatusIndicatorSizeOffset
                                    cache: false
                                }

                                EnforcedPlainTextLabel {
                                    Layout.fillWidth: true
                                    text: statusButton.text
                                    font: statusButton.font
                                    color: statusButton.enabled ? palette.windowText : palette.mid
                                    elide: Text.ElideRight
                                }
                            }
                            onClicked: {
                                root._closing = true
                                Systray.showUserStatusWindow(accountRow.userId)
                            }

                            Accessible.role: Accessible.Button
                            Accessible.name: text
                            Accessible.onPressAction: statusButton.clicked()
                        }

                        MenuItem {
                            id: openLocalFolderButton

                            text: qsTranslate("TrayFoldersMenuButton", "Open local folder")
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: true
                            onHoveredChanged: {
                                if (hovered) {
                                    accountActionsMenu.closeAppsMenu()
                                }
                            }
                            onClicked: accountRow.openLocalFolder()

                            Accessible.role: Accessible.Button
                            Accessible.name: text
                            Accessible.onPressAction: openLocalFolderButton.clicked()
                        }

                        MenuItem {
                            id: assistantButton

                            enabled: false
                            text: root.translatedAskAssistantText()
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: true
                            onHoveredChanged: {
                                if (hovered) {
                                    accountActionsMenu.closeAppsMenu()
                                }
                            }

                            Accessible.role: Accessible.Button
                            Accessible.name: text
                        }

                        MenuSeparator {
                        }

                        MenuItem {
                            id: activitiesButton

                            text: qsTranslate("FileDetailsPage", "Activity")
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: true
                            onHoveredChanged: {
                                if (hovered) {
                                    accountActionsMenu.closeAppsMenu()
                                }
                            }
                            onClicked: accountRow.openActivities()

                            Accessible.role: Accessible.Button
                            Accessible.name: text
                            Accessible.onPressAction: activitiesButton.clicked()
                        }

                        MenuItem {
                            id: appsButton

                            text: qsTranslate("TrayWindowHeader", "More apps")
                            enabled: TrayAccountAppsModel.count > 0
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: true

                            function openAppsMenu() {
                                if (!enabled) {
                                    return
                                }
                                TrayAccountAppsModel.setUserId(accountRow.userId)
                                if (!appsMenu.opened) {
                                    appsMenu.popup(appsButton, appsButton.width, 0)
                                }
                            }

                            onHoveredChanged: {
                                if (hovered) {
                                    openAppsMenu()
                                }
                            }

                            onClicked: openAppsMenu()

                            background: Rectangle {
                                color: appsButton.hovered || appsMenu.opened ? root.rowHoverColor : "transparent"
                            }

                            contentItem: RowLayout {
                                spacing: 8

                                EnforcedPlainTextLabel {
                                    Layout.fillWidth: true
                                    text: appsButton.text
                                    font: appsButton.font
                                    color: appsButton.enabled ? palette.windowText : palette.mid
                                    elide: Text.ElideRight
                                }

                                EnforcedPlainTextLabel {
                                    text: "›"
                                    font.pixelSize: Style.trayAccountPopupChevronFontSize
                                    color: appsButton.enabled ? palette.windowText : palette.mid
                                    opacity: appsButton.enabled ? 0.35 : 1.0
                                }
                            }

                            Accessible.role: Accessible.Button
                            Accessible.name: text
                            Accessible.onPressAction: appsButton.clicked()
                        }

                        AutoSizingMenu {
                            id: appsMenu

                            closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                            Repeater {
                                model: TrayAccountAppsModel

                                delegate: MenuItem {
                                    id: appEntry

                                    text: "  " + model.appName
                                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                                    icon.source: "image://tray-image-provider/" + model.appIconUrl
                                    icon.color: palette.windowText
                                    onTriggered: {
                                        root._closing = true
                                        appsMenu.close()
                                        accountActionsMenu.close()
                                        Systray.hideWindow()
                                        TrayAccountAppsModel.openAppUrl(appUrl)
                                    }

                                    Accessible.role: Accessible.MenuItem
                                    Accessible.name: qsTr("Open %1 in browser").arg(model.appName)
                                    Accessible.onPressAction: appEntry.triggered()
                                }
                            }
                        }
                    }

                    contentItem: RowLayout {
                        spacing: Style.trayAccountPopupRowSpacing

                        Image {
                            Layout.preferredWidth: Style.trayAccountPopupAvatarSize
                            Layout.preferredHeight: Style.trayAccountPopupAvatarSize
                            source: model.avatar !== "" ? model.avatar
                                : (Style.darkMode ? "image://avatars/fallbackWhite" : "image://avatars/fallbackBlack")
                            fillMode: Image.PreserveAspectCrop
                            cache: false
                            layer.enabled: true
                            layer.effect: OpacityMask {
                                maskSource: Rectangle {
                                    width: Style.trayAccountPopupAvatarSize
                                    height: Style.trayAccountPopupAvatarSize
                                    radius: width / 2
                                    visible: false
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            EnforcedPlainTextLabel {
                                Layout.fillWidth: true
                                text: model.name
                                font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                color: palette.windowText
                            }

                            EnforcedPlainTextLabel {
                                Layout.fillWidth: true
                                text: model.server
                                font.pixelSize: Style.trayAccountPopupSecondaryFontSize
                                elide: Text.ElideRight
                                color: palette.windowText
                                opacity: 0.6
                            }
                        }

                        Image {
                            Layout.preferredWidth: Style.trayAccountPopupSyncIconSize
                            Layout.preferredHeight: Style.trayAccountPopupSyncIconSize
                            source: model.syncStatusIcon
                            sourceSize: Qt.size(Style.trayAccountPopupSyncIconSize,
                                                Style.trayAccountPopupSyncIconSize)
                        }

                        EnforcedPlainTextLabel {
                            text: "›"
                            font.pixelSize: Style.trayAccountPopupChevronFontSize
                            color: palette.windowText
                            opacity: 0.35
                        }
                    }

                    onClicked: {
                        accountRow.openActivities()
                    }
                }
            }

            Rectangle {
                visible: root.hasAccounts
                width: parent.width
                height: visible ? Style.trayWindowBorderWidth : 0
                color: palette.mid
                opacity: Style.darkMode ? 1.0 : 0.5
            }

            Item {
                width: parent.width
                height: Style.trayAccountPopupActionVerticalPadding
            }

            ItemDelegate {
                id: addAccountRow
                visible: Systray.enableAddAccount
                width: root.width
                height: visible ? Style.trayAccountPopupActionHeight : 0
                hoverEnabled: true
                topInset: 0
                leftInset: 0
                rightInset: 0
                bottomInset: 0
                padding: 0
                leftPadding: Style.trayAccountPopupRowPadding

                background: Item {
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Style.trayAccountPopupHoverMargin
                        anchors.rightMargin: Style.trayAccountPopupHoverMargin
                        radius: Style.trayAccountPopupHoverRadius
                        color: addAccountRow.hovered ? root.rowHoverColor : "transparent"
                        Behavior on color { ColorAnimation { duration: Style.trayAccountPopupHoverAnimationDuration } }
                    }
                }

                contentItem: EnforcedPlainTextLabel {
                    text: qsTr("Add account")
                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onHoveredChanged: {
                    if (hovered) {
                        root.closeActiveAccountActionsMenu()
                    }
                }

                onClicked: {
                    root._closing = true
                    Systray.hideWindow()
                    Systray.openAccountWizard()
                }
            }

            ItemDelegate {
                id: settingsRow
                width: root.width
                height: Style.trayAccountPopupActionHeight
                hoverEnabled: true
                topInset: 0
                leftInset: 0
                rightInset: 0
                bottomInset: 0
                padding: 0
                leftPadding: Style.trayAccountPopupRowPadding

                background: Item {
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Style.trayAccountPopupHoverMargin
                        anchors.rightMargin: Style.trayAccountPopupHoverMargin
                        radius: Style.trayAccountPopupHoverRadius
                        color: settingsRow.hovered ? root.rowHoverColor : "transparent"
                        Behavior on color { ColorAnimation { duration: Style.trayAccountPopupHoverAnimationDuration } }
                    }
                }

                contentItem: EnforcedPlainTextLabel {
                    text: qsTr("Settings")
                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onHoveredChanged: {
                    if (hovered) {
                        root.closeActiveAccountActionsMenu()
                    }
                }

                onClicked: {
                    root._closing = true
                    Systray.hideWindow()
                    Systray.openSettings()
                }
            }

            ItemDelegate {
                id: quitRow
                width: root.width
                height: Style.trayAccountPopupActionHeight
                hoverEnabled: true
                topInset: 0
                leftInset: 0
                rightInset: 0
                bottomInset: 0
                padding: 0
                leftPadding: Style.trayAccountPopupRowPadding

                background: Item {
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Style.trayAccountPopupHoverMargin
                        anchors.rightMargin: Style.trayAccountPopupHoverMargin
                        radius: Style.trayAccountPopupHoverRadius
                        color: quitRow.hovered ? root.rowHoverColor : "transparent"
                        Behavior on color { ColorAnimation { duration: Style.trayAccountPopupHoverAnimationDuration } }
                    }
                }

                contentItem: EnforcedPlainTextLabel {
                    text: qsTr("Quit")
                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onHoveredChanged: {
                    if (hovered) {
                        root.closeActiveAccountActionsMenu()
                    }
                }

                onClicked: {
                    root._closing = true
                    Systray.shutdown()
                }
            }

            Item {
                width: parent.width
                height: Style.trayAccountPopupActionVerticalPadding
            }
        }
    }
}
