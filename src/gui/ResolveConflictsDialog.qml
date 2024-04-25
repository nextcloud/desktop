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

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Window 2.15 as QtWindow
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtQml.Models 2.15
import Style 1.0
import com.nextcloud.desktopclient 1.0
import "./tray"

ApplicationWindow {
    id: conflictsDialog

    required property var allConflicts

    flags: Qt.Window | Qt.Dialog
    visible: true

    width: Style.minimumWidthResolveConflictsDialog
    height: Style.minimumHeightResolveConflictsDialog
    minimumWidth: Style.minimumWidthResolveConflictsDialog
    minimumHeight: Style.minimumHeightResolveConflictsDialog
    title: qsTr('Solve sync conflicts')

    // TODO: Rather than setting all these palette colours manually,
    // create a custom style and do it for all components globally
    palette {
        text: Style.ncTextColor
        windowText: Style.ncTextColor
        buttonText: Style.ncTextColor
        brightText: Style.ncTextBrightColor
        highlight: Style.lightHover
        highlightedText: Style.ncTextColor
        light: Style.lightHover
        midlight: Style.ncSecondaryTextColor
        mid: Style.darkerHover
        dark: Style.menuBorder
        button: Style.buttonBackgroundColor
        window: Style.backgroundColor
        base: Style.backgroundColor
        toolTipBase: Style.backgroundColor
        toolTipText: Style.ncTextColor
    }

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

                palette {
                    text: Style.ncTextColor
                    windowText: Style.ncTextColor
                    buttonText: Style.ncTextColor
                    brightText: Style.ncTextBrightColor
                    highlight: Style.lightHover
                    highlightedText: Style.ncTextColor
                    light: Style.lightHover
                    midlight: Style.ncSecondaryTextColor
                    mid: Style.darkerHover
                    dark: Style.menuBorder
                    button: Style.buttonBackgroundColor
                    window: palette.dark // NOTE: Fusion theme uses darker window colour for the border of the checkbox
                    base: Style.backgroundColor
                    toolTipBase: Style.backgroundColor
                    toolTipText: Style.ncTextColor
                }

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

                palette {
                    text: Style.ncTextColor
                    windowText: Style.ncTextColor
                    buttonText: Style.ncTextColor
                    brightText: Style.ncTextBrightColor
                    highlight: Style.lightHover
                    highlightedText: Style.ncTextColor
                    light: Style.lightHover
                    midlight: Style.ncSecondaryTextColor
                    mid: Style.darkerHover
                    dark: Style.menuBorder
                    button: Style.buttonBackgroundColor
                    window: palette.dark // NOTE: Fusion theme uses darker window colour for the border of the checkbox
                    base: Style.backgroundColor
                    toolTipBase: Style.backgroundColor
                    toolTipText: Style.ncTextColor
                }

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
        color: Style.backgroundColor
        anchors.fill: parent
        z: 1
    }
}
