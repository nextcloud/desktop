// SPDX-FileCopyrightText: 2022 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import com.nextcloud.desktopclient
import Style

TextField {
    id: root

    readonly property color accentColor: Style.ncBlue
    readonly property color secondaryColor: palette.placeholderText
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

