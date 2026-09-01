/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import Style
import "qrc:/qml/src/gui/wizard/qml"

Popup {
    id: root

    required property var searchModel
    required property var peopleModel
    required property real windowWidth

    signal personSelected()

    parent: Overlay.overlay
    width: Math.min(windowWidth - Style.unifiedSearchPeoplePopupHorizontalMargin,
                    Style.wizardDialogMaximumWidth)
    height: Style.unifiedSearchPeoplePopupHeight
    x: (windowWidth - width) / 2
    y: Style.unifiedSearchPeoplePopupTopMargin
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: peopleSearch.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Style.smallSpacing

        TextField {
            id: peopleSearch

            Layout.fillWidth: true
            placeholderText: qsTr("Search people")
            onTextEdited: root.peopleModel.searchTerm = text
        }

        EnforcedPlainTextLabel {
            visible: root.peopleModel.errorString.length > 0
            text: root.peopleModel.errorString
            wrapMode: Text.Wrap
        }

        WizardButton {
            visible: root.peopleModel.errorString.length > 0
            text: qsTr("Retry")
            onClicked: root.peopleModel.retry()
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.peopleModel

            delegate: UnifiedSearchPeopleDelegate {
                width: ListView.view.width

                onPersonChosen: (userId, displayName, avatarUrl) => {
                    root.searchModel.setPersonFilter(userId, displayName, avatarUrl)
                    root.close()
                    root.personSelected()
                }
            }
        }
    }
}
