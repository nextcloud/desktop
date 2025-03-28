// SPDX-FileCopyrightText: 2025 Jyrki Gadinger <nilsding@nilsding.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import com.nextcloud.desktopclient
import Style

TextArea {
    id: root

    readonly property color accentColor: palette.highlight
    readonly property color secondaryColor: palette.placeholderText
    readonly property alias submitButton: submitButton

    // no implicitHeight here -- let the textarea take as much as it needs
    // otherwise it will cut off some text vertically on multi-line strings...

    selectByMouse: true
    rightPadding: submitButton.width

    wrapMode: TextEdit.Wrap

    Button {
        id: submitButton

        anchors.bottom: root.bottom
        anchors.right: root.right
        anchors.margins: 1

        width: height
        height: parent.height

        flat: true
        icon.source: "image://svgimage-custom-color/confirm.svg" + "/" + root.secondaryColor
        icon.color: hovered && enabled ? UserModel.currentUser.accentColor : root.secondaryColor

        enabled: root.text !== ""

        onClicked: root.editingFinished()
    }
}
