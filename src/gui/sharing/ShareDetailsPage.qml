/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "qrc:/qml/src/gui/wizard/qml"

Page {
    id: root

    required property SharingController sharingController
    required property Share share

    title: qsTr("Share details")

    ColumnLayout {
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
                    Layout.fillWidth: true

                    account: root.sharingController.account

                    onRecipientSelected: (recipientType, recipientValue) => {
                        root.sharingController.addRecipient(root.share, recipientType, recipientValue)
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

                    // value: root.share.permissionPreset
                    onCurrentValueChanged: {
                        if (currentValue) {
                            root.sharingController.setPermissionPreset(root.share, currentValue)
                        }
                    }
                }

                Repeater {
                    Layout.fillWidth: true

                    model: PermissionModel {
                        share: root.share
                    }

                    delegate: ItemDelegate {
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
                                    if (model.enabled !== checked) {
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
                text: qsTr("Copy link")
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Send")
            }
        }
    }
}
