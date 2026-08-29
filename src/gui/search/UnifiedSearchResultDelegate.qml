/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import Style
import com.nextcloud.desktopclient
import "qrc:/qml/src/gui/tray"

Item {
    id: root

    required property var searchModel
    required property string providerName
    required property string providerId
    required property string providerIcon
    required property string resultTitle
    required property string subline
    required property url resourceUrlRole
    required property string darkIcons
    required property string lightIcons
    required property bool darkIconsIsThumbnail
    required property bool lightIconsIsThumbnail
    required property string darkImagePlaceholder
    required property string lightImagePlaceholder
    required property bool isRounded
    required property int resultType
    required property bool isSelected
    required property bool isPartialMatch
    required property bool hasOverflow
    required property bool isLoading

    readonly property alias loadedItem: rowContent.item

    implicitHeight: {
        if (resultType === UnifiedSearchResultsListModel.ProviderHeader) {
            return Style.unifiedSearchProviderHeaderHeight
        }
        if (resultType === UnifiedSearchResultsListModel.PartialMatchesHeader) {
            return Style.unifiedSearchPartialMatchesHeaderHeight
        }
        if (resultType === UnifiedSearchResultsListModel.FetchMoreTrigger
                || resultType === UnifiedSearchResultsListModel.RetryFetchMoreTrigger) {
            return Style.unifiedSearchPagingRowHeight
        }
        return Style.unifiedSearchItemHeight
    }
    height: implicitHeight

    Loader {
        id: rowContent

        anchors.fill: parent
        sourceComponent: {
            if (root.resultType === UnifiedSearchResultsListModel.ProviderHeader) {
                return providerHeader
            }
            if (root.resultType === UnifiedSearchResultsListModel.PartialMatchesHeader) {
                return partialHeader
            }
            if (root.resultType === UnifiedSearchResultsListModel.FetchMoreTrigger
                    || root.resultType === UnifiedSearchResultsListModel.RetryFetchMoreTrigger) {
                return pagingRow
            }
            return resultRow
        }
    }

    Component {
        id: providerHeader

        Button {
            id: providerHeaderButton

            objectName: "providerHeaderRow"
            width: root.width
            height: Style.unifiedSearchProviderHeaderHeight
            flat: true
            text: root.hasOverflow ? qsTr("More from %1  →").arg(root.providerName) : root.providerName
            font.bold: false
            font.pixelSize: Style.unifiedSearchResultTitleFontSize
            leftPadding: 0
            rightPadding: 0
            hoverEnabled: root.hasOverflow
            activeFocusOnTab: root.hasOverflow
            Accessible.role: root.hasOverflow ? Accessible.Button : Accessible.StaticText
            Accessible.name: text

            HoverHandler {
                enabled: root.hasOverflow
                cursorShape: Qt.PointingHandCursor
            }

            background: Rectangle {
                color: root.hasOverflow && providerHeaderButton.hovered
                    ? Style.listItemHoverBackground
                    : "transparent"
                radius: Style.mediumRoundedButtonRadius
            }

            contentItem: EnforcedPlainTextLabel {
                text: providerHeaderButton.text
                color: Style.wizardPrimaryText
                font: providerHeaderButton.font
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }

            onPressed: {
                if (root.hasOverflow) {
                    root.searchModel.openProviderDetail(root.providerId)
                }
            }
        }
    }

    Component {
        id: partialHeader

        EnforcedPlainTextLabel {
            objectName: "partialMatchesHeaderRow"
            width: root.width
            height: Style.unifiedSearchPartialMatchesHeaderHeight
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Partial matches")
            color: Style.wizardSecondaryText
            font.bold: true
        }
    }

    Component {
        id: resultRow

        ItemDelegate {
            id: resultDelegateButton

            objectName: "searchResultRow"
            width: root.width
            height: Style.unifiedSearchItemHeight
            leftPadding: 0
            rightPadding: 0
            topPadding: 0
            bottomPadding: 0
            activeFocusOnTab: false
            opacity: root.isPartialMatch ? Style.unifiedSearchPartialMatchOpacity : 1.0
            hoverEnabled: true
            Accessible.role: Accessible.ListItem
            Accessible.name: root.resultTitle
            Accessible.description: root.subline
            Accessible.selected: root.isSelected

            background: Rectangle {
                color: root.isSelected
                    ? Style.wizardSecondaryButtonPressed
                    : (resultDelegateButton.hovered ? Style.listItemHoverBackground : "transparent")
                radius: Style.mediumRoundedButtonRadius
            }

            contentItem: UnifiedSearchResultItem {
                accessibilityEnabled: false
                title: root.resultTitle
                subline: root.subline
                icons: Style.darkMode ? root.darkIcons : root.lightIcons
                iconsIsThumbnail: Style.darkMode ? root.darkIconsIsThumbnail : root.lightIconsIsThumbnail
                iconPlaceholder: Style.darkMode ? root.darkImagePlaceholder : root.lightImagePlaceholder
                isRounded: root.isRounded && iconsIsThumbnail
            }

            onClicked: root.searchModel.resultClicked(root.providerId, root.resourceUrlRole)
        }
    }

    Component {
        id: pagingRow

        ItemDelegate {
            id: pagingDelegate

            objectName: "searchPagingRow"
            width: root.width
            enabled: !root.isLoading
            hoverEnabled: true
            leftPadding: Style.unifiedSearchResultIconLeftMargin
            rightPadding: Style.unifiedSearchResultTextRightMargin
            topPadding: 0
            bottomPadding: 0
            text: root.isLoading
                ? qsTr("Loading more results …")
                : (root.resultType === UnifiedSearchResultsListModel.RetryFetchMoreTrigger
                    ? qsTr("Retry loading more results") : qsTr("Load more results"))
            icon.source: "image://svgimage-custom-color/more.svg/" + Style.wizardPrimaryText
            icon.width: Style.smallIconSize
            icon.height: Style.smallIconSize
            Accessible.role: Accessible.Button
            Accessible.name: text

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }

            background: Rectangle {
                color: pagingDelegate.hovered ? Style.listItemHoverBackground : "transparent"
                radius: Style.mediumRoundedButtonRadius
            }

            contentItem: RowLayout {
                spacing: Style.smallSpacing

                BusyIndicator {
                    Layout.preferredWidth: Style.smallIconSize
                    Layout.preferredHeight: Style.smallIconSize
                    running: root.isLoading
                    visible: running
                }

                Image {
                    Layout.preferredWidth: Style.smallIconSize
                    Layout.preferredHeight: Style.smallIconSize
                    sourceSize.width: Style.smallIconSize
                    sourceSize.height: Style.smallIconSize
                    source: pagingDelegate.icon.source
                    visible: !root.isLoading
                    Accessible.ignored: true
                }

                EnforcedPlainTextLabel {
                    objectName: "searchPagingLabel"
                    Layout.fillWidth: true
                    text: pagingDelegate.text
                    color: Style.wizardPrimaryText
                    font.bold: false
                    font.pixelSize: Style.unifiedSearchResultTitleFontSize
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            onPressed: root.resultType === UnifiedSearchResultsListModel.RetryFetchMoreTrigger
                ? root.searchModel.retryLoadMore(root.providerId)
                : root.searchModel.loadMore(root.providerId)
        }
    }
}
