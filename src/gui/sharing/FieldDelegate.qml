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
            EnforcedPlainTextLabel {
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
            EnforcedPlainTextLabel {
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
            EnforcedPlainTextLabel {
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
            EnforcedPlainTextLabel {
                text: modelData.label
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
                        EnforcedPlainTextLabel {
                            // TODO: use plaintext-only fields
                            text: recipientDelegate.value
                        }
                        EnforcedPlainTextLabel {
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
            EnforcedPlainTextLabel {
                text: "unknown!"
            }
        }
    }
}
