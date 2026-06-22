/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Style

RowLayout {
    id: unifiedSearchResultItemDetails

    property string title: ""
    property string subline: ""
    property string icons: ""
    property string iconPlaceholder: ""

    property bool iconsIsThumbnail: false
    property bool isRounded: false

    property int iconWidth: iconsIsThumbnail && icons !== "" ? Style.unifiedSearchResultIconWidth : Style.unifiedSearchResultSmallIconWidth
    property int titleFontSize: Style.unifiedSearchResultTitleFontSize
    property int sublineFontSize: Style.unifiedSearchResultSublineFontSize

    property color titleColor: palette.buttonText
    property color sublineColor: palette.dark


    Accessible.role: Accessible.ListItem
    Accessible.name: resultTitle
    Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

    spacing: Style.trayHorizontalMargin

    Item {
        id: unifiedSearchResultImageContainer

        property int whiteSpace: (Style.trayListItemIconSize - unifiedSearchResultItemDetails.iconWidth)

        Layout.preferredWidth: unifiedSearchResultItemDetails.iconWidth
        Layout.preferredHeight: unifiedSearchResultItemDetails.iconWidth
        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
        Layout.leftMargin: Style.trayHorizontalMargin + (whiteSpace * (0.5 - Style.thumbnailImageSizeReduction))
        Layout.rightMargin: whiteSpace * (0.5 + Style.thumbnailImageSizeReduction)

        Image {
            id: unifiedSearchResultThumbnail
            anchors.fill: parent
            visible: false
            asynchronous: true
            source: "image://tray-image-provider/" + unifiedSearchResultItemDetails.icons
            cache: true
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
            sourceSize.width: width
            sourceSize.height: height
        }
        Rectangle {
            id: mask
            anchors.fill: unifiedSearchResultThumbnail
            visible: false
            radius: unifiedSearchResultItemDetails.isRounded ? width / 2 : 3
        }
        OpacityMask {
            id: imageData
            anchors.fill: unifiedSearchResultThumbnail
            visible: unifiedSearchResultItemDetails.icons !== ""
            source: unifiedSearchResultThumbnail
            maskSource: mask
        }
        Image {
            id: unifiedSearchResultThumbnailPlaceholder
            anchors.fill: parent
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
            cache: true
            source: "image://tray-image-provider/" + unifiedSearchResultItemDetails.iconPlaceholder
            visible: unifiedSearchResultItemDetails.iconPlaceholder !== "" && unifiedSearchResultItemDetails.icons === ""
            sourceSize.height: unifiedSearchResultItemDetails.iconWidth
            sourceSize.width: unifiedSearchResultItemDetails.iconWidth
        }
    }

    ListItemLineAndSubline {
        id: unifiedSearchResultTextContainer

        spacing: Style.standardSpacing

        Layout.fillWidth: true
        Layout.rightMargin: Style.trayHorizontalMargin

        lineText: unifiedSearchResultItemDetails.title.replace(/[\r\n]+/g, " ")
        sublineText: unifiedSearchResultItemDetails.subline.replace(/[\r\n]+/g, " ")
    }

}
