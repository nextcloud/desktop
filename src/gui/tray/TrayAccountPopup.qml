/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQml.Models
import Qt5Compat.GraphicalEffects

import Style
import com.nextcloud.desktopclient
import com.nextcloud.desktopclient as NC

// Keep behavior and layout aligned with src/gui/macOS/trayaccountpopup_mac.mm.

Window {
    id: root

    property bool _closing: false
    property bool _hadFocusSinceShow: false
    property var activeAccountActionsMenu: null

    readonly property bool hasAccounts: UserModel && UserModel.count > 0

    width: Style.trayAccountPopupWidth
    height: contentColumn.height
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint

    Component.onCompleted: Systray.forceWindowInit(root)

    onVisibleChanged: {
        if (visible) {
            _hadFocusSinceShow = false
        } else {
            closeActiveTraySubmenus()
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

    function closeActiveTraySubmenus() {
        closeActiveAccountActionsMenu()
    }

    function translatedAskAssistantText() {
        return qsTranslate("MainWindow", "Ask Assistant\u00A0…")
    }

    component TrayActionHoverBackground: Item {
        property bool active: false
        property int verticalMargin: 0

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: Style.trayAccountPopupHoverMargin
            anchors.rightMargin: Style.trayAccountPopupHoverMargin
            anchors.topMargin: parent.verticalMargin
            anchors.bottomMargin: parent.verticalMargin
            radius: Style.trayAccountPopupHoverRadius
            visible: opacity > 0
            color: Style.darkMode
                   ? Qt.rgba(1, 1, 1, Style.trayAccountPopupRowHoverOpacity)
                   : Qt.rgba(0, 0, 0, Style.trayAccountPopupRowHoverOpacity)
            opacity: parent.active ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: Style.trayAccountPopupHoverAnimationDuration } }
        }
    }

    Rectangle {
        id: popupContainer
        anchors.fill: parent
        radius: Style.trayWindowRadius
        color: palette.window
        border.width: Style.trayWindowBorderWidth
        border.color: palette.dark
        clip: true
        layer.enabled: root.visible
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

                delegate: Column {
                    id: accountDelegate
                    readonly property int userId: model.id
                    readonly property int onlineStatus: model.status
                    readonly property bool onlineStatusEnabled: model.isConnected && model.serverHasUserStatus
                    readonly property url statusIcon: model.statusIcon
                    readonly property bool hasStatusIcon: statusIcon.toString() !== ""
                    readonly property string statusMessage: model.statusMessage
                    readonly property var recentActivities: model.recentActivities ? model.recentActivities : []
                    readonly property var trayNotifications: model.trayNotifications ? model.trayNotifications : []
                    readonly property var accountAlert: model.accountAlert ? model.accountAlert : ({})
                    readonly property string accountAlertTitle: accountAlert.title ? accountAlert.title : ""
                    readonly property bool hasAccountAlert: accountAlertTitle !== ""
                    readonly property bool assistantEnabled: model.assistantEnabled

                    width: root.width
                    height: accountRow.height + accountAlertBox.height
                    spacing: 0

                    function openActivities() {
                        root._closing = true
                        Systray.showActivitiesWindow(accountDelegate.userId)
                    }

                    function openLocalFolder() {
                        root._closing = true
                        UserModel.currentUserId = accountDelegate.userId
                        Systray.hideWindow()
                        if (UserModel.currentUser && UserModel.currentUser.hasLocalFolder) {
                            UserModel.openCurrentAccountLocalFolder()
                        } else if (Qt.platform.os === "osx"
                                   && UserModel.currentUser
                                   && UserModel.currentUser.hasFileProvider) {
                            UserModel.openCurrentAccountFileProviderDomain()
                        }
                    }

                    function openAssistant() {
                        root._closing = true
                        Systray.showAssistantWindow(accountDelegate.userId)
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
                        const message = statusMessage.trim()
                        return message !== "" ? message : currentStatusText()
                    }

                    function openAccountActionsMenu() {
                        TrayAccountAppsModel.setUserId(accountDelegate.userId)
                        UserModel.fetchActivityPreview(accountDelegate.userId)
                        root.closeActiveAccountActionsMenu()
                        accountActionsMenu.popupSubmenuForRow(accountActionsMenu, accountRow)
                        root.activeAccountActionsMenu = accountActionsMenu
                    }

                    ItemDelegate {
                        id: accountRow

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

                    onHoveredChanged: {
                        if (hovered && !accountActionsMenu.opened) {
                            openAccountActionsMenu()
                        }
                    }

                    background: TrayActionHoverBackground {
                        active: accountRow.hovered || accountActionsMenu.opened
                        verticalMargin: Style.trayAccountPopupAccountHoverVerticalMargin
                    }

                    AutoSizingMenu {
                        id: accountActionsMenu

                        property var activeNotificationActionsMenu: null
                        property var anchorRow: null

                        width: Style.trayAccountActionsMenuWidth
                        topPadding: Style.trayAccountPopupActionVerticalPadding
                        bottomPadding: Style.trayAccountPopupActionVerticalPadding
                        focus: false
                        modal: false
                        closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape
                        height: implicitHeight
                        onHeightChanged: repositionOpenSubmenu(accountActionsMenu)
                        onClosed: {
                            appsMenu.close()
                            closeNotificationActionsMenu()
                            anchorRow = null
                            if (root.activeAccountActionsMenu === accountActionsMenu) {
                                root.activeAccountActionsMenu = null
                            }
                        }

                        function closeAppsMenu() {
                            if (appsMenu.opened) {
                                appsMenu.close()
                            }
                        }

                        function closeNotificationActionsMenu() {
                            if (activeNotificationActionsMenu && activeNotificationActionsMenu.opened) {
                                activeNotificationActionsMenu.close()
                            }
                            activeNotificationActionsMenu = null
                        }

                        function closeSubmenus() {
                            closeAppsMenu()
                            closeNotificationActionsMenu()
                        }

                        function itemIndex(item) {
                            for (let i = 0; i < count; ++i) {
                                if (itemAt(i) === item) {
                                    return i
                                }
                            }
                            return -1
                        }

                        function insertionIndexAfter(anchorItem, offset) {
                            const anchorIndex = itemIndex(anchorItem)
                            return anchorIndex < 0 ? count : anchorIndex + 1 + offset
                        }

                        function submenuPositionForRow(menu, row) {
                            const menuWidth = Math.max(menu.width, menu.implicitWidth)
                            const menuHeight = Math.max(menu.height, menu.implicitHeight)
                            const margin = Style.trayAccountPopupHoverMargin
                            const rowPosition = row.mapToItem(popupContainer, 0, 0)
                            const screenLeft = root.screen ? root.screen.virtualX : root.x
                            const screenTop = root.screen ? root.screen.virtualY : root.y
                            const screenRight = screenLeft + (root.screen ? root.screen.width : root.width)
                            const screenBottom = screenTop + (root.screen ? root.screen.height : root.height)
                            const rightAlignedX = row.width
                            const leftAlignedX = -menuWidth
                            const rowScreenX = root.x + rowPosition.x
                            const rightAlignedScreenRight = rowScreenX + rightAlignedX + menuWidth
                            const leftAlignedScreenLeft = rowScreenX + leftAlignedX
                            let menuX = rightAlignedScreenRight > screenRight - margin
                                          && leftAlignedScreenLeft >= screenLeft + margin
                                          ? leftAlignedX
                                          : rightAlignedX
                            const menuScreenLeft = rowScreenX + menuX
                            if (menuScreenLeft < screenLeft + margin) {
                                menuX = screenLeft + margin - rowScreenX
                            } else if (menuScreenLeft + menuWidth > screenRight - margin) {
                                menuX = screenRight - margin - menuWidth - rowScreenX
                            }

                            let menuY = 0
                            const screenY = root.y + rowPosition.y
                            const bottomOverflow = screenY + menuHeight - (screenBottom - margin)
                            if (bottomOverflow > 0) {
                                menuY -= bottomOverflow
                            }
                            if (screenY + menuY < screenTop + margin) {
                                menuY = screenTop + margin - screenY
                            }

                            return { "x": menuX, "y": menuY }
                        }

                        function popupSubmenuForRow(menu, row) {
                            menu.anchorRow = row
                            const position = submenuPositionForRow(menu, row)
                            menu.popup(row, position.x, position.y)
                        }

                        function repositionOpenSubmenu(menu) {
                            if (menu.opened && menu.anchorRow) {
                                const position = submenuPositionForRow(menu, menu.anchorRow)
                                menu.popup(menu.anchorRow, position.x, position.y)
                            }
                        }

                        MenuItem {
                            id: userStatusHeader

                            enabled: false
                            text: qsTr("User status")
                            font.pixelSize: Style.trayAccountPopupSecondaryFontSize
                            font.weight: Font.DemiBold
                            hoverEnabled: false
                            background: Item {}
                            contentItem: EnforcedPlainTextLabel {
                                text: userStatusHeader.text
                                font: userStatusHeader.font
                                color: palette.windowText
                                opacity: 0.7
                                elide: Text.ElideRight
                            }

                            Accessible.role: Accessible.StaticText
                            Accessible.name: text
                        }

                        MenuItem {
                            id: statusButton

                            readonly property bool isActionable: accountDelegate.onlineStatusEnabled

                            enabled: statusButton.isActionable
                            text: accountDelegate.currentStatusLabelText()
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: statusButton.enabled
                            onHoveredChanged: {
                                if (hovered) {
                                    accountActionsMenu.closeSubmenus()
                                }
                            }
                            background: TrayActionHoverBackground {
                                active: statusButton.enabled && statusButton.hovered
                            }
                            contentItem: RowLayout {
                                spacing: 8

                                Image {
                                    Layout.preferredWidth: Style.trayAccountPopupSyncIconSize
                                    Layout.preferredHeight: Style.trayAccountPopupSyncIconSize
                                    visible: accountDelegate.hasStatusIcon
                                    source: accountDelegate.statusIcon
                                    sourceSize.width: Style.trayAccountPopupSyncIconSize
                                    sourceSize.height: Style.trayAccountPopupSyncIconSize
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
                                if (!statusButton.enabled) {
                                    return
                                }
                                root._closing = true
                                Systray.showUserStatusWindow(accountDelegate.userId)
                            }

                            Accessible.role: statusButton.isActionable ? Accessible.Button : Accessible.StaticText
                            Accessible.name: text
                            Accessible.onPressAction: {
                                if (statusButton.isActionable) {
                                    statusButton.clicked()
                                }
                            }
                        }

                        MenuSeparator {
                        }

                        MenuItem {
                            id: openLocalFolderButton

                            text: qsTranslate("TrayFoldersMenuButton", "Open local folder")
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: true
                            onHoveredChanged: {
                                if (hovered) {
                                    accountActionsMenu.closeSubmenus()
                                }
                            }
                            background: TrayActionHoverBackground {
                                active: openLocalFolderButton.hovered
                            }
                            onClicked: accountDelegate.openLocalFolder()

                            Accessible.role: Accessible.Button
                            Accessible.name: text
                            Accessible.onPressAction: openLocalFolderButton.clicked()
                        }

                        MenuItem {
                            id: assistantButton

                            visible: accountDelegate.assistantEnabled
                            enabled: accountDelegate.assistantEnabled
                            height: visible ? implicitHeight : 0
                            text: root.translatedAskAssistantText()
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: true
                            onHoveredChanged: {
                                if (hovered) {
                                    accountActionsMenu.closeSubmenus()
                                }
                            }
                            background: TrayActionHoverBackground {
                                active: assistantButton.hovered
                            }
                            onClicked: accountDelegate.openAssistant()

                            Accessible.role: Accessible.Button
                            Accessible.name: text
                            Accessible.onPressAction: assistantButton.clicked()
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
                                accountActionsMenu.closeNotificationActionsMenu()
                                TrayAccountAppsModel.setUserId(accountDelegate.userId)
                                if (!appsMenu.opened) {
                                    accountActionsMenu.popupSubmenuForRow(appsMenu, appsButton)
                                }
                            }

                            onHoveredChanged: {
                                if (hovered) {
                                    openAppsMenu()
                                }
                            }

                            onClicked: openAppsMenu()

                            background: TrayActionHoverBackground {
                                active: appsButton.hovered || appsMenu.opened
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

                        MenuSeparator {
                        }

                        MenuItem {
                            id: notificationsHeader

                            visible: accountDelegate.trayNotifications.length > 0
                            height: visible ? implicitHeight : 0
                            enabled: false
                            text: qsTr("Notifications")
                            font.pixelSize: Style.trayAccountPopupSecondaryFontSize
                            font.weight: Font.DemiBold
                            hoverEnabled: false
                            background: Item {}
                            contentItem: EnforcedPlainTextLabel {
                                text: notificationsHeader.text
                                font: notificationsHeader.font
                                color: palette.windowText
                                opacity: 0.7
                                elide: Text.ElideRight
                            }

                            Accessible.role: Accessible.StaticText
                            Accessible.name: text
                        }

                        Instantiator {
                            model: accountDelegate.trayNotifications

                            onObjectAdded: (index, object) => {
                                accountActionsMenu.insertItem(accountActionsMenu.insertionIndexAfter(notificationsHeader, index), object)
                            }
                            onObjectRemoved: (index, object) => {
                                accountActionsMenu.removeItem(object)
                            }

                            delegate: MenuItem {
                                id: notificationRow

                                required property var modelData

                                readonly property var notificationActions: modelData.actions ? modelData.actions : []
                                readonly property bool hasNotificationActions: notificationActions.length > 0
                                readonly property string rowDateTime: modelData.dateTime ? modelData.dateTime : ""

                                text: modelData.title
                                height: Style.trayAccountPopupPreviewActionHeight
                                font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                                hoverEnabled: true

                                function iconSource() {
                                    if (!modelData.icon || modelData.icon === "") {
                                        return "image://svgimage-custom-color/activity.svg/" + palette.windowText
                                    }
                                    return modelData.icon + "/" + palette.windowText
                                }

                                function openNotification() {
                                    accountActionsMenu.closeSubmenus()
                                    if (modelData.opensSettings === true) {
                                        root._closing = true
                                        accountActionsMenu.close()
                                        Systray.hideWindow()
                                        Systray.openSettings()
                                        return
                                    }
                                    accountDelegate.openActivities()
                                }

                                function openNotificationActionsMenu() {
                                    if (!hasNotificationActions) {
                                        return
                                    }
                                    if (accountActionsMenu.activeNotificationActionsMenu
                                            && accountActionsMenu.activeNotificationActionsMenu !== notificationActionsMenu) {
                                        accountActionsMenu.activeNotificationActionsMenu.close()
                                    }
                                    if (!notificationActionsMenu.opened) {
                                        accountActionsMenu.popupSubmenuForRow(notificationActionsMenu, notificationRow)
                                    }
                                    accountActionsMenu.activeNotificationActionsMenu = notificationActionsMenu
                                }

                                onHoveredChanged: {
                                    if (hovered) {
                                        accountActionsMenu.closeAppsMenu()
                                        if (hasNotificationActions) {
                                            openNotificationActionsMenu()
                                        } else {
                                            accountActionsMenu.closeNotificationActionsMenu()
                                        }
                                    }
                                }

                                onClicked: openNotification()

                                background: TrayActionHoverBackground {
                                    active: notificationRow.hovered || notificationActionsMenu.opened
                                }

                                contentItem: RowLayout {
                                    spacing: 8

                                    Image {
                                        Layout.preferredWidth: Style.trayAccountPopupSyncIconSize
                                        Layout.preferredHeight: Style.trayAccountPopupSyncIconSize
                                        Layout.alignment: Qt.AlignVCenter
                                        source: notificationRow.iconSource()
                                        sourceSize.width: Style.trayAccountPopupSyncIconSize
                                        sourceSize.height: Style.trayAccountPopupSyncIconSize
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 0

                                        EnforcedPlainTextLabel {
                                            Layout.fillWidth: true
                                            text: notificationRow.text
                                            font: notificationRow.font
                                            color: palette.windowText
                                            elide: Text.ElideRight
                                            wrapMode: Text.Wrap
                                            maximumLineCount: 2
                                        }

                                        EnforcedPlainTextLabel {
                                            Layout.fillWidth: true
                                            text: notificationRow.rowDateTime
                                            font.pixelSize: Style.trayAccountPopupSecondaryFontSize
                                            color: palette.windowText
                                            opacity: 0.65
                                            horizontalAlignment: Text.AlignLeft
                                            elide: Text.ElideRight
                                            visible: text !== ""
                                        }
                                    }

                                    EnforcedPlainTextLabel {
                                        Layout.alignment: Qt.AlignVCenter
                                        visible: notificationRow.hasNotificationActions
                                        text: "›"
                                        font.pixelSize: Style.trayAccountPopupChevronFontSize
                                        color: palette.windowText
                                        opacity: 0.35
                                    }
                                }

                                AutoSizingMenu {
                                    id: notificationActionsMenu

                                    property var anchorRow: null

                                    width: Style.trayNotificationActionsMenuWidth
                                    topPadding: Style.trayAccountPopupActionVerticalPadding
                                    bottomPadding: Style.trayAccountPopupActionVerticalPadding
                                    focus: false
                                    modal: false
                                    closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape
                                    onHeightChanged: accountActionsMenu.repositionOpenSubmenu(notificationActionsMenu)
                                    onClosed: {
                                        anchorRow = null
                                        if (accountActionsMenu.activeNotificationActionsMenu === notificationActionsMenu) {
                                            accountActionsMenu.activeNotificationActionsMenu = null
                                        }
                                    }

                                    Repeater {
                                        model: notificationRow.notificationActions

                                        delegate: MenuItem {
                                            id: notificationActionMenuItem

                                            required property var modelData

                                            text: modelData.label
                                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                                            hoverEnabled: true
                                            background: TrayActionHoverBackground {
                                                active: notificationActionMenuItem.hovered
                                            }
                                            onTriggered: {
                                                const activityIndex = notificationRow.modelData.activityIndex
                                                const actionIndex = modelData.actionIndex
                                                notificationActionsMenu.close()
                                                if (modelData.actionType === "dismiss") {
                                                    UserModel.dismissNotification(accountDelegate.userId, activityIndex)
                                                    return
                                                }
                                                if (modelData.actionType === "openActivities") {
                                                    accountDelegate.openActivities()
                                                    return
                                                }
                                                root._closing = true
                                                accountActionsMenu.close()
                                                Systray.hideWindow()
                                                UserModel.triggerNotificationAction(accountDelegate.userId, activityIndex, actionIndex)
                                            }

                                            Accessible.role: Accessible.MenuItem
                                            Accessible.name: text
                                            Accessible.onPressAction: notificationActionMenuItem.triggered()
                                        }
                                    }
                                }

                                Accessible.role: Accessible.Button
                                Accessible.name: text
                                Accessible.onPressAction: notificationRow.clicked()
                            }
                        }

                        MenuSeparator {
                            visible: accountDelegate.trayNotifications.length > 0
                            height: visible ? Style.trayAccountPopupCompactSeparatorHeight : 0
                        }

                        MenuItem {
                            id: lastActivitiesHeader

                            enabled: false
                            text: qsTr("Recent activity")
                            font.pixelSize: Style.trayAccountPopupSecondaryFontSize
                            font.weight: Font.DemiBold
                            hoverEnabled: false
                            background: Item {}
                            contentItem: EnforcedPlainTextLabel {
                                text: lastActivitiesHeader.text
                                font: lastActivitiesHeader.font
                                color: palette.windowText
                                opacity: 0.7
                                elide: Text.ElideRight
                            }

                            Accessible.role: Accessible.StaticText
                            Accessible.name: text
                        }

                        Instantiator {
                            model: accountDelegate.recentActivities

                            onObjectAdded: (index, object) => {
                                accountActionsMenu.insertItem(accountActionsMenu.insertionIndexAfter(lastActivitiesHeader, index), object)
                            }
                            onObjectRemoved: (index, object) => {
                                accountActionsMenu.removeItem(object)
                            }

                            delegate: MenuItem {
                                id: recentActivityRow

                                required property var modelData

                                readonly property string rowDateTime: modelData.dateTime ? modelData.dateTime : ""
                                readonly property string rowSubtitle: modelData.subtitle ? modelData.subtitle : ""

                                enabled: true
                                text: modelData.title
                                height: rowSubtitle !== "" ? Style.trayAccountPopupDetailedPreviewActionHeight
                                                           : Style.trayAccountPopupPreviewActionHeight
                                font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                                hoverEnabled: true
                                background: TrayActionHoverBackground {
                                    active: recentActivityRow.hovered
                                }

                                function iconSource() {
                                    if (!modelData.icon || modelData.icon === "") {
                                        return "image://svgimage-custom-color/activity.svg/" + palette.windowText
                                    }
                                    return modelData.icon + "/" + palette.windowText
                                }

                                contentItem: RowLayout {
                                    spacing: 8

                                    Image {
                                        Layout.preferredWidth: Style.trayAccountPopupSyncIconSize
                                        Layout.preferredHeight: Style.trayAccountPopupSyncIconSize
                                        Layout.alignment: Qt.AlignVCenter
                                        source: recentActivityRow.iconSource()
                                        sourceSize.width: Style.trayAccountPopupSyncIconSize
                                        sourceSize.height: Style.trayAccountPopupSyncIconSize
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 0

                                        EnforcedPlainTextLabel {
                                            Layout.fillWidth: true
                                            text: recentActivityRow.text
                                            font.pixelSize: recentActivityRow.font.pixelSize
                                            font.weight: Font.DemiBold
                                            color: palette.windowText
                                            elide: Text.ElideRight
                                            maximumLineCount: 1
                                        }

                                        EnforcedPlainTextLabel {
                                            Layout.fillWidth: true
                                            text: recentActivityRow.rowSubtitle
                                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                                            color: palette.windowText
                                            opacity: 0.7
                                            elide: Text.ElideRight
                                            maximumLineCount: 1
                                            visible: text !== ""
                                        }

                                        EnforcedPlainTextLabel {
                                            Layout.fillWidth: true
                                            text: recentActivityRow.rowDateTime
                                            font.pixelSize: Style.trayAccountPopupSecondaryFontSize
                                            color: palette.windowText
                                            opacity: 0.65
                                            horizontalAlignment: Text.AlignLeft
                                            elide: Text.ElideRight
                                            visible: text !== ""
                                        }
                                    }
                                }

                                onHoveredChanged: {
                                    if (hovered) {
                                        accountActionsMenu.closeSubmenus()
                                    }
                                }

                                onClicked: accountDelegate.openActivities()

                                Accessible.role: Accessible.Button
                                Accessible.name: text
                                Accessible.onPressAction: recentActivityRow.clicked()
                            }
                        }

                        MenuItem {
                            id: noRecentActivitiesRow

                            visible: accountDelegate.recentActivities.length === 0
                            height: visible ? implicitHeight : 0
                            enabled: false
                            text: qsTr("No recent activity")
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: false
                            background: Item {}
                            contentItem: RowLayout {
                                spacing: 8

                                Image {
                                    Layout.preferredWidth: Style.trayAccountPopupSyncIconSize
                                    Layout.preferredHeight: Style.trayAccountPopupSyncIconSize
                                    Layout.alignment: Qt.AlignVCenter
                                    source: "image://svgimage-custom-color/activity.svg/" + palette.windowText
                                    sourceSize.width: Style.trayAccountPopupSyncIconSize
                                    sourceSize.height: Style.trayAccountPopupSyncIconSize
                                }

                                EnforcedPlainTextLabel {
                                    Layout.fillWidth: true
                                    text: noRecentActivitiesRow.text
                                    font: noRecentActivitiesRow.font
                                    color: palette.windowText
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                }
                            }

                            Accessible.role: Accessible.StaticText
                            Accessible.name: text
                        }

                        MenuItem {
                            id: moreActivitiesButton

                            text: qsTr("More activity…")
                            font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                            hoverEnabled: true
                            onHoveredChanged: {
                                if (hovered) {
                                    accountActionsMenu.closeSubmenus()
                                }
                            }
                            background: TrayActionHoverBackground {
                                active: moreActivitiesButton.hovered
                            }
                            onClicked: accountDelegate.openActivities()

                            Accessible.role: Accessible.Button
                            Accessible.name: text
                            Accessible.onPressAction: moreActivitiesButton.clicked()
                        }
                    }

                    AutoSizingMenu {
                        id: appsMenu

                        property var anchorRow: null

                        width: Style.trayAccountAppsMenuWidth
                        topPadding: Style.trayAccountPopupActionVerticalPadding
                        bottomPadding: Style.trayAccountPopupActionVerticalPadding
                        focus: false
                        modal: false
                        closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape
                        onHeightChanged: accountActionsMenu.repositionOpenSubmenu(appsMenu)

                        Repeater {
                            model: TrayAccountAppsModel

                            delegate: MenuItem {
                                id: appEntry

                                text: model.appName
                                font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                                hoverEnabled: true

                                function appIconSource() {
                                    if (!model.appIconUrl || model.appIconUrl === "") {
                                        return ""
                                    }
                                    return "image://tray-image-provider/" + model.appIconUrl + "/" + palette.windowText
                                }

                                background: TrayActionHoverBackground {
                                    active: appEntry.hovered
                                }

                                contentItem: RowLayout {
                                    spacing: 8

                                    Image {
                                        Layout.preferredWidth: Style.trayAccountPopupSyncIconSize
                                        Layout.preferredHeight: Style.trayAccountPopupSyncIconSize
                                        source: appEntry.appIconSource()
                                        sourceSize.width: Style.trayAccountPopupSyncIconSize
                                        sourceSize.height: Style.trayAccountPopupSyncIconSize
                                        fillMode: Image.PreserveAspectFit
                                    }

                                    EnforcedPlainTextLabel {
                                        Layout.fillWidth: true
                                        text: appEntry.text
                                        font: appEntry.font
                                        color: palette.windowText
                                        elide: Text.ElideRight
                                    }
                                }

                                onTriggered: {
                                    root._closing = true
                                    appsMenu.close()
                                    accountActionsMenu.close()
                                    Systray.hideWindow()
                                    TrayAccountAppsModel.openAppUrl(model.appUrl)
                                }

                                Accessible.role: Accessible.MenuItem
                                Accessible.name: qsTr("Open %1 in browser").arg(model.appName)
                                Accessible.onPressAction: appEntry.triggered()
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
                            layer.enabled: visible && status === Image.Ready
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
                        accountDelegate.openActivities()
                    }
                }

                    ItemDelegate {
                        id: accountAlertBox

                        visible: accountDelegate.hasAccountAlert
                        width: root.width
                        height: visible ? Math.max(Style.trayAccountPopupActionHeight,
                                                   accountAlertLabel.implicitHeight
                                                   + (2 * Style.trayAccountPopupAccountHoverVerticalMargin)) : 0
                        hoverEnabled: true
                        topInset: 0
                        leftInset: 0
                        rightInset: 0
                        bottomInset: 0
                        padding: 0
                        leftPadding: Style.trayAccountPopupRowPadding
                        rightPadding: Style.trayAccountPopupRowPadding

                        background: Item {}

                        contentItem: RowLayout {
                            spacing: Style.trayAccountPopupRowSpacing

                            Item {
                                Layout.preferredWidth: Style.trayAccountPopupAvatarSize
                                Layout.fillHeight: true
                            }

                            EnforcedPlainTextLabel {
                                id: accountAlertLabel

                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: accountDelegate.accountAlertTitle
                                font.pixelSize: Style.trayAccountPopupSecondaryFontSize
                                font.weight: Font.DemiBold
                                color: palette.windowText
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            Button {
                                id: accountAlertResolveButton

                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: Math.max(implicitWidth, 82)
                                Layout.preferredHeight: Style.trayAccountPopupActionHeight
                                text: qsTr("Resolve")
                                font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                                onClicked: accountDelegate.openActivities()

                                background: Rectangle {
                                    radius: Style.mediumRoundedButtonRadius
                                    color: accountAlertResolveButton.hovered || accountAlertResolveMouseArea.containsMouse ? palette.mid : palette.button
                                    border.color: palette.mid
                                    border.width: Style.trayWindowBorderWidth
                                }

                                contentItem: EnforcedPlainTextLabel {
                                    text: accountAlertResolveButton.text
                                    font: accountAlertResolveButton.font
                                    color: palette.buttonText
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                MouseArea {
                                    id: accountAlertResolveMouseArea

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: accountDelegate.openActivities()
                                }

                                Accessible.role: Accessible.Button
                                Accessible.name: text
                                Accessible.onPressAction: accountAlertResolveButton.clicked()
                            }
                        }

                        onHoveredChanged: {
                            if (hovered) {
                                root.closeActiveAccountActionsMenu()
                            }
                        }

                        onClicked: accountDelegate.openActivities()

                        Accessible.role: Accessible.Button
                        Accessible.name: accountDelegate.accountAlertTitle
                        Accessible.onPressAction: accountAlertBox.clicked()
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

                background: TrayActionHoverBackground {
                    active: addAccountRow.hovered
                }

                contentItem: EnforcedPlainTextLabel {
                    text: qsTr("Add account")
                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onHoveredChanged: {
                    if (hovered) {
                        root.closeActiveTraySubmenus()
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

                background: TrayActionHoverBackground {
                    active: settingsRow.hovered
                }

                contentItem: EnforcedPlainTextLabel {
                    text: qsTr("Settings")
                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onHoveredChanged: {
                    if (hovered) {
                        root.closeActiveTraySubmenus()
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

                background: TrayActionHoverBackground {
                    active: quitRow.hovered
                }

                contentItem: EnforcedPlainTextLabel {
                    text: qsTr("Quit")
                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onHoveredChanged: {
                    if (hovered) {
                        root.closeActiveTraySubmenus()
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
