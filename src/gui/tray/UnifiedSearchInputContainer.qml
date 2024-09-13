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
    id: trayWindowUnifiedSearchTextField

    property bool isSearchInProgress: false

    readonly property color textFieldIconsColor: palette.dark

    readonly property color placeholderColor: palette.dark

    readonly property int textFieldIconsOffset: Style.trayHorizontalMargin

    readonly property double textFieldIconsScaleFactor: 0.6

    readonly property int textFieldHorizontalPaddingOffset: Style.trayHorizontalMargin

    signal clearText()

    leftPadding: trayWindowUnifiedSearchTextFieldSearchIcon.width + trayWindowUnifiedSearchTextFieldSearchIcon.anchors.leftMargin + textFieldHorizontalPaddingOffset - 1
    rightPadding: trayWindowUnifiedSearchTextFieldClearTextButton.width + trayWindowUnifiedSearchTextFieldClearTextButton.anchors.rightMargin + textFieldHorizontalPaddingOffset

    placeholderText: qsTr("Search files, messages, events …")
    verticalAlignment: Qt.AlignVCenter

    selectByMouse: true

    background: Rectangle {
        border.color: palette.dark
        radius: Style.trayWindowRadius
        color: palette.window
    }

    Image {
        id: trayWindowUnifiedSearchTextFieldSearchIcon
        width: Style.trayListItemIconSize - anchors.leftMargin
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignLeft

        anchors {
            left: parent.left
            leftMargin: parent.textFieldIconsOffset
            verticalCenter: parent.verticalCenter
        }

        visible: !trayWindowUnifiedSearchTextField.isSearchInProgress

        smooth: true;
        antialiasing: true
        mipmap: true
        source: "image://svgimage-custom-color/search.svg" + "/" + trayWindowUnifiedSearchTextField.textFieldIconsColor
        sourceSize: Qt.size(parent.height * parent.textFieldIconsScaleFactor, parent.height * parent.textFieldIconsScaleFactor)
    }

    NCBusyIndicator {
        id: busyIndicator

        anchors {
            left: trayWindowUnifiedSearchTextField.left
            bottom: trayWindowUnifiedSearchTextField.bottom
            leftMargin: trayWindowUnifiedSearchTextField.textFieldIconsOffset - 4
            topMargin: Style.smallSpacing
            bottomMargin: Style.smallSpacing
            verticalCenter: trayWindowUnifiedSearchTextField.verticalCenter
        }

        width: height
        color: trayWindowUnifiedSearchTextField.textFieldIconsColor
        visible: trayWindowUnifiedSearchTextField.isSearchInProgress
        running: visible
    }

    Image {
        id: trayWindowUnifiedSearchTextFieldClearTextButton

        anchors {
            right: parent.right
            rightMargin: parent.textFieldIconsOffset
            verticalCenter: parent.verticalCenter
        }

        smooth: true;
        antialiasing: true
        mipmap: true

        visible: parent.text
        source: "image://svgimage-custom-color/clear.svg" + "/" + trayWindowUnifiedSearchTextField.textFieldIconsColor
        sourceSize: Qt.size(parent.height * parent.textFieldIconsScaleFactor, parent.height * parent.textFieldIconsScaleFactor)

        MouseArea {
            id: trayWindowUnifiedSearchTextFieldClearTextButtonMouseArea

            anchors.fill: parent

            onClicked: trayWindowUnifiedSearchTextField.clearText()
        }
    }
}
