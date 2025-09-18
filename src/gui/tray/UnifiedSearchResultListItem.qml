/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls
import Style
import com.nextcloud.desktopclient

MouseArea {
    id: unifiedSearchResultMouseArea

    property string currentFetchMoreInProgressProviderId: ""

    readonly property bool isFetchMoreTrigger: model.typeAsString === "FetchMoreTrigger"

    property bool isFetchMoreInProgress: currentFetchMoreInProgressProviderId === model.providerId
    property bool isSearchInProgress: false

    property bool isPooled: false

    property var fetchMoreTriggerClicked: function(){}
    property var resultClicked: function(){}

    enabled: !isSearchInProgress
    hoverEnabled: enabled

    height: Style.unifiedSearchItemHeight

    ToolTip {
        visible: unifiedSearchResultMouseArea.containsMouse
        text: isFetchMoreTrigger ? qsTr("Load more results") : model.resultTitle + "\n\n" + model.subline
    }

    Rectangle {
        id: unifiedSearchResultHoverBackground
        anchors.fill: parent
        color: parent.containsMouse ? palette.highlight : "transparent"
    }

    Loader {
        anchors.fill: parent
        active: !isFetchMoreTrigger
        sourceComponent: UnifiedSearchResultItem {
            anchors.fill: parent
            title: model.resultTitle
            subline: model.subline
            icons: Style.darkMode ? model.darkIcons : model.lightIcons
            iconsIsThumbnail: Style.darkMode ? model.darkIconsIsThumbnail : model.lightIconsIsThumbnail
            iconPlaceholder: Style.darkMode ? model.darkImagePlaceholder : model.lightImagePlaceholder
            isRounded: model.isRounded && iconsIsThumbnail
        }
    }

    Loader {
        anchors.fill: parent
        active: isFetchMoreTrigger
        sourceComponent: UnifiedSearchResultFetchMoreTrigger {
            anchors.fill: parent
            isFetchMoreInProgress: unifiedSearchResultMouseArea.isFetchMoreInProgress
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
