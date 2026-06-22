/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 5
        anchors.topMargin: 10
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
            Layout.topMargin: 5

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
