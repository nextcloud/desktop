import QtQml 2.12
import QtQuick 2.9
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.0

MouseArea {
    id: unifiedSearchResultMouseArea

    property int defaultHeight: 0

    readonly property int contentLeftMargin: 4
    readonly property int contentRightMargin: contentLeftMargin

    readonly property bool isFetchMoreTrigger: model.typeAsString === "FetchMoreTrigger"
    readonly property bool isCategorySeparator: model.typeAsString === "CategorySeparator"

    property string currentFetchMoreInProgressCategoryId: ""

    property bool isFetchMoreInProgress: currentFetchMoreInProgressCategoryId === model.categoryId
    property bool isSearchInProgress: false

    enabled: !isCategorySeparator && !isSearchInProgress
    hoverEnabled: !isCategorySeparator && !isSearchInProgress

    height: !isCategorySeparator ? defaultHeight : defaultHeight/2

    Rectangle {
        id: unifiedSearchResultHoverBackground
        anchors.fill: parent
        color: (parent.containsMouse ? Style.lightHover : "transparent")
    }

    RowLayout {
        id: unifiedSearchResultItemDetails

        visible: !isFetchMoreTrigger && !isCategorySeparator

        width: visible ? unifiedSearchResultMouseArea.width : 0
        height: visible ? Style.trayWindowHeaderHeight : 0

        Accessible.role: Accessible.ListItem
        Accessible.name: resultTitle
        Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

        ColumnLayout {
            id: unifiedSearchResultImageContainer
            readonly property int iconWidth: Style.trayWindowHeaderHeight / 2
            visible: true
            Layout.preferredWidth: visible ? Layout.preferredHeight : 0
            Layout.preferredHeight: visible ? Style.trayWindowHeaderHeight : 0
            Image {
                id: unifiedSearchResultThumbnail
                visible: false
                asynchronous: true
                source: "image://unified-search-result-image/" + model.images
                cache: true
                sourceSize.width: imageData.width
                sourceSize.height: imageData.height
                width: imageData.width
                height: imageData.height
            }
            Rectangle {
                id: mask
                visible: false
                radius: model.isRounded ? width / 2 : 0
                width: imageData.width
                height: imageData.height
            }
            OpacityMask {
                id: imageData
                visible: !unifiedSearchResultThumbnailPlaceholder.visible
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                Layout.leftMargin: contentLeftMargin
                Layout.preferredWidth: model.images ? unifiedSearchResultImageContainer.iconWidth : 0
                Layout.preferredHeight: model.images ? unifiedSearchResultImageContainer.iconWidth: 0
                source: unifiedSearchResultThumbnail
                maskSource: mask
            }
            Image {
                id: unifiedSearchResultThumbnailPlaceholder
                visible: model.images && unifiedSearchResultThumbnail.status != Image.Ready
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                Layout.leftMargin: contentLeftMargin
                verticalAlignment: Qt.AlignCenter
                cache: true
                source: model.imagePlaceholder ? imagePlaceholder : "qrc:///client/theme/change.svg"
                sourceSize.height: unifiedSearchResultImageContainer.iconWidth
                sourceSize.width: unifiedSearchResultImageContainer.iconWidth
                Layout.preferredWidth: visible ? unifiedSearchResultImageContainer.iconWidth : 0
                Layout.preferredHeight: visible ? unifiedSearchResultImageContainer.iconWidth : 0
            }
        }

        ColumnLayout {
            id: unifiedSearchResultTextContainer
            Layout.fillWidth: true

            Text {
                id: unifiedSearchResultTitleText
                text: model.resultTitle.replace(/[\r\n]+/g, " ")
                visible: parent.visible
                Layout.leftMargin: contentLeftMargin
                Layout.rightMargin: contentRightMargin
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pixelSize: Style.topLinePixelSize
                color: "black"
            }
            Text {
                id: unifiedSearchResultTextSubline
                text: model.subline.replace(/[\r\n]+/g, " ")
                elide: Text.ElideRight
                visible: parent.visible
                Layout.leftMargin: contentLeftMargin
                Layout.rightMargin: contentRightMargin
                Layout.fillWidth: true
                color: "grey"
            }
        }

    }

    RowLayout {
        id: unifiedSearchResultItemFetchMore
        visible: isFetchMoreTrigger

        width: visible ? unifiedSearchResultMouseArea.width : 0
        height: visible ? Style.trayWindowHeaderHeight : 0

        Accessible.role: Accessible.ListItem
        Accessible.name: qsTr("Load more results")
        Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

        Column {
            id: unifiedSearchResultItemFetchMoreColumn
            visible: isFetchMoreTrigger
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                id: unifiedSearchResultItemFetchMoreText
                text: qsTr("Load more results")
                visible: parent.visible && !isFetchMoreInProgress
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                width: parent.visible && !isFetchMoreInProgress ? parent.width : 0
                height: parent.visible && !isFetchMoreInProgress ? parent.height: 0
                font.pixelSize: Style.topLinePixelSize
                color: "grey"
            }
            Image {
                id: unifiedSearchResultItemFetchMoreIconInProgress
                visible: parent.visible && isFetchMoreInProgress
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    verticalCenter: parent.verticalCenter
                }
                cache: true
                source: "qrc:///client/theme/change.svg"
                sourceSize.height: Style.trayWindowHeaderHeight / 2
                sourceSize.width: Style.trayWindowHeaderHeight / 2
                Layout.preferredWidth: visible ? Style.trayWindowHeaderHeight / 2 : 0
                Layout.preferredHeight: visible ? Style.trayWindowHeaderHeight / 2 : 0

                ColorOverlay {
                    anchors.fill: parent
                    source: parent
                    color: Style.menuBorder
                }
            }

            RotationAnimator {
                target: unifiedSearchResultItemFetchMoreIconInProgress
                running: unifiedSearchResultItemFetchMoreIconInProgress.visible
                from: 0
                to: 360
                loops: Animation.Infinite
                duration: 1250
            }
        }
    }

    RowLayout {
        id: unifiedSearchResultItemCategorySeparator
        visible: isCategorySeparator

        width: visible ? unifiedSearchResultMouseArea.width : 0

        Accessible.role: Accessible.ListItem
        Accessible.name: qsTr("Category separator")
        Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

        Column {
            id: unifiedSearchResultItemCategorySeparatorColumn
            visible: isCategorySeparator
            Layout.topMargin: 8
            Layout.leftMargin: contentLeftMargin
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

            Text {
                id: unifiedSearchResultItemCategorySeparatorText
                text: model.categoryName
                visible: parent.visible
                width: parent.width
                height: parent.height
                font.pixelSize: Style.topLinePixelSize
                color: Style.ncBlue
            }
        }
    }
}
