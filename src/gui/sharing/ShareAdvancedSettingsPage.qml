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
import "qrc:/qml/src/gui"
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

ColumnLayout {
    id: root

    property var account: null
    property SharingController sharingController: null
    property Share share: null
    property string propertyUpdateError: ""

    signal commitRequested

    spacing: Style.standardSpacing

    function commitPendingChanges(): void {
        commitRequested()
    }

    ListView {
        id: advancedPropertyList
        objectName: "advancedPropertyList"

        Layout.fillWidth: true
        Layout.preferredHeight: contentHeight
        interactive: false
        spacing: Style.standardSpacing

        model: PropertyModel {
            objectName: "advancedPropertyModel"
            share: root.share
            advanced: true
        }

        delegate: FieldDelegate {
            id: advancedPropertyDelegate

            width: advancedPropertyList.width
            height: item ? item.implicitHeight : 0

            onValueEdited: (propertyClass, value) => {
                root.propertyUpdateError = ""
                root.sharingController.setProperty(root.share, propertyClass, value)
            }

            Connections {
                target: root
                function onCommitRequested() {
                    advancedPropertyDelegate.commit()
                }
            }
        }
    }

    EnforcedPlainTextLabel {
        Layout.fillWidth: true
        text: qsTr("No advanced settings are available for this share.")
        color: Style.wizardSecondaryText
        visible: advancedPropertyList.count === 0
    }

    ErrorBox {
        Layout.fillWidth: true
        text: root.propertyUpdateError
        visible: text.length > 0
    }

    Connections {
        target: root.sharingController
        ignoreUnknownSignals: true

        function onPropertyUpdateFailed(share, error) {
            if (share === root.share) {
                root.propertyUpdateError = error
            }
        }
    }
}
