/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Style
import com.nextcloud.desktopclient
import "./tray"

Item {
    id: root

    required property string existingFileName
    required property string existingSize
    required property string conflictSize
    required property string existingDate
    required property string conflictDate
    required property bool existingSelected
    required property bool conflictSelected
    required property url existingPreviewUrl
    required property url conflictPreviewUrl
    required property var model
    required property string existingFilePath
    required property string conflictFilePath

    EnforcedPlainTextLabel {
        id: existingFileNameLabel

        anchors.top: parent.top
        anchors.left: parent.left

        text: root.existingFileName

        font.weight: Font.Bold
        font.pixelSize: Style.fontPixelSizeResolveConflictsDialog
    }

    GridLayout {
        anchors.top: existingFileNameLabel.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottomMargin: 8
        
        columns: 2
        columnSpacing: Style.standardSpacing

        ConflictItemFileInfo {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1

            itemSelected: root.conflictSelected
            itemPreviewUrl: root.conflictPreviewUrl
            itemVersionLabel: qsTr('Local version')
            itemDateLabel: root.conflictDate
            itemFileSizeLabel: root.conflictSize

            onSelectedChanged: function() {
                model.conflictSelected = itemSelected
            }
        }

        ConflictItemFileInfo {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1

            itemSelected: root.existingSelected
            itemPreviewUrl: root.existingPreviewUrl
            itemVersionLabel: qsTr('Server version')
            itemDateLabel: root.existingDate
            itemFileSizeLabel: root.existingSize

            onSelectedChanged: function() {
                model.existingSelected = itemSelected
            }
        }
        
        EnforcedPlainTextLabel {
            text: qsTr("Open in File Manager")
            Layout.fillWidth: true
            Layout.columnSpan: 2
            Layout.preferredWidth: 1
            elide: Text.ElideRight
            
            Layout.leftMargin: 28
                        
            color: Style.ncBlue
            
            font.underline: true

            MouseArea {
                id: linkMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                
                onClicked: {
                    try {
                        var path = root.conflictFilePath;
                        
                        if (!path) {
                            console.warn("openContainingFolder: Path is empty or not available");
                            return;
                        }

                        path = path.replace(/\\/g, "/");
                        
                        var lastSlash = path.lastIndexOf("/");
                        var dir = lastSlash > 0 ? path.substring(0, lastSlash) : path;

                        var url = "file://" + dir;
                        Qt.openUrlExternally(url);
                        
                    } catch (e) {
                        console.warn("openContainingFolder Exception:", e);
                    }
                }
            }
        }
    }
}
