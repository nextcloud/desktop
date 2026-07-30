/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "qrc:/qml/src/gui"
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

WizardStyledWindow {
    id: dialog
    visible: true

    property var account
    property string localPath: ""
    property string shortLocalPath: dialog.localPath.split("/").reverse()[0]
    property string fileId: ""
    property Share selectedShare: null

    title: qsTr("Share \"%1\"").arg(dialog.shortLocalPath)
    width: 720
    height: 500
    minimumWidth: 600
    minimumHeight: Style.wizardStandaloneWindowMinimumHeight

    function reconcileSelectedShare() {
        const shares = sharingController.shares
        if (dialog.selectedShare && shares.indexOf(dialog.selectedShare) !== -1) {
            return
        }

        dialog.selectedShare = shares.length > 0 ? shares[0] : null
        settingsPopup.close()
    }

    SharingController {
        id: sharingController
        account: dialog.account
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: dialog.close()
    }

    Component.onCompleted: {
        sharingController.initialize(dialog.fileId)
    }

    Connections {
        target: sharingController

        function onSharesChanged() {
            dialog.reconcileSelectedShare()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Style.wizardWindowMargin
        anchors.rightMargin: Style.wizardWindowMargin
        anchors.topMargin: Style.wizardWindowTopMargin
        anchors.bottomMargin: Style.wizardWindowMargin
        spacing: Style.wizardSectionSpacing

        EnforcedPlainTextLabel {
            text: dialog.title
            elide: Text.ElideRight
            font.pixelSize: Style.wizardHeaderTitleFontPixelSize
            font.weight: Font.DemiBold
            color: palette.text
            Layout.fillWidth: true
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 1
                color: Style.wizardRowBorder
            }

            MainPage {
                id: mainPage

                SplitView.minimumWidth: 180
                SplitView.preferredWidth: 240
                SplitView.maximumWidth: 320

                sharingController: sharingController
                fileId: dialog.fileId
                selectedShare: dialog.selectedShare

                onShareSelected: share => {
                    dialog.selectedShare = share
                    settingsPopup.close()
                }
            }

            Page {
                SplitView.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Style.standardSpacing

                    RowLayout {
                        Layout.fillWidth: true

                        EnforcedPlainTextLabel {
                            text: qsTr("Share details")
                            elide: Text.ElideRight
                            font.pixelSize: Style.wizardHeaderTitleFontPixelSize
                            font.weight: Font.DemiBold
                            color: palette.text
                            Layout.fillWidth: true
                        }

                        ToolButton {
                            icon.source: "image://svgimage-custom-color/settings.svg/" + palette.windowText

                            visible: dialog.selectedShare && dialog.selectedShare.properties.length > 0
                            onClicked: settingsPopup.open()
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Loader {
                            anchors.fill: parent

                            active: dialog.selectedShare !== null
                            sourceComponent: detailsComponent
                        }

                        Label {
                            anchors.fill: parent

                            text: qsTr("Select a share to view its details.")
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.Wrap
                            visible: dialog.selectedShare === null
                        }

                        ShareSettingsPopup {
                            id: settingsPopup

                            sharingController: sharingController
                            share: dialog.selectedShare
                        }
                    }
                }
            }
        }
    }

    Component {
        id: detailsComponent

        ShareDetailsPage {
            sharingController: sharingController
            share: dialog.selectedShare
        }
    }

}
