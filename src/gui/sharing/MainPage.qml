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
// import "qrc:/qml/src/gui"
// import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

Page {
    id: root

    property string localPath: ""
    property string shortLocalPath: ""
    required property SharingController sharingController
    property bool isLinkShare: false
    readonly property Share share: sharingController.shares.length === 1 ? sharingController.shares[0] : null

    title: qsTr("Share \"%1\"").arg(root.shortLocalPath)

    ColumnLayout {
        id: windowContent
        anchors.fill: parent

        ScrollView {
            Layout.fillHeight: true
            Layout.fillWidth: true

            ScrollBar.vertical.policy: propertyList.contentHeight > propertyList.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            contentWidth: availableWidth
            contentHeight: availableHeight
            rightPadding: ScrollBar.vertical.policy == ScrollBar.AlwaysOn ? ScrollBar.vertical.width + Style.standardSpacing : 0

            ColumnLayout {
                width: parent.width

                RecipientSearchField {
                    id: searchField
                    Layout.fillWidth: true

                    visible: !root.isLinkShare

                    account: root.sharingController.account

                    onRecipientSelected: (recipientType, recipientValue) => {
                        if (root.share) {
                            root.sharingController.addRecipient(root.share, recipientType, recipientValue)
                        }
                    }
                }

                WizardComboBox {
                    id: permissionPresetSelector
                    Layout.fillWidth: true

                    model: [
                        { preset: "view", text: qsTr("Can view") },
                        { preset: "edit", text: qsTr("Can edit") },
                        { preset: null,   text: qsTr("Can…") },
                    ]
                    textRole: "text"
                    valueRole: "preset"

                    // value: root.sharingController.share.permissionPreset
                    onCurrentValueChanged: {
                        if (!currentValue) {
                            return;
                        }

                        if (root.share) {
                            root.sharingController.setPermissionPreset(root.share, currentValue)
                        }
                    }
                }

                Repeater {
                    id: permissionsList
                    Layout.fillWidth: true

                    model: PermissionModel {
                        share: root.share
                    }

                    delegate: ItemDelegate {
                        // Layout.fillWidth: true
                        visible: !permissionPresetSelector.currentValue

                        required property var model
                        RowLayout {
                            Layout.fillWidth: true
                            EnforcedPlainTextLabel {
                                text: model.label
                                Layout.fillWidth: true
                            }
                            Switch {
                                checked: model.enabled
                                onCheckedChanged: {
                                    if (model.enabled === checked) {
                                        return;
                                    }
                                    if (root.share) {
                                        root.sharingController.setPermission(root.share, model.className, checked)
                                    }
                                }
                            }
                        }
                    }
                }

            }

            ListView {
                id: propertyList
                clip: true

                // model: SharingFilterModel {
                //     filterType: SharingFilterModel.General
                //     sourceModel: root.sharingModel
                //     recipientTypes: root.recipientTypes
                // }
                model: PropertyModel {
                    // TODO: only show properties with prio=1
                    share: root.share
                }

                delegate: FieldDelegate {
                    width: propertyList.contentItem.width
                }
            }
        }

        RowLayout {
            Button {
                Layout.fillWidth: true

                text: root.isLinkShare ? qsTr("Copy public link") : qsTr("Copy private link")
            }
            Button {
                Layout.fillWidth: true

                text: qsTr("Send")
                visible: !root.isLinkShare
                enabled: !root.isLinkShare
            }
        }
    }
}
