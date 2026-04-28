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

Loader {
    id: instantiator
    required property var model
    required property var accountState

    sourceComponent: switch (model.type) {
        case SharingModel.Switch:
            return switchComponent;
        case SharingModel.TextField:
            return textFieldComponent;
        case SharingModel.TextArea:
            return textAreaComponent;
        case SharingModel.RecipientsField:
            return recipientsFieldComponent;
        default:
            return unknownItem;
    }

    Component {
        id: switchComponent
        RowLayout {
            Label {
                text: instantiator.model.label
                Layout.fillWidth: true
            }
            Switch {
                Component.onCompleted: checked = instantiator.model.value ?? false
                onCheckedChanged: {
                    if (instantiator.model.value === checked) {
                        return;
                    }
                    instantiator.model.value = checked
                }
            }
        }
    }

    Component {
        id: textFieldComponent
        ColumnLayout {
            Label {
                text: instantiator.model.label
            }
            TextField {
                Layout.fillWidth: true
                placeholderText: instantiator.model.placeholder
                Component.onCompleted: text = instantiator.model.value ?? ""
                onEditingFinished: {
                    if (instantiator.model.value === text) {
                        return;
                    }
                    instantiator.model.value = text
                }
            }
        }
    }

    Component {
        id: textAreaComponent
        ColumnLayout {
            Label {
                text: instantiator.model.label
            }
            TextArea {
                Layout.fillWidth: true
                placeholderText: instantiator.model.placeholder
                Component.onCompleted: text = instantiator.model.value ?? ""
                onEditingFinished: {
                    if (instantiator.model.value === text) {
                        return;
                    }
                    instantiator.model.value = text
                }
            }
        }
    }

    Component {
        id: recipientsFieldComponent
        ColumnLayout {
            Label {
                text: instantiator.model.label
            }
            SearchField {
                id: searchField
                // TODO: only available with Qt 6.10
                Layout.fillWidth: true
                // no placeholderText on SearchField, really?

                suggestionModel: RecipientSearchModel {
                    accountState: instantiator.accountState
                    query: searchField.text
                }
                textRole: "query"
                delegate: ItemDelegate {
                    id: recipientDelegate
                    text: displayName

                    contentItem: Row {
                        Label {
                            // TODO: use plaintext-only fields
                            text: recipientDelegate.value
                        }
                        Label {
                            text: recipientDelegate.text
                            // TODO: use plaintext-only fields
                        }
                    }

                    required property string displayName
                    required property string value
                }

                onActivated: (index) => {
                    console.log(`activated index ${index}`)
                    console.log(`activated index ${index} => ${suggestionModel.get(index).value}`)
                    instantiator.model.value.append(suggestionModel.get(index).value)
                }
            }
            Repeater {
                model: model.value ?? []
                Label {
                    required property list<string> value
                    text: value
                }
            }
        }
    }

    Component {
        id: unknownItem
        RowLayout {
            Label {
                text: "unknown!"
            }
        }
    }
}
