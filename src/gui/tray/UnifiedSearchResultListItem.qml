import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import Style 1.0
import com.nextcloud.desktopclient 1.0

MouseArea {
    id: unifiedSearchResultMouseArea

    property string currentFetchMoreInProgressProviderId: ""

    readonly property bool isFetchMoreTrigger: model.typeAsString === "FetchMoreTrigger"

    property bool isFetchMoreInProgress: currentFetchMoreInProgressProviderId === model.providerId
    property bool isSearchInProgress: false

    property bool isPooled: false

    property var fetchMoreTriggerClicked: function(){}
    property var resultClicked: function(){}

    enabled: !isFetchMoreTrigger || !isSearchInProgress
    hoverEnabled: enabled

    height: Style.unifiedSearchItemHeight

    NCToolTip {
        visible: unifiedSearchResultMouseArea.containsMouse
        text: isFetchMoreTrigger ? qsTr("Load more results") : model.resultTitle + "\n\n" + model.subline
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
            icons: Theme.darkMode ? model.darkIcons : model.lightIcons
            iconPlaceholder: Theme.darkMode ? model.darkImagePlaceholder : model.lightImagePlaceholder
            isRounded: model.isRounded
        }
    }

    Loader {
        active: isFetchMoreTrigger
        sourceComponent: UnifiedSearchResultFetchMoreTrigger {
            isFetchMoreInProgress: unifiedSearchResultMouseArea.isFetchMoreInProgress
            width: unifiedSearchResultMouseArea.width
            height: unifiedSearchResultMouseArea.height
            isWithinViewPort: !unifiedSearchResultMouseArea.isPooled
        }
    }

    onClicked: {
        if (isFetchMoreTrigger) {
            unifiedSearchResultMouseArea.fetchMoreTriggerClicked(model.providerId)
        } else {
            unifiedSearchResultMouseArea.resultClicked(model.providerId, model.resourceUrlRole)
        }
    }
}
