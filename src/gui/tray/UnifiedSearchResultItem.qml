import QtQml 2.15
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.0

import Style 1.0

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

    property color titleColor: Style.ncTextColor
    property color sublineColor: Style.ncSecondaryTextColor

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

    ColumnLayout {
        id: unifiedSearchResultTextContainer

        Layout.fillWidth: true
        Layout.rightMargin: Style.trayHorizontalMargin
        spacing: Style.standardSpacing

        Label {
            id: unifiedSearchResultTitleText
            Layout.fillWidth: true
            text: unifiedSearchResultItemDetails.title.replace(/[\r\n]+/g, " ")
            elide: Text.ElideRight
            font.pixelSize: unifiedSearchResultItemDetails.titleFontSize
            color: unifiedSearchResultItemDetails.titleColor
        }
        Label {
            id: unifiedSearchResultTextSubline
            Layout.fillWidth: true
            text: unifiedSearchResultItemDetails.subline.replace(/[\r\n]+/g, " ")
            elide: Text.ElideRight
            font.pixelSize: unifiedSearchResultItemDetails.sublineFontSize
            color: unifiedSearchResultItemDetails.sublineColor
        }
    }

}
