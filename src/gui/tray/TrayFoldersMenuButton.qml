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
import Qt5Compat.GraphicalEffects
import Style 1.0

HeaderButton {
    id: root

    implicitWidth: contentItem.implicitWidth

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

    contentItem: Item {
        id: rootContent

        anchors.fill: parent

        Item {
            id: contentContainer
            anchors.centerIn: parent

            implicitWidth: openLocalFolderButtonCaretIconLoader.active ? openLocalFolderButtonIcon.width + openLocalFolderButtonCaretIconLoader.width : openLocalFolderButtonIcon.width
            implicitHeight: openLocalFolderButtonIcon.height

            Image {
                id: folderStateIndicator
                visible: root.currentUser.hasLocalFolder
                source: root.currentUser.isConnected ? Style.stateOnlineImageSource : Style.stateOfflineImageSource
                cache: false

                anchors.bottom: openLocalFolderButtonIcon.bottom
                anchors.bottomMargin: Style.trayFoldersMenuButtonStateIndicatorBottomOffset
                anchors.right: openLocalFolderButtonIcon.right
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

            Image {
                id: openLocalFolderButtonIcon

                property int imageWidth: rootContent.width * Style.trayFoldersMenuButtonMainIconSizeFraction
                property int imageHeight: rootContent.width * Style.trayFoldersMenuButtonMainIconSizeFraction

                cache: true

                source: "image://svgimage-custom-color/folder.svg/" + Style.currentUserHeaderTextColor
                sourceSize {
                    width: imageWidth
                    height: imageHeight
                }

                width: imageWidth
                height: imageHeight

                anchors.verticalCenter: parent.verticalCenter
            }


            Loader {
                id: openLocalFolderButtonCaretIconLoader

                active: root.userHasGroupFolders
                visible: active

                anchors.left: openLocalFolderButtonIcon.right
                anchors.verticalCenter: openLocalFolderButtonIcon.verticalCenter

                property int imageWidth: rootContent.width * Style.trayFoldersMenuButtonDropDownCaretIconSizeFraction
                property int imageHeight: rootContent.width * Style.trayFoldersMenuButtonDropDownCaretIconSizeFraction

                sourceComponent: Image {
                    id: openLocalFolderButtonCaretIcon

                    cache: true

                    source: "image://svgimage-custom-color/caret-down.svg/" + Style.currentUserHeaderTextColor
                    sourceSize {
                        width: openLocalFolderButtonCaretIconLoader.imageWidth
                        height: openLocalFolderButtonCaretIconLoader.imageHeight
                    }
                }

                width: openLocalFolderButtonCaretIconLoader.imageWidth
                height: openLocalFolderButtonCaretIconLoader.imageHeight

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
                        backgroundIconSource: "image://svgimage-custom-color/folder.svg/" + palette.buttonText
                        iconSource: isGroupFolder
                                    ? "image://svgimage-custom-color/account-group.svg/" + palette.brightText
                                    : ""

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
