import QtQml 2.12
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.0

RowLayout {
    id: unifiedSearchResultSkeletonItemDetails

    readonly property int iconLeftMargin: 12
    readonly property int textLeftMargin: 4
    readonly property int textRightMargin: 16

    property int index: 0

    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("Search result skeleton.").arg(index)

    ColumnLayout {
        id: unifiedSearchResultSkeletonImageContainer
        readonly property int iconWidth: 24
        visible: true
        Layout.preferredWidth: iconWidth + 10
        Layout.preferredHeight: Style.trayWindowHeaderHeight
        Rectangle {
            id: unifiedSearchResultSkeletonThumbnail
            color: "grey"
            width:  unifiedSearchResultSkeletonImageContainer.iconWidth
            height:  unifiedSearchResultSkeletonImageContainer.iconWidth
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
            height: Style.topLinePixelSize
            Layout.leftMargin: unifiedSearchResultSkeletonItemDetails.textLeftMargin
            Layout.rightMargin: unifiedSearchResultSkeletonItemDetails.textRightMargin
            Layout.fillWidth: true
        }
    }
}
