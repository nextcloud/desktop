/*
 * Copyright (C) 2023 by Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

import QtQml
import QtQuick
import QtQuick.Window as QtWindow
import QtQuick.Layouts
import QtQuick.Controls
import QtQml.Models
import Style
import com.nextcloud.desktopclient
import "./tray"

ApplicationWindow {
    id: conflictsDialog

    required property var allConflicts

    flags: Qt.Window | Qt.Dialog
    visible: true

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    width: Style.minimumWidthResolveConflictsDialog
    height: Style.minimumHeightResolveConflictsDialog
    minimumWidth: Style.minimumWidthResolveConflictsDialog
    minimumHeight: Style.minimumHeightResolveConflictsDialog
    title: qsTr('Solve sync conflicts')

    onClosing: function(close) {
        Systray.destroyDialog(self);
        close.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        anchors.topMargin: 20
        spacing: 15
        z: 2

        EnforcedPlainTextLabel {
            text: qsTr("%1 files in conflict", 'indicate the number of conflicts to resolve', delegateModel.count).arg(delegateModel.count)
            font.bold: true
            font.pixelSize: Style.bigFontPixelSizeResolveConflictsDialog
            Layout.fillWidth: true
        }

        EnforcedPlainTextLabel {
            text: qsTr("Choose if you want to keep the local version, server version, or both. If you choose both, the local file will have a number added to its name.")
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            font.pixelSize: Style.fontPixelSizeResolveConflictsDialog
            Layout.fillWidth: true
            Layout.topMargin: -15
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 15

            CheckBox {
                id: selectExisting

                Layout.fillWidth: true

                text: qsTr('All local versions')

                leftPadding: 0
                implicitWidth: 100

                font.pixelSize: Style.fontPixelSizeResolveConflictsDialog

                checked: realModel.allConflictingSelected
                onToggled: function() {
                    realModel.selectAllConflicting(checked)
                }
            }

            CheckBox {
                id: selectConflict

                Layout.fillWidth: true

                text: qsTr('All server versions')

                leftPadding: 0
                implicitWidth: 100

                font.pixelSize: Style.fontPixelSizeResolveConflictsDialog

                checked: realModel.allExistingsSelected
                onToggled: function() {
                    realModel.selectAllExisting(checked)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 5
            Layout.rightMargin: 5
            color: palette.dark
            height: 1
        }

        SyncConflictsModel {
            id: realModel

            conflictActivities: conflictsDialog.allConflicts
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ListView {
                id: conflictListView

                model: DelegateModel {
                    id: delegateModel

                    model: realModel

                    delegate: ConflictDelegate {
                        width: conflictListView.contentItem.width
                        height: 100
                    }
                }
            }
        }

        DialogButtonBox {
            Layout.fillWidth: true

            Button {
                text: qsTr("Resolve conflicts")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            onAccepted: function() {
                realModel.applySolution()
                Systray.destroyDialog(conflictsDialog)
            }

            onRejected: function() {
                Systray.destroyDialog(conflictsDialog)
            }
        }
    }

    Rectangle {
        color: palette.base
        anchors.fill: parent
        z: 1
    }
}
