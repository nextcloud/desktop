/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

import Style 1.0
import "./tray"

ComboBox {
    id: clearComboBox

    topPadding: Style.smallSpacing + topInset
    leftPadding: Style.smallSpacing + leftInset
    rightPadding: Style.smallSpacing + rightInset
    bottomPadding: Style.smallSpacing + bottomInset

    background: Rectangle {
        radius: Style.slightlyRoundedButtonRadius
        color: palette.button
        opacity: clearComboBox.hovered ? Style.hoverOpacity : 1.0
    }

    contentItem: EnforcedPlainTextLabel {
        leftPadding: clearComboBox.leftPadding
        rightPadding: clearComboBox.indicator.width + clearComboBox.spacing

        text: clearComboBox.displayText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: ColorOverlay {
        anchors.right: clearComboBox.right
        anchors.rightMargin: clearComboBox.rightPadding
        anchors.verticalCenter: clearComboBox.verticalCenter

        cached: true
        width: source.width
        height: source.height

        source: Image {
            horizontalAlignment: Qt.AlignRight
            verticalAlignment: Qt.AlignVCenter
            source: "image://svgimage-custom-color/caret-down.svg/" + palette.windowText
            sourceSize.width: Style.accountDropDownCaretSize
            sourceSize.height: Style.accountDropDownCaretSize
            Accessible.role: Accessible.PopupMenu
            Accessible.name: qsTr("Clear status message menu")
        }
    }

    popup: Popup {
        y: clearComboBox.height - Style.normalBorderWidth
        width: clearComboBox.width
        implicitHeight: contentItem.implicitHeight
        padding: Style.normalBorderWidth

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: clearComboBox.popup.visible ? clearComboBox.delegateModel : null
            currentIndex: clearComboBox.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: palette.toolTipBase
            border.color: palette.dark
            radius: Style.slightlyRoundedButtonRadius
        }
    }


    delegate: ItemDelegate {
        id: clearStatusDelegate
        width: clearComboBox.width
        contentItem: EnforcedPlainTextLabel {
            text: modelData.display
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: clearComboBox.highlightedIndex === index
    }
}
