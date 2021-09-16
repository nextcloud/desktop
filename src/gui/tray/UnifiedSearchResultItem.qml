import QtQml 2.12
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import Style 1.0
import com.nextcloud.desktopclient 1.0

MouseArea {
    id: unifiedSearchResultMouseArea
    hoverEnabled: !model.isCategorySeparator
    anchors.fill: unifiedSearchResultItem
    
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        color: (parent.containsMouse ? Style.lightHover : "transparent")
    }

    RowLayout {
        id: unifiedSearchResultItem

        RowLayout {
            id: unifiedSearchResultItemDetails

            visible: !model.isFetchMoreTrigger && !model.isCategorySeparator

            width: !model.isFetchMoreTrigger && !model.isCategorySeparator ? unifiedSearchResultMouseArea.width : 0
            height: !model.isFetchMoreTrigger && !model.isCategorySeparator ? Style.trayWindowHeaderHeight : 0

            Accessible.role: Accessible.ListItem
            Accessible.name: resultTitle
            Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

            Column {
                id: unifiedSearchResultLeftColumn
                Layout.leftMargin: 8
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                spacing: 4
                Image {
                    id: unifiedSearchResultThumbnail
                    visible: parent.visible && !unifiedSearchResultThumbnailPlaceholder.visible
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    verticalAlignment: Qt.AlignCenter
                    asynchronous: true
                    cache: true
                    source: "image://unified-search-result-image/" + model.thumbnailUrl
                    sourceSize.height: 16
                    sourceSize.width: 16
                }
                Image {
                    id: unifiedSearchResultThumbnailPlaceholder
                    visible: model.thumbnailUrl && unifiedSearchResultThumbnail.status != Image.Ready
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    verticalAlignment: Qt.AlignCenter
                    asynchronous: true
                    cache: true
                    source: "qrc:///client/theme/change.svg"
                    sourceSize.height: 16
                    sourceSize.width: 16
                }
            }

            Column {
                id: unifiedSearchResultRightColumn
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 8
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                spacing: 4
                ColumnLayout {
                    spacing: 2
                    Rectangle {
                        Layout.preferredHeight: Style.trayWindowHeaderHeight / 2
                        Layout.fillWidth: true

                        Text {
                            id: unifiedSearchResultTitleText
                            text: model.resultTitle
                            visible: parent.visible
                            width: parent.width
                            font.pixelSize: Style.topLinePixelSize
                            color: "black"
                        }
                    }
                    Rectangle {
                        Layout.preferredHeight: Style.trayWindowHeaderHeight / 2
                        Layout.fillWidth: true
                        Text {
                            id: unifiedSearchResultTextSubline
                            text: model.subline
                            visible: parent.visible
                            width: parent.width
                            font.pixelSize: Style.subLinePixelSize
                            color: "grey"
                        }
                    }
                }


            }

        }

        RowLayout {
            id: unifiedSearchResultItemFetchMore
            visible: model.isFetchMoreTrigger

            width: model.isFetchMoreTrigger ? unifiedSearchResultMouseArea.width : 0
            height: model.isFetchMoreTrigger ? Style.trayWindowHeaderHeight : 0

            Accessible.role: Accessible.ListItem
            Accessible.name: qsTr("Load more results")
            Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

            Column {
                id: unifiedSearchResultItemFetchMoreColumn
                visible: model.isFetchMoreTrigger
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    id: unifiedSearchResultItemFetchMoreText
                    text: qsTr("Load more results")
                    visible: parent.visible
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width
                    height: parent.height
                    font.pixelSize: Style.topLinePixelSize
                    color: "grey"
                }
            }
        }

        RowLayout {
            id: unifiedSearchResultItemCategorySeparator
            visible: model.isCategorySeparator

            width: model.isCategorySeparator ? unifiedSearchResultMouseArea.width : 0
            height: model.isCategorySeparator ? Style.trayWindowHeaderHeight : 0
            spacing: 2

            Accessible.role: Accessible.ListItem
            Accessible.name: qsTr("Category separator")
            Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

            Column {
                id: unifiedSearchResultItemCategorySeparatorColumn
                visible: model.isCategorySeparator
                Layout.leftMargin: 8
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4
                Layout.alignment: Qt.AlignLeft

                Text {
                    id: unifiedSearchResultItemCategorySeparatorText
                    text: model.categoryName
                    visible: parent.visible
                    width: parent.width
                    font.pixelSize: Style.topLinePixelSize * 1.5
                    color: Style.ncBlue
                }
            }
        }
    }
}
