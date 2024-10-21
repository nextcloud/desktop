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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import com.nextcloud.desktopclient
import Style

TextField {
    id: root

    readonly property color accentColor: Style.ncBlue
    readonly property color secondaryColor: palette.dark
    readonly property alias submitButton: submitButton
    property bool validInput: true

    implicitHeight: Math.max(Style.talkReplyTextFieldPreferredHeight, contentHeight)

    rightPadding: submitButton.width

    selectByMouse: true

    Button {
        id: submitButton

        anchors.top: root.top
        anchors.right: root.right
        anchors.margins: 1

        width: height
        height: parent.height

        background: null
        flat: true
        icon.source: "image://svgimage-custom-color/confirm.svg" + "/" + root.secondaryColor
        icon.color: hovered && enabled ? UserModel.currentUser.accentColor : root.secondaryColor

        enabled: root.text !== "" && root.validInput

        onClicked: root.accepted()
    }

    verticalAlignment: Qt.AlignVCenter
}

