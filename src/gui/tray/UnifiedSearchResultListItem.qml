import QtQml 2.12
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.0

MouseArea {
    id: unifiedSearchResultMouseArea

    property int textLeftMargin: 18
    property int textRightMargin: 16
    property int iconWidth: 24
    property int iconLeftMargin: 12

    property int titleFontSize: Style.topLinePixelSize
    property int sublineFontSize: Style.subLinePixelSize

    property string titleColor: "black"
    property string sublineColor: "grey"

    readonly property bool isFetchMoreTrigger: model.typeAsString === "FetchMoreTrigger"

    property bool isFetchMoreInProgress: unifiedSearchResultsModel.currentFetchMoreInProgressProviderId === model.providerId
    property bool isSearchInProgress: false

    property bool isPooled: false

    enabled: !isFetchMoreTrigger || !isSearchInProgress
    hoverEnabled: enabled

    ListView.onPooled: isPooled = true
    ListView.onReused: isPooled = false

    ToolTip {
        parent: unifiedSearchResultMouseArea
        visible: unifiedSearchResultMouseArea.containsMouse
        text: isFetchMoreTrigger ? qsTr("Load more results") : model.resultTitle + "\n\n" + model.subline
        delay: 1000
    }

    Rectangle {
        id: unifiedSearchResultHoverBackground
        anchors.fill: parent
        color: (parent.containsMouse ? Style.lightHover : "transparent")
    }

    Loader {
        active: !isFetchMoreTrigger
        sourceComponent: UnifiedSearchResultItem {
            width: unifiedSearchResultMouseArea.width
            height: unifiedSearchResultMouseArea.height
            title: model.resultTitle
            subline: model.subline
            icons: model.icons
            iconPlaceholder: model.imagePlaceholder
            isRounded: model.isRounded
            textLeftMargin: unifiedSearchResultMouseArea.textLeftMargin
            textRightMargin: unifiedSearchResultMouseArea.textRightMargin
            iconWidth: unifiedSearchResultMouseArea.iconWidth
            iconLeftMargin: unifiedSearchResultMouseArea.iconLeftMargin
            titleFontSize: unifiedSearchResultMouseArea.titleFontSize
            sublineFontSize: unifiedSearchResultMouseArea.sublineFontSize
            titleColor: unifiedSearchResultMouseArea.titleColor
            sublineColor: unifiedSearchResultMouseArea.sublineColor
        }
    }

    Loader {
        active: isFetchMoreTrigger
        sourceComponent: UnifiedSearchResultFetchMoreTrigger {
            isFetchMoreInProgress: unifiedSearchResultMouseArea.isFetchMoreInProgress
            width: unifiedSearchResultMouseArea.width
            height: unifiedSearchResultMouseArea.height
            isWihinViewPort: !unifiedSearchResultMouseArea.isPooled
            fontSize: unifiedSearchResultMouseArea.titleFontSize
            textColor: unifiedSearchResultMouseArea.sublineColor
        }
    }

    onClicked: {
        if (isFetchMoreTrigger) {
            unifiedSearchResultsModel.fetchMoreTriggerClicked(model.providerId)
        } else {
            unifiedSearchResultsModel.resultClicked(model.providerId, model.resourceUrlRole)
        }
    }
}
