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
    required property int index

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
            id: openFolderLabel
            Layout.fillWidth: true
            Layout.columnSpan: 2
            Layout.preferredWidth: 1
            elide: Text.ElideRight
            text: root.ListView.view.model.model.fileManagerText()
            Layout.leftMargin: Style.resolveConflictsLabelMargin
            color: Style.ncBlue
            font.underline: true

            MouseArea {
                id: linkMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                
                onClicked: {
                    let success = root.ListView.view.model.model.openConflictFolder(index);
                    
                    if (!success) {
                        parent.text = qsTr("Folder not found");
                        parent.color = Style.wizardErrorText;
                        
                        openFolderLabel.font.underline = false;
                        linkMouseArea.cursorShape = Qt.ArrowCursor;
                        linkMouseArea.enabled = false;
                    }
                }
            }
        }
    }
}
