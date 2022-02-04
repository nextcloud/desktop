import QtQml 2.15
import QtQuick 2.15
import QtQuick.Layouts 1.2
import Style 1.0

RowLayout {
    id: unifiedSearchResultSkeletonItemDetails

    property int textLeftMargin: 18
    property int textRightMargin: 16
    property int iconWidth: 24
    property int iconLeftMargin: 12

    property int titleFontSize: Style.topLinePixelSize
    property int sublineFontSize: Style.subLinePixelSize

    property color titleColor: Style.ncTextColor
    property color sublineColor: Style.ncSecondaryTextColor

    property string iconColor: "#afafaf"

    property int index: 0

    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("Search result skeleton.").arg(index)

    Rectangle {
        id: unifiedSearchResultSkeletonThumbnail
        color: unifiedSearchResultSkeletonItemDetails.iconColor
        Layout.preferredWidth: unifiedSearchResultSkeletonItemDetails.iconWidth
        Layout.preferredHeight: unifiedSearchResultSkeletonItemDetails.iconWidth
        Layout.leftMargin: unifiedSearchResultSkeletonItemDetails.iconLeftMargin
        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
    }

    ColumnLayout {
        id: unifiedSearchResultSkeletonTextContainer
        Layout.fillWidth: true

        Rectangle {
            id: unifiedSearchResultSkeletonTitleText
            color: unifiedSearchResultSkeletonItemDetails.titleColor
            Layout.preferredHeight: unifiedSearchResultSkeletonItemDetails.titleFontSize
            Layout.leftMargin: unifiedSearchResultSkeletonItemDetails.textLeftMargin
            Layout.rightMargin: unifiedSearchResultSkeletonItemDetails.textRightMargin
            Layout.fillWidth: true
        }

        Rectangle {
            id: unifiedSearchResultSkeletonTextSubline
            color: unifiedSearchResultSkeletonItemDetails.sublineColor
            Layout.preferredHeight: unifiedSearchResultSkeletonItemDetails.sublineFontSize
            Layout.leftMargin: unifiedSearchResultSkeletonItemDetails.textLeftMargin
            Layout.rightMargin: unifiedSearchResultSkeletonItemDetails.textRightMargin
            Layout.fillWidth: true
        }
    }
}
