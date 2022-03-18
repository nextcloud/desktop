import QtQml 2.15
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.0

RowLayout {
    id: unifiedSearchResultItemDetails

    property string title: ""
    property string subline: ""
    property string icons: ""
    property string iconPlaceholder: ""
    property bool isRounded: false


    property int textLeftMargin: 18
    property int textRightMargin: 16
    property int iconWidth: 24
    property int iconLeftMargin: 12

    property int titleFontSize: Style.topLinePixelSize
    property int sublineFontSize: Style.subLinePixelSize

    property color titleColor: Style.ncTextColor
    property color sublineColor: Style.ncSecondaryTextColor

    Accessible.role: Accessible.ListItem
    Accessible.name: resultTitle
    Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

    ColumnLayout {
        id: unifiedSearchResultImageContainer
        visible: true
        Layout.preferredWidth: unifiedSearchResultItemDetails.iconWidth + 10
        Layout.preferredHeight: unifiedSearchResultItemDetails.height
        Image {
            id: unifiedSearchResultThumbnail
            visible: false
            asynchronous: true
            source: "image://tray-image-provider/" + icons
            cache: true
            sourceSize.width: imageData.width
            sourceSize.height: imageData.height
            width: imageData.width
            height: imageData.height
        }
        Rectangle {
            id: mask
            visible: false
            radius: isRounded ? width / 2 : 0
            width: imageData.width
            height: imageData.height
        }
        OpacityMask {
            id: imageData
            visible: !unifiedSearchResultThumbnailPlaceholder.visible && icons
            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            Layout.leftMargin: iconLeftMargin
            Layout.preferredWidth: unifiedSearchResultItemDetails.iconWidth
            Layout.preferredHeight: unifiedSearchResultItemDetails.iconWidth
            source: unifiedSearchResultThumbnail
            maskSource: mask
        }
        Image {
            id: unifiedSearchResultThumbnailPlaceholder
            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            Layout.leftMargin: iconLeftMargin
            verticalAlignment: Qt.AlignCenter
            cache: true
            source: iconPlaceholder
            visible: false
            sourceSize.height: unifiedSearchResultItemDetails.iconWidth
            sourceSize.width: unifiedSearchResultItemDetails.iconWidth
            Layout.preferredWidth: unifiedSearchResultItemDetails.iconWidth
            Layout.preferredHeight: unifiedSearchResultItemDetails.iconWidth
        }
    }

    ColumnLayout {
        id: unifiedSearchResultTextContainer
        Layout.fillWidth: true

        Label {
            id: unifiedSearchResultTitleText
            text: title.replace(/[\r\n]+/g, " ")
            Layout.leftMargin: textLeftMargin
            Layout.rightMargin: textRightMargin
            Layout.fillWidth: true
            elide: Text.ElideRight
            font.pixelSize: unifiedSearchResultItemDetails.titleFontSize
            color: unifiedSearchResultItemDetails.titleColor
        }
        Label {
            id: unifiedSearchResultTextSubline
            text: subline.replace(/[\r\n]+/g, " ")
            elide: Text.ElideRight
            font.pixelSize: unifiedSearchResultItemDetails.sublineFontSize
            Layout.leftMargin: textLeftMargin
            Layout.rightMargin: textRightMargin
            Layout.fillWidth: true
            color: unifiedSearchResultItemDetails.sublineColor
        }
    }

}
