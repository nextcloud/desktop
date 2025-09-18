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

    EnforcedPlainTextLabel {
        id: existingFileNameLabel

        anchors.top: parent.top
        anchors.left: parent.left

        text: root.existingFileName

        font.weight: Font.Bold
        font.pixelSize: Style.fontPixelSizeResolveConflictsDialog
    }

    RowLayout {
        anchors.top: existingFileNameLabel.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottomMargin: 8

        ConflictItemFileInfo {
            Layout.fillWidth: true
            Layout.fillHeight: true

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

            itemSelected: root.existingSelected
            itemPreviewUrl: root.existingPreviewUrl
            itemVersionLabel: qsTr('Server version')
            itemDateLabel: root.existingDate
            itemFileSizeLabel: root.existingSize

            onSelectedChanged: function() {
                model.existingSelected = itemSelected
            }
        }
    }
}
