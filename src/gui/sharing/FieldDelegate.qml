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

    sourceComponent: switch (model.type) {
        case PropertyModel.Switch:
            return switchComponent;
        case PropertyModel.TextField:
            return textFieldComponent;
        case PropertyModel.TextArea:
            return textAreaComponent;
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
        id: unknownItem
        RowLayout {
            Label {
                text: "unknown!"
            }
        }
    }
}
