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
    required property var modelData
    required property var accountState

    sourceComponent: switch (modelData.type) {
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
                text: modelData.label
                Layout.fillWidth: true
            }
            Switch {
            }
        }
    }

    Component {
        id: textFieldComponent
        ColumnLayout {
            Label {
                text: modelData.label
            }
            TextField {
                Layout.fillWidth: true
                placeholderText: modelData.placeholder
            }
        }
    }

    Component {
        id: textAreaComponent
        ColumnLayout {
            Label {
                text: modelData.label
            }
            TextArea {
                Layout.fillWidth: true
                placeholderText: modelData.placeholder
            }
        }
    }

    Component {
        id: recipientsFieldComponent
        ColumnLayout {
            Label {
                text: modelData.label
            }
            SearchField {
                id: searchField
                // TODO: only available with Qt 6.10
                Layout.fillWidth: true
                // no placeholderText on SearchField, really?

                suggestionModel: RecipientSearchModel {
                    accountState: root.accountState
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
