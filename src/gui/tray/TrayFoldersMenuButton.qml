/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.0
import Style 1.0

HeaderButton {
    id: root

    signal folderEntryTriggered(string fullFolderPath, bool isGroupFolder)

    required property var currentUser
    property bool userHasGroupFolders: currentUser.groupFolders.length > 0

    function openMenu() {
        foldersMenuLoader.openMenu()
    }

    function closeMenu() {
        foldersMenuLoader.closeMenu()
    }

    function toggleMenuOpen() {
        if (foldersMenuLoader.isMenuVisible) {
            closeMenu()
        } else {
            openMenu()
        }
    }

    visible: currentUser.hasLocalFolder
    display: AbstractButton.IconOnly
    flat: true

    Accessible.role: root.userHasGroupFolders ? Accessible.ButtonMenu : Accessible.Button
    Accessible.name: tooltip.text
    Accessible.onPressAction: root.clicked()

    NCToolTip {
        id: tooltip
        visible: root.hovered && !foldersMenuLoader.isMenuVisible
        text: root.userHasGroupFolders ? qsTr("Open local or group folders") : qsTr("Open local folder")
    }

    Image {
        id: folderStateIndicator
        visible: root.currentUser.hasLocalFolder
        source: root.currentUser.isConnected ? Style.stateOnlineImageSource : Style.stateOfflineImageSource
        cache: false

        anchors.top: root.verticalCenter
        anchors.left: root.horizontalCenter
        sourceSize.width: Style.folderStateIndicatorSize
        sourceSize.height: Style.folderStateIndicatorSize

        Accessible.role: Accessible.Indicator
        Accessible.name: root.currentUser.isConnected ? qsTr("Connected") : qsTr("Disconnected")
        z: 1

        Rectangle {
            id: folderStateIndicatorBackground
            width: Style.folderStateIndicatorSize + Style.trayFolderStatusIndicatorSizeOffset
            height: width
            anchors.centerIn: parent
            color: Style.currentUserHeaderColor
            radius: width * Style.trayFolderStatusIndicatorRadiusFactor
            z: -2
        }

        Rectangle {
            id: folderStateIndicatorBackgroundMouseHover
            width: Style.folderStateIndicatorSize + Style.trayFolderStatusIndicatorSizeOffset
            height: width
            anchors.centerIn: parent
            color: root.hovered ? Style.currentUserHeaderTextColor : "transparent"
            opacity: Style.trayFolderStatusIndicatorMouseHoverOpacityFactor
            radius: width * Style.trayFolderStatusIndicatorRadiusFactor
            z: -1
        }
    }

    RowLayout {
        id: openLocalFolderButtonRowLayout

        anchors.fill: parent
        spacing: 0

        Image {
            id: openLocalFolderButtonIcon
            cache: false
            source: "image://svgimage-custom-color/folder.svg/" + Style.currentUserHeaderTextColor

            verticalAlignment: Qt.AlignCenter

            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Group folder button")
            Layout.leftMargin: Style.trayHorizontalMargin
        }

        Loader {
            id: openLocalFolderButtonCaretIconLoader

            active: root.userHasGroupFolders
            visible: active

            sourceComponent: ColorOverlay {
                width: source.width
                height: source.height
                cached: true
                color: Style.currentUserHeaderTextColor
                source: Image {
                    source: "image://svgimage-custom-color/caret-down.svg/" + Style.currentUserHeaderTextColor
                    sourceSize.width: Style.accountDropDownCaretSize
                    sourceSize.height: Style.accountDropDownCaretSize

                    verticalAlignment: Qt.AlignCenter

                    Layout.alignment: Qt.AlignRight
                    Layout.margins: Style.accountDropDownCaretMargin
                }
            }
        }
    }

    Loader {
        id: foldersMenuLoader

        property var openMenu: function(){}
        property var closeMenu: function(){}
        property bool isMenuVisible: false

        anchors.fill: parent
        active: root.userHasGroupFolders
        visible: active

        sourceComponent: AutoSizingMenu {
            id: foldersMenu

            x: Style.trayWindowMenuOffsetX
            y: (root.y + root.height + Style.trayWindowMenuOffsetY)
            width: Style.trayWindowWidth * Style.trayWindowMenuWidthFactor
            height: implicitHeight + y > Style.trayWindowHeight ? Style.trayWindowHeight - y : implicitHeight
            closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

            contentItem: ScrollView {
                id: foldersMenuScrollView

                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                data: WheelHandler {
                    target: foldersMenuScrollView.contentItem
                }

                ListView {
                    id: foldersMenuListView

                    implicitHeight: contentHeight
                    model: root.currentUser.groupFolders
                    interactive: true
                    clip: true
                    currentIndex: foldersMenu.currentIndex
                    anchors.left: parent.left
                    anchors.right: parent.right

                    delegate: TrayFolderListItem {
                        id: groupFoldersEntry

                        property bool isGroupFolder: model.modelData.isGroupFolder

                        text: model.modelData.name
                        toolTipText: !isGroupFolder ? qsTr("Open local folder \"%1\"").arg(model.modelData.fullPath) : qsTr("Open group folder \"%1\"").arg(model.modelData.fullPath)
                        subline: model.modelData.parentPath
                        width: foldersMenuListView.width
                        height: Style.standardPrimaryButtonHeight
                        iconSource: !isGroupFolder ?
                                        "image://svgimage-custom-color/folder.svg/" + palette.buttonText :
                                        "image://svgimage-custom-color/folder-group.svg/" + palette.buttonText

                        onTriggered: {
                            foldersMenu.close();
                            root.folderEntryTriggered(model.modelData.fullPath, isGroupFolder);
                        }

                        Accessible.role: Accessible.MenuItem
                        Accessible.name: qsTr("Open %1 in file explorer").arg(title)
                        Accessible.onPressAction: groupFoldersEntry.triggered()
                    }

                    Accessible.role: Accessible.PopupMenu
                    Accessible.name: qsTr("User group and local folders menu")
                }
            }

            Component.onCompleted: {
                foldersMenuLoader.openMenu = open
                foldersMenuLoader.closeMenu = close
            }

            Connections {
                onVisibleChanged: foldersMenuLoader.isMenuVisible = visible
            }
        }
    }
}
