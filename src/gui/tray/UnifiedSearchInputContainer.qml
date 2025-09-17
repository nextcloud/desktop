/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import Style

import com.nextcloud.desktopclient

TextField {
    id: root

    signal clearText()

    property bool isSearchInProgress: false

    readonly property color textFieldIconsColor: palette.placeholderText

    readonly property int iconInset: Style.smallSpacing

    readonly property real leadingControlWidth: root.isSearchInProgress ? busyIndicator.width : searchIconImage.width
    readonly property real trailingControlWidth: clearTextButton.visible ? clearTextButton.width : 0

    topPadding: topInset
    bottomPadding: bottomInset
    leftPadding: iconInset + leadingControlWidth + Style.smallSpacing
    rightPadding: iconInset + trailingControlWidth + Style.smallSpacing
    verticalAlignment: Qt.AlignVCenter

    placeholderText: qsTr("Search files, messages, events …")

    selectByMouse: true

    Image {
        id: searchIconImage

        anchors {
            left: root.left
            leftMargin: iconInset
            top: root.top
            topMargin: Style.extraSmallSpacing
            bottom: root.bottom
            bottomMargin: Style.extraSmallSpacing 
        }

        fillMode: Image.PreserveAspectFit
        smooth: true
        antialiasing: true
        mipmap: true
        source: "image://svgimage-custom-color/search.svg" + "/" + root.textFieldIconsColor
        visible: !root.isSearchInProgress
    }

    NCBusyIndicator {
        id: busyIndicator

        anchors {
            top: root.top
            topMargin: Style.extraSmallSpacing
            bottom: root.bottom
            bottomMargin: Style.extraSmallSpacing
            left: root.left
            leftMargin: iconInset
        }

        width: height
        color: root.textFieldIconsColor
        visible: root.isSearchInProgress
        running: visible
    }

    Image {
        id: clearTextButton

        anchors {
            top: root.top
            topMargin: Style.extraSmallSpacing
            bottom: root.bottom
            bottomMargin: Style.extraSmallSpacing
            right: root.right
            rightMargin: iconInset
        }

        fillMode: Image.PreserveAspectFit
        visible: root.text
        source: "image://svgimage-custom-color/clear.svg" + "/" + root.textFieldIconsColor

        MouseArea {
            id: clearTextButtonMouseArea
            anchors.fill: parent
            onClicked: root.clearText()
        }
    }
}
