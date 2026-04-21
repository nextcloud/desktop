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

    sourceComponent: switch (modelData.type) {
        case SharingModel.Switch:
            return switchComponent;
        case SharingModel.TextField:
            return textFieldComponent;
        case SharingModel.TextArea:
            return textAreaComponent;
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
        id: unknownItem
        RowLayout {
            Label {
                text: "unknown!"
            }
        }
    }
}
