/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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
import QtQuick.Dialogs 1.3
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.0

import Style 1.0

ComboBox {
    id: clearComboBox

    padding: Style.standardSpacing

    background: Rectangle {
        radius: Style.slightlyRoundedButtonRadius
        color: Style.buttonBackgroundColor
        opacity: clearComboBox.hovered ? Style.hoverOpacity : 1.0
    }

    contentItem: Label {
        leftPadding: 0
        rightPadding: clearComboBox.indicator.width + clearComboBox.spacing

        text: clearComboBox.displayText
        color: Style.ncTextColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: ColorOverlay {
        x: clearComboBox.width - clearComboBox.rightPadding
        y: clearComboBox.topPadding + (clearComboBox.availableHeight - height) / 2
        cached: true
        color: Style.ncTextColor
        width: source.width
        height: source.height
        source: Image {
            horizontalAlignment: Qt.AlignRight
            verticalAlignment: Qt.AlignVCenter
            source: "qrc:///client/theme/white/caret-down.svg"
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
            color: Style.backgroundColor
            border.color: Style.menuBorder
            radius: Style.slightlyRoundedButtonRadius
        }
    }


    delegate: ItemDelegate {
        id: clearStatusDelegate
        width: clearComboBox.width
        contentItem: Label {
            text: modelData
            color: Style.ncTextColor
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: clearComboBox.highlightedIndex === index
        background: Rectangle {
           color: clearStatusDelegate.highlighted || clearStatusDelegate.hovered ? Style.lightHover : Style.backgroundColor
       }
    }
}
