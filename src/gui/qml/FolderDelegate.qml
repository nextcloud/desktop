/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
import QtQuick.Controls
import QtQuick.Layouts
import org.ownCloud.qmlcomponents 1.0

Pane {
    // TODO: not cool
    readonly property real normalSize: 170

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn

        ListView {
            anchors.fill: parent
            model: ctx.model

            spacing: 20

            delegate: Pane {
                id: folderDelegate

                // model index
                required property int index

                required property string displayName
                required property string subtitle
                required property url imageUrl
                required property url statusUrl
                required property double progress
                required property string overallText
                required property string itemText
                required property var errorMsg
                required property string quota
                required property string toolTip

                required property Folder folder

                clip: true
                width: ListView.view.width - scrollView.ScrollBar.vertical.width - 10

                implicitHeight: normalSize

                ToolTip.text: folderDelegate.toolTip
                ToolTip.visible: hovered
                ToolTip.delay: 1000
                ToolTip.timeout: 5000
                hoverEnabled: true
                background: Rectangle {
                    color: folderDelegate.ListView.isCurrentItem ? scrollView.palette.highlight : scrollView.palette.base
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton) {
                            ctx.slotCustomContextMenuRequested(folder);
                        }
                        folderDelegate.ListView.view.currentIndex = folderDelegate.index;
                        folderDelegate.forceActiveFocus();
                    }
                }

                RowLayout {
                    anchors.fill: parent

                    spacing: 10
                    SpaceDelegate {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        title: displayName
                        description: subtitle
                        imageSource: imageUrl
                        statusSource: statusUrl

                        // we will either display quota or overallText
                        Label {
                            Layout.fillWidth: true
                            text: folderDelegate.quota
                            elide: Text.ElideRight
                            visible: folderDelegate.quota && !folderDelegate.overallText
                        }

                        Item {
                            // ensure the progress bar always consumes its space
                            Layout.preferredHeight: 10
                            Layout.fillWidth: true
                            ProgressBar {
                                anchors.fill: parent
                                value: folderDelegate.progress
                                visible: folderDelegate.overallText || folderDelegate.itemText
                            }
                        }

                        Label {

                            Layout.fillWidth: true
                            text: folderDelegate.overallText
                            elide: Text.ElideMiddle
                        }

                        Label {
                            Layout.fillWidth: true
                            text: folderDelegate.itemText
                            elide: Text.ElideMiddle
                            // only display the item text if we don't have errors
                            // visible:  !folderDelegate.errorMsg.length
                        }

                        FolderError {
                            Layout.fillWidth: true
                            errorMessages: folderDelegate.errorMsg
                            onCollapsedChanged: {
                                if (!collapsed) {
                                    // TODO: not cool
                                    folderDelegate.implicitHeight = normalSize + implicitHeight + 10;
                                } else {
                                    folderDelegate.implicitHeight = normalSize;
                                }
                            }
                        }
                    }
                    Button {
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        Layout.maximumHeight: 30
                        display: AbstractButton.IconOnly
                        icon.source: "image://ownCloud/core/more"
                        onClicked: {
                            ctx.slotCustomContextMenuRequested(folder);
                        }
                    }
                }
            }
        }
    }
}
