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
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

Loader {
    id: instantiator
    required property var model

    signal valueEdited(string propertyClass, string value)

    function labelText(): string {
        return model.required ? qsTr("%1 (required)").arg(model.label) : model.label
    }

    readonly property bool optionalField: !!model.advanced && !model.required
    readonly property bool multilineField: typeof model.property === "string"
        && (model.property === "note-property" || model.property.endsWith("\\NoteProperty"))

    function valueIsSet(): bool {
        const value = model.value
        return value !== null && value !== undefined && String(value).length > 0
    }

    function submit(value: string): void {
        if (model.value === value) {
            return
        }
        valueEdited(model.property, value)
    }

    function commit(): void {
        if (item && item.commit) {
            item.commit()
        }
    }

    sourceComponent: switch (model.type) {
    case PropertyModel.Boolean:
        return booleanComponent
    case PropertyModel.Date:
        return dateComponent
    case PropertyModel.Enum:
        return enumComponent
    case PropertyModel.Password:
        return passwordComponent
    case PropertyModel.String:
        return multilineField ? multilineStringComponent : stringComponent
    default:
        return unknownComponent
    }

    Component {
        id: booleanComponent

        SwitchDelegate {
            background: null
            text: instantiator.labelText()
            checked: instantiator.model.value === "true"

            onToggled: {
                instantiator.submit(checked ? "true" : "false")
            }
        }
    }

    Component {
        id: dateComponent

        ColumnLayout {
            id: dateColumn

            function commit(): void {
                if (dateField.valid) {
                    instantiator.submit(dateField.text)
                }
            }

            EnforcedPlainTextLabel {
                text: instantiator.labelText()
            }
            RowLayout {
                property bool fieldEnabled: !instantiator.optionalField || instantiator.valueIsSet()

                WizardTextField {
                    id: dateField

                    Layout.fillWidth: true
                    enabled: parent.fieldEnabled
                    text: instantiator.model.value ?? ""
                    placeholderText: instantiator.model.placeholder || qsTr("YYYY-MM-DD")
                    inputMethodHints: Qt.ImhDate

                    property bool withinMinimum: !instantiator.model.minimum || !text || Date.parse(text) > Date.parse(instantiator.model.minimum)
                    property bool withinMaximum: !instantiator.model.maximum || !text || Date.parse(text) < Date.parse(instantiator.model.maximum)
                    property bool valid: (!instantiator.model.required || text.length > 0) && (!text || !isNaN(Date.parse(text))) && withinMinimum && withinMaximum

                    onEditingFinished: {
                        dateColumn.commit()
                    }
                }

                SwitchDelegate {
                    objectName: "optionalFieldSwitch"
                    background: null
                    visible: instantiator.optionalField
                    checked: parent.fieldEnabled
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    onToggled: {
                        parent.fieldEnabled = checked
                        if (!checked) {
                            instantiator.submit("")
                        }
                    }
                }
            }
            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                visible: dateField.text.length > 0 && !dateField.valid
                text: qsTr("Enter a valid date within the allowed range.")
                color: Style.wizardErrorText
                wrapMode: Text.Wrap
            }
        }
    }

    Component {
        id: enumComponent

        ColumnLayout {
            EnforcedPlainTextLabel {
                text: instantiator.labelText()
            }
            RowLayout {
                property bool fieldEnabled: !instantiator.optionalField || instantiator.valueIsSet()

                WizardComboBox {
                    id: enumSelector

                    Layout.fillWidth: true
                    enabled: parent.fieldEnabled
                    model: instantiator.model.validValues.map(value => ({
                                "name": value,
                                "isSelected": value === instantiator.model.value
                            }))
                    textRole: "name"
                    currentIndex: instantiator.model.validValues.indexOf(instantiator.model.value)

                    onActivated: index => {
                        instantiator.submit(instantiator.model.validValues[index])
                    }
                }

                SwitchDelegate {
                    objectName: "optionalFieldSwitch"
                    background: null
                    visible: instantiator.optionalField
                    checked: parent.fieldEnabled
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    onToggled: {
                        parent.fieldEnabled = checked
                        if (!checked) {
                            instantiator.submit("")
                        }
                    }
                }
            }
        }
    }

    Component {
        id: passwordComponent

        ColumnLayout {
            id: passwordColumn

            function commit(): void {
                if (!instantiator.model.required || passwordField.text.length > 0) {
                    instantiator.submit(passwordField.text)
                }
            }

            EnforcedPlainTextLabel {
                text: instantiator.labelText()
            }
            RowLayout {
                property bool fieldEnabled: !instantiator.optionalField || instantiator.valueIsSet()

                WizardTextField {
                    id: passwordField

                    Layout.fillWidth: true
                    enabled: parent.fieldEnabled
                    text: instantiator.model.value ?? ""
                    placeholderText: instantiator.model.placeholder
                    echoMode: TextInput.Password

                    onEditingFinished: {
                        passwordColumn.commit()
                    }
                }

                SwitchDelegate {
                    objectName: "optionalFieldSwitch"
                    background: null
                    visible: instantiator.optionalField
                    checked: parent.fieldEnabled
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    onToggled: {
                        parent.fieldEnabled = checked
                        if (!checked) {
                            instantiator.submit("")
                        }
                    }
                }
            }
        }
    }

    Component {
        id: stringComponent

        ColumnLayout {
            id: stringColumn

            function commit(): void {
                if (stringField.valid) {
                    instantiator.submit(stringField.text)
                }
            }

            EnforcedPlainTextLabel {
                text: instantiator.labelText()
            }
            RowLayout {
                property bool fieldEnabled: !instantiator.optionalField || instantiator.valueIsSet()

                WizardTextField {
                    id: stringField

                    objectName: "optionalFieldControl"
                    Layout.fillWidth: true
                    enabled: parent.fieldEnabled
                    text: instantiator.model.value ?? ""
                    placeholderText: instantiator.model.placeholder
                    maximumLength: instantiator.model.maximum ?? 32767

                    property bool valid: (!instantiator.model.required || text.length > 0) && (!instantiator.model.minimum || text.length >= instantiator.model.minimum)

                    onEditingFinished: {
                        stringColumn.commit()
                    }
                }

                SwitchDelegate {
                    objectName: "optionalFieldSwitch"
                    background: null
                    visible: instantiator.optionalField
                    checked: parent.fieldEnabled
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    onToggled: {
                        parent.fieldEnabled = checked
                        if (!checked) {
                            instantiator.submit("")
                        }
                    }
                }
            }
            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                visible: stringField.text.length > 0 && !stringField.valid
                text: qsTr("This value is shorter than the minimum length.")
                color: Style.wizardErrorText
                wrapMode: Text.Wrap
            }
        }
    }

    Component {
        id: multilineStringComponent

        ColumnLayout {
            id: multilineStringColumn

            function commit(): void {
                if (multilineField.valid) {
                    instantiator.submit(multilineField.text)
                }
            }

            EnforcedPlainTextLabel {
                text: instantiator.labelText()
            }
            RowLayout {
                property bool fieldEnabled: !instantiator.optionalField || instantiator.valueIsSet()

                WizardTextArea {
                    id: multilineField

                    objectName: "multilineFieldControl"
                    Layout.fillWidth: true
                    enabled: parent.fieldEnabled
                    text: instantiator.model.value ?? ""
                    placeholderText: instantiator.model.placeholder

                    property bool valid: (!instantiator.model.required || text.length > 0) && (!instantiator.model.minimum || text.length >= instantiator.model.minimum)

                    onEditingFinished: {
                        multilineStringColumn.commit()
                    }
                }

                SwitchDelegate {
                    objectName: "optionalFieldSwitch"
                    background: null
                    visible: instantiator.optionalField
                    checked: parent.fieldEnabled
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    onToggled: {
                        parent.fieldEnabled = checked
                        if (!checked) {
                            instantiator.submit("")
                        }
                    }
                }
            }
            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                visible: multilineField.text.length > 0 && !multilineField.valid
                text: qsTr("This value is shorter than the minimum length.")
                color: Style.wizardErrorText
                wrapMode: Text.Wrap
            }
        }
    }

    Component {
        id: unknownComponent

        ColumnLayout {
            EnforcedPlainTextLabel {
                text: instantiator.labelText()
            }
            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: qsTr("This setting is not supported by this version of the desktop client.")
                color: palette.placeholderText
                wrapMode: Text.Wrap
            }
        }
    }
}
