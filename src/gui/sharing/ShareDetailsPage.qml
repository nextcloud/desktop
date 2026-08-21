/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "qrc:/qml/src/gui"
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

ColumnLayout {
    id: root

    property var account: null
    property SharingController sharingController: null
    property Share share: null
    property string recipientOperationError: ""
    property string permissionUpdateError: ""
    property string propertyUpdateError: ""
    readonly property bool shareIsActive: !!share && share.state === Share.Active

    signal commitRequested

    spacing: Style.standardSpacing

    function copyToClipboard(value: string): void {
        clipboardHelper.text = value
        clipboardHelper.selectAll()
        clipboardHelper.copy()
        clipboardHelper.clear()
    }

    function commitPendingChanges(): void {
        commitRequested()
    }

    TextEdit {
        id: clipboardHelper
        visible: false
    }

    RecipientSearchField {
        id: recipientSearch
        objectName: "recipientSearch"
        Layout.fillWidth: true

        account: root.account
        shareId: root.share ? root.share.id : ""
        visible: !!root.share && !root.share.publicLink

        onRecipientSelected: (recipientType, recipientValue, recipientInstance) => {
            root.recipientOperationError = ""
            root.sharingController.addRecipient(root.share, recipientType, recipientValue, recipientInstance)
        }
    }

    ErrorBox {
        Layout.fillWidth: true

        text: root.recipientOperationError
        visible: text.length > 0
    }

    ListView {
        Layout.fillWidth: true
        Layout.preferredHeight: contentHeight
        interactive: false
        spacing: Style.extraSmallSpacing
        model: RecipientModel {
            objectName: "recipientModel"
            share: root.share
        }

        delegate: WizardItemDelegate {
            id: recipientDelegate

            required property var model

            width: ListView.view.width
            hoverEnabled: true

            contentItem: RowLayout {
                spacing: Style.standardSpacing

                Image {
                    Layout.preferredWidth: Style.activityListButtonIconSize
                    Layout.preferredHeight: Style.activityListButtonIconSize

                    source: RecipientIcon.source(recipientDelegate.model.iconSvgUrl, recipientDelegate.model.iconLight, recipientDelegate.model.iconDark)
                    sourceSize.width: Style.activityListButtonIconSize
                    sourceSize.height: Style.activityListButtonIconSize
                    fillMode: Image.PreserveAspectFit
                    visible: source.toString().length > 0
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    EnforcedPlainTextLabel {
                        Layout.fillWidth: true
                        text: recipientDelegate.model.label
                        color: Style.wizardPrimaryText
                        elide: Text.ElideRight
                    }

                    EnforcedPlainTextLabel {
                        Layout.fillWidth: true
                        text: {
                            const details = []
                            if (recipientDelegate.model.instance) {
                                details.push(recipientDelegate.model.instance)
                            }
                            if (recipientDelegate.model.initiatorDisplayName) {
                                details.push(qsTr("Added by %1").arg(recipientDelegate.model.initiatorDisplayName))
                            }
                            return details.join(" · ")
                        }
                        color: Style.wizardSecondaryText
                        elide: Text.ElideRight
                        visible: text.length > 0
                    }
                }

                WizardButton {
                    objectName: "removeRecipientButton"
                    Layout.preferredWidth: implicitHeight
                    leftPadding: 0
                    rightPadding: 0
                    text: ""
                    iconSource: "image://svgimage-custom-color/copy.svg/" + palette.buttonText
                    visible: root.shareIsActive && recipientDelegate.model.secretUrl !== ""
                    enabled: visible

                    Accessible.name: qsTr("Copy recipient link")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name

                    onClicked: root.copyToClipboard(recipientDelegate.model.secretUrl)
                }

                WizardButton {
                    Layout.preferredWidth: implicitHeight
                    leftPadding: 0
                    rightPadding: 0
                    text: ""
                    iconSource: "image://svgimage-custom-color/change.svg/" + palette.buttonText
                    visible: root.shareIsActive && recipientDelegate.model.secretUpdatable
                    enabled: visible

                    Accessible.name: recipientDelegate.model.secretUrl !== "" ? qsTr("Regenerate recipient link") : qsTr("Generate recipient link")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name

                    onClicked: {
                        root.recipientOperationError = ""
                        root.sharingController.updateRecipientSecret(root.share, recipientDelegate.model.className, recipientDelegate.model.value, recipientDelegate.model.instance || "")
                    }
                }

                WizardButton {
                    Layout.preferredWidth: implicitHeight
                    leftPadding: 0
                    rightPadding: 0
                    text: ""
                    iconSource: "image://svgimage-custom-color/delete.svg/" + palette.buttonText
                    visible: !!root.share && !root.share.publicLink

                    Accessible.name: qsTr("Remove recipient")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name

                    onClicked: {
                        root.recipientOperationError = ""
                        root.sharingController.removeRecipient(root.share, recipientDelegate.model.className, recipientDelegate.model.value, recipientDelegate.model.instance || "")
                    }
                }
            }
        }
    }

    WizardComboBox {
        id: permissionPresetSelector
        Layout.fillWidth: true

        readonly property var presetValues: ["OC\\Core\\Sharing\\Permission\\ViewSharePermissionPreset", "OC\\Core\\Sharing\\Permission\\EditSharePermissionPreset", ""]
        readonly property int selectedPresetIndex: {
            const preset = root.share && root.share.permissionPreset ? root.share.permissionPreset : ""
            if (preset.endsWith("\\ViewSharePermissionPreset")) {
                return 0
            }
            if (preset.endsWith("\\EditSharePermissionPreset")) {
                return 1
            }
            return 2
        }
        model: [
            {
                "name": qsTr("Can view"),
                "isSelected": selectedPresetIndex === 0
            },
            {
                "name": qsTr("Can edit"),
                "isSelected": selectedPresetIndex === 1
            },
            {
                "name": qsTr("Custom permissions"),
                "isSelected": selectedPresetIndex === 2
            }
        ]
        textRole: "name"
        currentIndex: selectedPresetIndex

        onActivated: function (index) {
            const preset = presetValues[index]
            if (preset) {
                root.permissionUpdateError = ""
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
                root.permissionUpdateError = ""
                root.sharingController.setPermission(root.share, model.className, checked)
            }
        }
    }

    ErrorBox {
        Layout.fillWidth: true

        text: root.permissionUpdateError
        visible: text.length > 0
    }

    EnforcedPlainTextLabel {
        Layout.fillWidth: true
        Layout.topMargin: Style.standardSpacing

        text: qsTr("Sharing settings")
        font.weight: Font.DemiBold
        visible: propertyList.count > 0
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
            id: propertyDelegate

            width: propertyList.width
            height: item ? item.implicitHeight : 0

            onValueEdited: (propertyClass, value) => {
                root.propertyUpdateError = ""
                root.sharingController.setProperty(root.share, propertyClass, value)
            }

            Connections {
                target: root
                function onCommitRequested() {
                    propertyDelegate.commit()
                }
            }
        }
    }

    ErrorBox {
        Layout.fillWidth: true

        text: root.propertyUpdateError
        visible: text.length > 0
    }

    Connections {
        target: root.sharingController
        ignoreUnknownSignals: true

        function onRecipientAdded(share) {
            if (share === root.share) {
                recipientSearch.clear()
                root.recipientOperationError = ""
            }
        }

        function onRecipientAdditionFailed(share, error) {
            if (share === root.share) {
                root.recipientOperationError = error
            }
        }

        function onRecipientRemoved(share) {
            if (share === root.share) {
                root.recipientOperationError = ""
            }
        }

        function onRecipientRemovalFailed(share, error) {
            if (share === root.share) {
                root.recipientOperationError = error
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

        function onPermissionUpdateFailed(share, error) {
            if (share === root.share) {
                root.permissionUpdateError = error
            }
        }
    }
}
