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
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Style 1.0

RowLayout {
    id: root

    property bool hovered: false
    property string imageSourceHover: ""
    property string imageSource: ""
    property int imageSourceWidth: undefined
    property int imageSourceHeight: undefined
    property string text: ""
    property var display

    property color textColor: palette.buttonText
    property color textColorHovered: textColor
    property alias font: buttonLabel.font

    Image {
        id: icon

        Layout.maximumWidth: root.height
        Layout.maximumHeight: root.height
        Layout.alignment: Qt.AlignCenter

        source: root.hovered ? root.imageSourceHover : root.imageSource

        sourceSize {
            width: root.imageSourceWidth
            height: root.imageSourceHeight
        }

        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        visible: root.display === Button.TextOnly ? false : root.hovered ? root.imageSourceHover !== "" : root.imageSource !== ""
    }

    EnforcedPlainTextLabel {
        id: buttonLabel

        Layout.fillWidth: true

        text: root.text

        visible: root.text !== ""

        color: root.hovered ? root.textColorHovered : root.textColor

        horizontalAlignment: icon.visible ? Text.AlignLeft : Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        elide: Text.ElideRight
    }
}
