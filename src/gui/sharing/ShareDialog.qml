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
    width: Style.sharingDialogWidth
    height: Style.sharingDialogHeight
    minimumWidth: Style.sharingDialogMinimumWidth
    minimumHeight: Style.sharingDialogMinimumHeight

    function reconcileSelectedShare() {
        const shares = sharingController.shares
        if (dialog.selectedShare && shares.indexOf(dialog.selectedShare) !== -1) {
            return
        }

        dialog.selectedShare = shares.length > 0 ? shares[0] : null
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
        anchors.topMargin: Style.standardSpacing
        spacing: 0

        EnforcedPlainTextLabel {
            Layout.leftMargin: Style.sharingDialogWindowMargin
            Layout.rightMargin: Style.sharingDialogWindowMargin
            Layout.bottomMargin: Style.standardSpacing

            text: dialog.title
            elide: Text.ElideRight
            font.pointSize: Style.titleFontPtSize
            font.weight: Font.DemiBold
            color: palette.text
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Style.normalBorderWidth
            color: Style.sharingDialogSeparatorColor
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: Style.normalBorderWidth
                color: Style.sharingDialogSeparatorColor
            }

            MainPage {
                id: mainPage

                SplitView.minimumWidth: Style.sharingDialogSidebarMinimumWidth
                SplitView.preferredWidth: Style.sharingDialogSidebarPreferredWidth
                SplitView.maximumWidth: Style.sharingDialogSidebarMaximumWidth

                sharingController: sharingController
                fileId: dialog.fileId
                selectedShare: dialog.selectedShare

                onShareSelected: share => {
                    dialog.selectedShare = share
                }
            }

            Page {
                SplitView.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Style.standardSpacing
                    anchors.rightMargin: Style.sharingDialogWindowMargin + Style.standardSpacing
                    anchors.topMargin: Style.standardSpacing
                    anchors.bottomMargin: Style.standardSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Style.sharingDialogPaneHeaderHeight

                        EnforcedPlainTextLabel {
                            Layout.alignment: Qt.AlignVCenter

                            text: qsTr("Share details")
                            elide: Text.ElideRight
                            font.pointSize: Style.subheaderFontPtSize
                            font.weight: Font.DemiBold
                            color: palette.text
                            Layout.fillWidth: true
                        }

                        ToolButton {
                            visible: dialog.selectedShare !== null
                            enabled: !sharingController.destroyingShare
                            display: AbstractButton.IconOnly
                            icon.source: "image://svgimage-custom-color/delete.svg/" + palette.buttonText
                            Accessible.name: qsTr("Delete share")
                            ToolTip.visible: hovered
                            ToolTip.text: Accessible.name

                            onClicked: deleteShareConfirmation.open()
                        }
                    }

                    EnforcedPlainTextLabel {
                        Layout.fillWidth: true
                        text: sharingController.shareDestructionError
                        color: Style.wizardErrorText
                        wrapMode: Text.Wrap
                        visible: text.length > 0
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Loader {
                            anchors.fill: parent

                            active: dialog.selectedShare !== null
                            sourceComponent: detailsComponent
                        }

                        EnforcedPlainTextLabel {
                            anchors.fill: parent

                            text: qsTr("Select a share to view its details.")
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.Wrap
                            visible: dialog.selectedShare === null
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

    Dialog {
        id: deleteShareConfirmation

        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete share?")

        EnforcedPlainTextLabel {
            width: parent.width
            text: qsTr("This removes the share and its access for all recipients.")
            wrapMode: Text.Wrap
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Delete")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: deleteShareConfirmation.accept()
            }

            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: deleteShareConfirmation.reject()
            }
        }

        onAccepted: {
            sharingController.destroyShare(dialog.selectedShare)
            close()
        }
    }

}
