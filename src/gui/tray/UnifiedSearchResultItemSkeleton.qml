import QtQml 2.12
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.0

RowLayout {
    id: unifiedSearchResultSkeletonItemDetails

    property int textLeftMargin: 18
    property int textRightMargin: 16
    property int iconWidth: 24
    property int iconLeftMargin: 12

    property int index: 0

    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("Search result skeleton.").arg(index)

    ColumnLayout {
        id: unifiedSearchResultSkeletonImageContainer
        visible: true
        Layout.preferredWidth: unifiedSearchResultSkeletonItemDetails.iconWidth + 10
        Layout.preferredHeight: unifiedSearchResultSkeletonItemDetails.height
        Rectangle {
            id: unifiedSearchResultSkeletonThumbnail
            color: "grey"
            width:  unifiedSearchResultSkeletonItemDetails.iconWidth
            height:  unifiedSearchResultSkeletonItemDetails.iconWidth
            Layout.leftMargin: unifiedSearchResultSkeletonItemDetails.iconLeftMargin
            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
        }
    }

    ColumnLayout {
        id: unifiedSearchResultSkeletonTextContainer
        Layout.fillWidth: true

        Rectangle {
            id: unifiedSearchResultSkeletonTitleText
            color: "grey"
            height: Style.topLinePixelSize
            Layout.leftMargin: unifiedSearchResultSkeletonItemDetails.textLeftMargin
            Layout.rightMargin: unifiedSearchResultSkeletonItemDetails.textRightMargin
            Layout.fillWidth: true
        }

        Rectangle {
            id: unifiedSearchResultSkeletonTextSubline
            color: "grey"
            height: Style.subLinePixelSize
            Layout.leftMargin: unifiedSearchResultSkeletonItemDetails.textLeftMargin
            Layout.rightMargin: unifiedSearchResultSkeletonItemDetails.textRightMargin
            Layout.fillWidth: true
        }
    }
}
