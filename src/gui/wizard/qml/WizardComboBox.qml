/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls
import Style

BasicControls.ComboBox {
    id: root

    implicitHeight: Style.standardPrimaryButtonHeight
    leftPadding: 12
    rightPadding: 40
    topPadding: 0
    bottomPadding: 0
    font.pixelSize: Style.pixelSize + 3

    contentItem: Text {
        text: root.displayText
        font: root.font
        color: root.enabled ? Style.wizardPrimaryText : Style.wizardDisabledText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Image {
        x: root.width - width - root.leftPadding
        y: Math.round((root.height - height) / 2)
        width: 16
        height: 16
        source: "image://svgimage-custom-color/caret-down.svg/" + Style.wizardPrimaryText
        rotation: root.popup.visible ? 180 : 0
        opacity: root.enabled ? 1 : 0.45
        fillMode: Image.PreserveAspectFit

        Behavior on rotation {
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }

    background: Rectangle {
        radius: 8
        color: Style.wizardFieldBackground
        border.width: 1
        border.color: root.activeFocus || root.popup.visible
            ? Style.ncBlue
            : Style.wizardFieldBorder
    }

    delegate: BasicControls.CheckDelegate {
        id: delegateItem

        required property int index
        required property var model

        width: ListView.view ? ListView.view.width : root.width - 8
        height: Style.standardPrimaryButtonHeight
        highlighted: root.highlightedIndex === index
        checkState: model.isSelected ? Qt.Checked : Qt.Unchecked

        contentItem: Text {
            text: delegateItem.model.name
            font: root.font
            color: root.currentIndex === delegateItem.index
                ? Style.wizardSelectedText
                : Style.wizardPrimaryText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 6
            color: {
                if (root.currentIndex === delegateItem.index) {
                    return Style.ncBlue
                }
                if (delegateItem.highlighted) {
                    return Style.wizardSecondaryButtonBackground
                }
                return Style.wizardFieldBackground
            }
        }
    }

    popup: BasicControls.Popup {
        y: root.height + 4
        width: root.width
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: Math.min(contentHeight, Style.standardPrimaryButtonHeight * 6)
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
        }

        background: Rectangle {
            radius: 8
            color: Style.wizardFieldBackground
            border.width: 1
            border.color: Style.wizardSecondaryButtonBorder
        }
    }
}
