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
    property string propertyUpdateError: ""
    property string shareActivationError: ""
    property bool activatingShare: false

    title: qsTr("Share details")

    ColumnLayout {
        anchors.fill: parent

        ScrollView {
            Layout.fillHeight: true
            Layout.fillWidth: true

            contentWidth: availableWidth
            rightPadding: ScrollBar.vertical.visible ? ScrollBar.vertical.width + Style.standardSpacing : 0

            ColumnLayout {
                width: parent.width
                spacing: Style.standardSpacing

                RecipientSearchField {
                    id: recipientSearch
                    Layout.fillWidth: true

                    account: root.sharingController.account
                    shareId: root.share.id

                    onRecipientSelected: (recipientType, recipientValue, recipientInstance) => {
                        root.recipientOperationError = ""
                        root.sharingController.addRecipient(root.share,
                                                            recipientType,
                                                            recipientValue,
                                                            recipientInstance)
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

                    delegate: EnforcedPlainTextLabel {
                        required property var model

                        width: ListView.view.width
                        text: model.label
                        elide: Text.ElideRight
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
                    delegate: ItemDelegate {
                        id: permissionPresetDelegate

                        required property int index
                        required property string modelData

                        width: permissionPresetSelector.width
                        text: modelData
                        highlighted: permissionPresetSelector.highlightedIndex === index

                        contentItem: EnforcedPlainTextLabel {
                            text: permissionPresetDelegate.text
                            color: permissionPresetDelegate.highlighted ? palette.highlightedText : palette.text
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
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

                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? contentHeight : 0

                    interactive: false
                    visible: permissionPresetSelector.currentIndex === 2
                    model: PermissionModel {
                        share: root.share
                    }

                    delegate: SwitchDelegate {
                        required property var model

                        width: ListView.view.width
                        text: model.label
                        checked: model.enabled

                        onToggled: {
                            root.sharingController.setPermission(root.share, model.className, checked)
                        }
                    }
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true
                    Layout.topMargin: Style.standardSpacing

                    text: qsTr("Sharing settings")
                    font.weight: Font.DemiBold
                    visible: propertyList.count > 0 || advancedPropertyList.count > 0
                }

                ListView {
                    id: propertyList

                    Layout.fillWidth: true
                    Layout.preferredHeight: contentHeight
                    interactive: false
                    spacing: Style.standardSpacing

                    model: PropertyModel {
                        share: root.share
                    }

                    delegate: FieldDelegate {
                        width: propertyList.width
                        height: item ? item.implicitHeight : 0

                        onValueEdited: (propertyClass, value) => {
                            root.propertyUpdateError = ""
                            root.sharingController.setProperty(root.share, propertyClass, value)
                        }
                    }
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true
                    Layout.topMargin: Style.standardSpacing

                    text: qsTr("Advanced settings")
                    font.weight: Font.DemiBold
                    visible: advancedPropertyList.count > 0
                }

                ListView {
                    id: advancedPropertyList

                    Layout.fillWidth: true
                    Layout.preferredHeight: contentHeight
                    interactive: false
                    spacing: Style.standardSpacing

                    model: PropertyModel {
                        share: root.share
                        advanced: true
                    }

                    delegate: FieldDelegate {
                        width: advancedPropertyList.width
                        height: item ? item.implicitHeight : 0

                        onValueEdited: (propertyClass, value) => {
                            root.propertyUpdateError = ""
                            root.sharingController.setProperty(root.share, propertyClass, value)
                        }
                    }
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true

                    text: root.propertyUpdateError
                    color: Style.wizardErrorText
                    wrapMode: Text.Wrap
                    visible: text.length > 0
                }
            }
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            text: root.shareActivationError
            color: Style.wizardErrorText
            wrapMode: Text.Wrap
            visible: text.length > 0
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            text: qsTr("Changes to this share are applied immediately.")
            color: palette.placeholderText
            wrapMode: Text.Wrap
            visible: root.share.state === Share.Active
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.share.state === Share.Draft

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                primary: true
                text: root.activatingShare ? qsTr("Sending…") : qsTr("Send share")
                enabled: !root.activatingShare

                onClicked: {
                    root.shareActivationError = ""
                    root.activatingShare = true
                    root.sharingController.activateShare(root.share)
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

        function onRecipientSecretUpdated(share) {
            if (share === root.share) {
                root.recipientOperationError = ""
            }
        }

        function onRecipientSecretUpdateFailed(share, error) {
            if (share === root.share) {
                root.recipientOperationError = error
            }
        }

        function onPropertyUpdateFailed(share, error) {
            if (share === root.share) {
                root.propertyUpdateError = error
            }
        }

        function onShareActivated(share) {
            if (share === root.share) {
                root.activatingShare = false
            }
        }

        function onShareActivationFailed(share, error) {
            if (share === root.share) {
                root.activatingShare = false
                root.shareActivationError = error
            }
        }
    }
}
