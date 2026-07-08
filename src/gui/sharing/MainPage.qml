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

Page {
    id: root

    property string localPath: ""
    property string shortLocalPath: ""
    required property SharingController sharingController
    property bool isLinkShare: false

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
                RecipientSearchField {
                    id: searchField
                    Layout.fillWidth: true

                    visible: !root.isLinkShare

                    account: root.sharingController.account

                    onRecipientSelected: (recipientType, recipientValue) => {
                        root.sharingController.addRecipient(recipientType, recipientValue)
                    }
                }

                ComboBox {
                    id: permissionPresetSelector
                    model: [
                        { preset: "view", text: qsTr("Can view") },
                        { preset: "edit", text: qsTr("Can edit") },
                        { preset: null,   text: qsTr("Can…") },
                    ]
                    textRole: "text"
                    valueRole: "preset"

                    // value: root.sharingController.share.permissionPreset
                }

                Repeater {
                    id: permissionsList
                    Layout.fillWidth: true

                    model: PermissionModel {
                        share: root.sharingController.share
                    }

                    delegate: ItemDelegate {
                        // Layout.fillWidth: true
                        visible: !permissionPresetSelector.currentValue

                        required property var model
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: model.label
                                Layout.fillWidth: true
                            }
                            Switch {
                                checked: model.enabled
                                onCheckedChanged: {
                                    if (model.enabled === checked) {
                                        return;
                                    }
                                    root.sharingController.setPermission(model.className, checked)
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

                delegate: FieldDelegate {
                    account: root.account
                    width: propertyList.contentItem.width
                }
            }
        }

        RowLayout {
            Button {
                Layout.fillWidth: true

                text: qsTr("Copy link")
            }
            Button {
                Layout.fillWidth: true

                text: qsTr("Send")
                visible: root.recipientTypes.length > 1
                enabled: root.recipientTypes.length > 1
            }
        }
    }
}
