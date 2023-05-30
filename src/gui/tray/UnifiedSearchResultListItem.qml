/*
 * Copyright (C) 2021 by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
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

    enabled: !isSearchInProgress
    hoverEnabled: enabled

    height: Style.unifiedSearchItemHeight

    NCToolTip {
        visible: unifiedSearchResultMouseArea.containsMouse
        text: isFetchMoreTrigger ? qsTr("Load more results") : model.resultTitle + "\n\n" + model.subline
    }

    Rectangle {
        id: unifiedSearchResultHoverBackground
        anchors.fill: parent
        color: (parent.containsMouse ? palette.highlight : "transparent")
    }

    Loader {
        anchors.fill: parent
        active: !isFetchMoreTrigger
        sourceComponent: UnifiedSearchResultItem {
            anchors.fill: parent
            title: model.resultTitle
            subline: model.subline
            icons: Theme.darkMode ? model.darkIcons : model.lightIcons
            iconsIsThumbnail: Theme.darkMode ? model.darkIconsIsThumbnail : model.lightIconsIsThumbnail
            iconPlaceholder: Theme.darkMode ? model.darkImagePlaceholder : model.lightImagePlaceholder
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
