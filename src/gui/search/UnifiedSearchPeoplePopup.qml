/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import Style
import "qrc:/qml/src/gui/tray"
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

            delegate: ItemDelegate {
                id: personDelegate

                required property string userId
                required property string displayName
                required property string avatarUrl

                width: ListView.view.width
                height: Style.unifiedSearchProviderHeaderHeight
                text: displayName
                hoverEnabled: true
                Accessible.description: userId

                background: Rectangle {
                    color: personDelegate.hovered || personDelegate.down
                        ? Style.listItemHoverBackground
                        : "transparent"
                    radius: Style.mediumRoundedButtonRadius
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                contentItem: RowLayout {
                    Image {
                        Layout.preferredWidth: Style.accountAvatarSize
                        Layout.preferredHeight: Style.accountAvatarSize
                        sourceSize.width: Style.accountAvatarSize
                        sourceSize.height: Style.accountAvatarSize
                        asynchronous: true
                        source: personDelegate.avatarUrl.length > 0
                            ? "image://tray-image-provider/" + personDelegate.avatarUrl
                            : ""
                        Accessible.ignored: true
                    }

                    EnforcedPlainTextLabel {
                        Layout.fillWidth: true
                        text: personDelegate.displayName
                        elide: Text.ElideRight
                    }
                }

                onClicked: {
                    root.searchModel.setPersonFilter(personDelegate.userId,
                                                     personDelegate.displayName,
                                                     personDelegate.avatarUrl)
                    root.close()
                    root.personSelected()
                }
            }
        }
    }
}
