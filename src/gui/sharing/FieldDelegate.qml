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
import "qrc:/qml/src/gui/wizard/qml"

Loader {
    id: instantiator
    required property var model

    signal valueEdited(string propertyClass, string value)

    function labelText(): string {
        return model.required ? qsTr("%1 (required)").arg(model.label) : model.label
    }

    function submit(value: string): void {
        if (model.value === value) {
            return
        }
        valueEdited(model.property, value)
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
        return stringComponent
    default:
        return unknownComponent
    }

    Component {
        id: booleanComponent

        SwitchDelegate {
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
            EnforcedPlainTextLabel {
                text: instantiator.labelText()
            }
            WizardTextField {
                id: dateField

                Layout.fillWidth: true
                text: instantiator.model.value ?? ""
                placeholderText: instantiator.model.placeholder || qsTr("ISO 8601 date")
                inputMethodHints: Qt.ImhDate

                property bool withinMinimum: !instantiator.model.minimum || !text || Date.parse(text) > Date.parse(instantiator.model.minimum)
                property bool withinMaximum: !instantiator.model.maximum || !text || Date.parse(text) < Date.parse(instantiator.model.maximum)
                property bool valid: (!instantiator.model.required || text.length > 0) && (!text || !isNaN(Date.parse(text))) && withinMinimum && withinMaximum

                onEditingFinished: {
                    if (valid) {
                        instantiator.submit(text)
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
            WizardComboBox {
                id: enumSelector

                Layout.fillWidth: true
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
        }
    }

    Component {
        id: passwordComponent

        ColumnLayout {
            EnforcedPlainTextLabel {
                text: instantiator.labelText()
            }
            WizardTextField {
                Layout.fillWidth: true
                text: instantiator.model.value ?? ""
                placeholderText: instantiator.model.placeholder
                echoMode: TextInput.Password

                onEditingFinished: {
                    if (!instantiator.model.required || text.length > 0) {
                        instantiator.submit(text)
                    }
                }
            }
        }
    }

    Component {
        id: stringComponent

        ColumnLayout {
            EnforcedPlainTextLabel {
                text: instantiator.labelText()
            }
            WizardTextField {
                id: stringField

                Layout.fillWidth: true
                text: instantiator.model.value ?? ""
                placeholderText: instantiator.model.placeholder
                maximumLength: instantiator.model.maximum ?? 32767

                property bool valid: (!instantiator.model.required || text.length > 0) && (!instantiator.model.minimum || text.length >= instantiator.model.minimum)

                onEditingFinished: {
                    if (valid) {
                        instantiator.submit(text)
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
