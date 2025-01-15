/*
 * Copyright (C) 2021 by Oleksandr Zolotov <alex@nextcloud.com>
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

    topPadding: topInset
    bottomPadding: bottomInset
    leftPadding: searchIconImage.width + searchIconImage.x + Style.smallSpacing
    rightPadding: (width - clearTextButton.x) + Style.smallSpacing
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
