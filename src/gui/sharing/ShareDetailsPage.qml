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
    property string recipientAdditionError: ""

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
                    id: recipientSearch
                    Layout.fillWidth: true

                    account: root.sharingController.account

                    onRecipientSelected: (recipientType, recipientValue) => {
                        root.recipientAdditionError = ""
                        root.sharingController.addRecipient(root.share, recipientType, recipientValue)
                    }
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true

                    text: root.recipientAdditionError
                    color: Style.wizardErrorText
                    wrapMode: Text.Wrap
                    visible: text.length > 0
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: contentHeight
                    interactive: false
                    spacing: Style.extraSmallSpacing

                    model: RecipientModel {
                        share: root.share
                    }

                    delegate: ItemDelegate {
                        required property var model

                        width: ListView.view.width
                        text: model.label
                    }
                }

                ComboBox {
                    id: permissionPresetSelector
                    Layout.fillWidth: true

                    readonly property var presetValues: [
                        "OC\\Core\\Sharing\\Permission\\ViewSharePermissionPreset",
                        "OC\\Core\\Sharing\\Permission\\EditSharePermissionPreset",
                        ""
                    ]
                    model: [qsTr("Can view"), qsTr("Can edit"), qsTr("Custom permissions")]
                    currentIndex: {
                        const preset = root.share.permissionPreset
                        if (preset.endsWith("\\ViewSharePermissionPreset")) {
                            return 0
                        }
                        if (preset.endsWith("\\EditSharePermissionPreset")) {
                            return 1
                        }
                        return 2
                    }

                    onActivated: function(index) {
                        const preset = presetValues[index]
                        if (preset) {
                            root.sharingController.setPermissionPreset(root.share, preset)
                        }
                    }
                }

                Repeater {
                    Layout.fillWidth: true

                    model: PermissionModel {
                        share: root.share
                    }

                    delegate: RowLayout {
                        visible: permissionPresetSelector.currentIndex === 2
                        required property var model

                            Layout.fillWidth: true

                            EnforcedPlainTextLabel {
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

            ListView {
                id: propertyList
                clip: true

                model: PropertyModel {
                    // TODO: only show properties with prio=1
                    share: root.share
                }

                delegate: FieldDelegate {
                    width: propertyList.contentItem.width
                    height: item ? item.implicitHeight : 0
                }
            }
        }

            }

    Connections {
        target: root.sharingController

        function onRecipientAdded(share) {
            if (share === root.share) {
                recipientSearch.clear()
            }
        }

        function onRecipientAdditionFailed(share, error) {
            if (share === root.share) {
                root.recipientAdditionError = error
            }
        }
    }
}
