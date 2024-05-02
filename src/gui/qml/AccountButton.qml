/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
import QtQuick.Controls
import QtQuick.Layouts

ToolButton {
    id: control

    readonly property real goldenRatio: 1.618
    readonly property real widthHint: height * goldenRatio

    clip: true
    icon.height: 32
    icon.width: 32
    implicitWidth: Math.min(implicitContentWidth + leftPadding + rightPadding, widthHint)

    // make the current button pop
    palette.button: palette.highlight

    contentItem: ColumnLayout {
        spacing: control.spacing

        Image {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            Layout.preferredHeight: control.icon.height
            Layout.preferredWidth: control.icon.width
            fillMode: Image.PreserveAspectFit
            source: control.icon.source
        }
        Label {
            Layout.fillHeight: true
            Layout.fillWidth: true
            color: control.visualFocus ? control.palette.highlight : control.palette.buttonText
            // elide middle would look better but doesn't work with wrapping
            elide: Text.ElideRight
            font: control.font
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 2
            opacity: enabled ? 1.0 : 0.3
            text: control.text
            verticalAlignment: Text.AlignTop
            wrapMode: Text.WordWrap
        }
    }
}
