import QtQml 2.12
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.0

MouseArea {
    id: unifiedSearchResultMouseArea

    readonly property int iconLeftMargin: 12
    readonly property int textLeftMargin: 4
    readonly property int textRightMargin: 16
    readonly property int categorySeparatorLeftMargin: 16

    readonly property bool isFetchMoreTrigger: model.typeAsString === "FetchMoreTrigger"

    property bool isFetchMoreInProgress: unifiedSearchResultsModel.currentFetchMoreInProgressProviderId === model.providerId
    property bool isSearchInProgress: false

    enabled: !isFetchMoreTrigger || !isSearchInProgress
    hoverEnabled: enabled

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
            title: model.resultTitle
            subline: model.subline
            icons: model.icons
            iconPlaceholder: model.imagePlaceholder
            isRounded: model.isRounded
        }
    }

    Loader {
        active: isFetchMoreTrigger
        sourceComponent: UnifiedSearchResultFetchMoreTrigger {
            isFetchMoreInProgress: unifiedSearchResultMouseArea.isFetchMoreInProgress
            width: unifiedSearchResultMouseArea.width
            height: Style.trayWindowHeaderHeight
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
