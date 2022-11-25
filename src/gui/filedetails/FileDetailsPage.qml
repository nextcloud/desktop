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

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "../tray"

Page {
    id: root

    signal closeButtonClicked

    property var accountState: ({})
    property string localPath: ""

    // We want the SwipeView to "spill" over the edges of the window to really
    // make it look nice. If we apply page-wide padding, however, the swipe
    // contents only go as far as the page contents, clipped by the padding.
    // This property reflects the padding we intend to display, but not the real
    // padding, which we have to apply selectively to achieve our desired effect.
    property int intendedPadding: Style.standardSpacing * 2
    property int iconSize: 32
    property StackView rootStackView: StackView {}
    property bool showCloseButton: false
    property bool backgroundsVisible: true
    property color accentColor: Style.ncBlue

    property FileDetails fileDetails: FileDetails {
        id: fileDetails
        localPath: root.localPath
    }

    Connections {
        target: Systray
        function onShowFileDetailsPage(fileLocalPath, page) {
            if (!root.fileDetails.sharingAvailable && page == Systray.FileDetailsPage.Sharing) {
                return;
            }

            if (fileLocalPath === root.localPath) {
                switch(page) {
                case Systray.FileDetailsPage.Activity:
                    swipeView.currentIndex = fileActivityView.swipeIndex;
                    break;
                case Systray.FileDetailsPage.Sharing:
                    swipeView.currentIndex = shareViewLoader.swipeIndex;
                    break;
                }
            }
        }
    }

    topPadding: intendedPadding
    bottomPadding: intendedPadding

    background: Rectangle {
        color: palette.base
        visible: root.backgroundsVisible
    }

    header: ColumnLayout {
        spacing: root.intendedPadding

        GridLayout {
            id: headerGridLayout

            readonly property bool showFileLockedString: root.fileDetails.lockExpireString !== ""
            readonly property int textRightMargin: root.showCloseButton ? root.intendedPadding : 0

            Layout.fillWidth: parent
            Layout.topMargin: root.topPadding

            columns: root.showCloseButton ? 3 : 2
            rows: {
                let rows = 2;

                if (showFileLockedString) {
                    rows++;
                }

                if (root.fileDetails.fileTagModel.totalTags > 0) {
                    rows++;
                }

                return rows;
            }

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
                Layout.rightMargin: headerGridLayout.textRightMargin

                text: root.fileDetails.name
                font.bold: true
                wrapMode: Text.Wrap
            }

            Button {
                id: closeButton

                Layout.rowSpan: headerGridLayout.rows
                Layout.preferredWidth: Style.activityListButtonWidth
                Layout.preferredHeight: Style.activityListButtonHeight
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.rightMargin: headerGridLayout.textRightMargin

                icon.source: "image://svgimage-custom-color/clear.svg" + "/" + palette.buttonText
                icon.width: Style.activityListButtonIconSize
                icon.height: Style.activityListButtonIconSize
                visible: root.showCloseButton
                onClicked: root.closeButtonClicked()
            }

            EnforcedPlainTextLabel {
                id: fileDetailsLabel

                Layout.fillWidth: true
                Layout.rightMargin: headerGridLayout.textRightMargin

                text: `${root.fileDetails.sizeString} · ${root.fileDetails.lastChangedString}`
                wrapMode: Text.Wrap
            }

            EnforcedPlainTextLabel {
                id: fileLockedLabel

                Layout.fillWidth: true
                Layout.rightMargin: headerGridLayout.textRightMargin

                text: root.fileDetails.lockExpireString
                wrapMode: Text.Wrap
                visible: headerGridLayout.showFileLockedString
            }

            Row {
                id: tagRow

                Layout.fillWidth: true
                Layout.rightMargin: headerGridLayout.textRightMargin

                Repeater {
                    id: tagRepeater

                    readonly property var fileTagModel: root.fileDetails.fileTagModel
                    readonly property int maxTags: 3

                    model: fileTagModel
                    delegate: FileTag {
                        readonly property int availableLayoutSpace: tagRow.width - tagRow.spacing - overflowTag.width
                        readonly property int maxWidth: (availableLayoutSpace / tagRepeater.maxTags) - tagRow.spacing

                        width: Math.min(maxWidth, implicitWidth)
                        text: model.display
                    }

                    Component.onCompleted: fileTagModel.maxTags = 3
                }

                FileTag {
                    id: overflowTag

                    readonly property int totalFileTags: tagRepeater.fileTagModel.totalTags
                    readonly property int maxFileTags: tagRepeater.fileTagModel.maxTags

                    visible: totalFileTags > maxFileTags
                    text: "+" + String(totalFileTags - maxFileTags)

                    HoverHandler {
                        id: hoverHandler
                    }

                    ToolTip {
                        visible: hoverHandler.hovered
                        text: tagRepeater.fileTagModel.overflowTagsString
                    }
                }
            }
        }

        TabBar {
            id: viewBar

            Layout.leftMargin: root.intendedPadding
            Layout.rightMargin: root.intendedPadding

            padding: 0
            background: null

            NCTabButton {
                svgCustomColorSource: "image://svgimage-custom-color/activity.svg"
                text: qsTr("Activity")
                checked: swipeView.currentIndex === fileActivityView.swipeIndex
                onClicked: swipeView.currentIndex = fileActivityView.swipeIndex
            }

            NCTabButton {
                width: visible ? implicitWidth : 0
                height: visible ? implicitHeight : 0
                svgCustomColorSource: "image://svgimage-custom-color/share.svg"
                text: qsTr("Sharing")
                checked: swipeView.currentIndex === shareViewLoader.swipeIndex
                onClicked: swipeView.currentIndex = shareViewLoader.swipeIndex
                visible: root.fileDetails.sharingAvailable
            }
        }
    }

    SwipeView {
        id: swipeView

        anchors.fill: parent
        clip: true

        FileActivityView {
            id: fileActivityView

            readonly property int swipeIndex: SwipeView.index

            delegateHorizontalPadding: root.intendedPadding

            accountState: root.accountState
            localPath: root.localPath
            iconSize: root.iconSize
        }

        Loader {
            id: shareViewLoader

            readonly property int swipeIndex: SwipeView.index

            width: swipeView.width
            height: swipeView.height
            active: root.fileDetails.sharingAvailable

            sourceComponent: ShareView {
                id: shareView

                anchors.fill: parent

                accountState: root.accountState
                localPath: root.localPath
                fileDetails: root.fileDetails
                horizontalPadding: root.intendedPadding
                iconSize: root.iconSize
                rootStackView: root.rootStackView
                backgroundsVisible: root.backgroundsVisible
                accentColor: root.accentColor
            }
        }
    }
}
