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

EnforcedPlainTextLabel {
    property bool hovered: false
    property color textColor: palette.windowText
    property color textColorHovered: palette.midlight
    property bool bold: false

    font.underline: true
    font.bold: bold
    color: hovered ? textColorHovered : textColor

    horizontalAlignment: Text.AlignLeft
    verticalAlignment: Text.AlignVCenter

    elide: Text.ElideRight
}

