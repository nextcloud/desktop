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

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Style
import "./tray"

AbstractButton {
    id: root

    hoverEnabled: true
    topPadding: Style.standardSpacing
    bottomPadding: Style.standardSpacing
    leftPadding: Style.standardSpacing / 2
    rightPadding: Style.standardSpacing / 2

    property int emojiWidth: -1
    property int internalSpacing: Style.standardSpacing
    property string emoji: ""
    property string statusText: ""
    property string clearAtText: ""

    background: Rectangle {
        color: root.hovered || root.checked ? palette.highlight : palette.base
        // TODO: fix radius borders - they were showing for each item
        // radius: Style.slightlyRoundedButtonRadius
    }

    contentItem: Row {
        spacing: root.internalSpacing

        EnforcedPlainTextLabel {
            width: root.emojiWidth > 0 ? root.emojiWidth : implicitWidth
            text: emoji
            horizontalAlignment: Image.AlignHCenter
            verticalAlignment: Image.AlignVCenter
        }

        Row {
            spacing: Style.smallSpacing
            EnforcedPlainTextLabel {
                text: root.statusText
                verticalAlignment: Text.AlignVCenter
                font.bold: true
            }

            EnforcedPlainTextLabel {
                text: "-"
                verticalAlignment: Text.AlignVCenter
            }

            EnforcedPlainTextLabel {
                text: root.clearAtText
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
