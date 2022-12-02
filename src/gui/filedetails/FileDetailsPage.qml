/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import com.nextcloud.desktopclient 1.0
import Style 1.0
import "../tray"

Page {
    id: root

    property var accountState: ({})
    property string localPath: ({})

    // We want the SwipeView to "spill" over the edges of the window to really
    // make it look nice. If we apply page-wide padding, however, the swipe
    // contents only go as far as the page contents, clipped by the padding.
    // This property reflects the padding we intend to display, but not the real
    // padding, which we have to apply selectively to achieve our desired effect.
    property int intendedPadding: Style.standardSpacing * 2
    property int iconSize: 32

    property FileDetails fileDetails: FileDetails {
        id: fileDetails
        localPath: root.localPath
    }

    Connections {
        target: Systray
        function onShowFileDetailsPage(fileLocalPath, page) {
            if(fileLocalPath === root.localPath) {
                switch(page) {
                case Systray.FileDetailsPage.Activity:
                    swipeView.currentIndex = fileActivityView.swipeIndex;
                    break;
                case Systray.FileDetailsPage.Sharing:
                    swipeView.currentIndex = shareView.swipeIndex;
                    break;
                }
            }
        }
    }

    topPadding: intendedPadding
    bottomPadding: intendedPadding

    background: Rectangle {
        color: Style.backgroundColor
    }

    header: ColumnLayout {
        spacing: root.intendedPadding

        GridLayout {
            id: headerGridLayout

            readonly property bool showFileLockedString: root.fileDetails.lockExpireString !== ""

            Layout.fillWidth: parent
            Layout.topMargin: root.topPadding

            columns: 2
            rows: showFileLockedString ? 3 : 2

            rowSpacing: Style.standardSpacing / 2
            columnSpacing: Style.standardSpacing

            Image {
                id: fileIcon

                Layout.rowSpan: headerGridLayout.rows
                Layout.preferredWidth: Style.trayListItemIconSize
                Layout.leftMargin: root.intendedPadding
                Layout.fillHeight: true

                verticalAlignment: Image.AlignVCenter
                horizontalAlignment: Image.AlignHCenter
                source: root.fileDetails.iconUrl
                sourceSize.width: Style.trayListItemIconSize
                sourceSize.height: Style.trayListItemIconSize
                fillMode: Image.PreserveAspectFit
            }

            EnforcedPlainTextLabel {
                id: fileNameLabel

                Layout.fillWidth: true
                Layout.rightMargin: root.intendedPadding

                text: root.fileDetails.name
                color: Style.ncTextColor
                font.bold: true
                wrapMode: Text.Wrap
            }

            EnforcedPlainTextLabel {
                id: fileDetailsLabel

                Layout.fillWidth: true
                Layout.rightMargin: root.intendedPadding

                text: `${root.fileDetails.sizeString} · ${root.fileDetails.lastChangedString}`
                color: Style.ncSecondaryTextColor
                wrapMode: Text.Wrap
            }

            EnforcedPlainTextLabel {
                id: fileLockedLabel

                Layout.fillWidth: true
                Layout.rightMargin: root.intendedPadding

                text: root.fileDetails.lockExpireString
                color: Style.ncSecondaryTextColor
                wrapMode: Text.Wrap
                visible: headerGridLayout.showFileLockedString
            }
        }

        TabBar {
            id: viewBar

            Layout.leftMargin: root.intendedPadding
            Layout.rightMargin: root.intendedPadding

            padding: 0
            background: Rectangle {
                color: Style.backgroundColor
            }

            NCTabButton {
                svgCustomColorSource: "image://svgimage-custom-color/activity.svg"
                text: qsTr("Activity")
                checked: swipeView.currentIndex === fileActivityView.swipeIndex
                onClicked: swipeView.currentIndex = fileActivityView.swipeIndex
            }

            NCTabButton {
                svgCustomColorSource: "image://svgimage-custom-color/share.svg"
                text: qsTr("Sharing")
                checked: swipeView.currentIndex === shareView.swipeIndex
                onClicked: swipeView.currentIndex = shareView.swipeIndex
            }
        }
    }

    SwipeView {
        id: swipeView

        anchors.fill: parent
        clip: true

        FileActivityView {
            id: fileActivityView

            property int swipeIndex: SwipeView.index

            delegateHorizontalPadding: root.intendedPadding

            accountState: root.accountState
            localPath: root.localPath
            iconSize: root.iconSize
        }

        ShareView {
            id: shareView

            property int swipeIndex: SwipeView.index

            accountState: root.accountState
            localPath: root.localPath
            fileDetails: root.fileDetails
            horizontalPadding: root.intendedPadding
            iconSize: root.iconSize
        }
    }
}
