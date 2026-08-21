/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import Style
import com.nextcloud.desktopclient
import "qrc:/qml/src/gui"
import "qrc:/qml/src/gui/wizard/qml"

WizardStyledWindow {
    id: root

    property var account: null
    property var searchModel: null
    property bool filtersRevealed: false
    readonly property bool aggregateView: searchModel && searchModel.viewMode === UnifiedSearchResultsListModel.Aggregate
    readonly property bool filtersVisible: aggregateView && searchModel && searchModel.providersReady
        && (filtersRevealed || searchModel.searchTerm.length > 0 || searchModel.activeFilters.length > 0)
    readonly property int searchState: searchModel ? searchModel.searchState : UnifiedSearchResultsListModel.Placeholder

    title: ""
    width: Style.searchWindowWidth
    height: Style.searchWindowHeight
    minimumWidth: Style.wizardStandaloneWindowMinimumWidth
    minimumHeight: Style.wizardStandaloneWindowMinimumHeight

    function focusSearchInput() {
        if (visible && searchInput.enabled) searchInput.forceActiveFocus()
    }

    function hasActiveFilter(type) {
        if (!searchModel) {
            return false
        }
        const filters = searchModel.activeFilters
        for (let index = 0; index < filters.length; ++index) {
            if (filters[index].type === type) {
                return true
            }
        }
        return false
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: !typeMenu.opened && !dateMenu.opened && !peoplePopup.opened && !customRangeDialog.opened
        onActivated: root.close()
    }

    UnifiedSearchPeopleModel {
        id: peopleModel
        accountState: root.searchModel ? root.searchModel.accountState : null
    }

    Connections {
        target: root.searchModel
        function onSelectedRowChanged() {
            if (root.searchModel && root.searchModel.selectedRow >= 0
                    && root.searchModel.selectedRow < resultsList.count) {
                resultsList.positionViewAtIndex(root.searchModel.selectedRow, ListView.Contain)
            }
        }
        function onViewModeChanged() {
            Qt.callLater(function() {
                if (root.searchModel && root.searchModel.selectedRow >= 0
                        && root.searchModel.selectedRow < resultsList.count) {
                    resultsList.positionViewAtIndex(root.searchModel.selectedRow, ListView.Beginning)
                }
            })
        }
        function onAccessibilityStatusChanged() {
            if (root.searchModel.accessibilityStatus.length > 0)
                Accessible.announce(root.searchModel.accessibilityStatus, Accessible.Polite)
        }
    }

    Component.onCompleted: Qt.callLater(focusSearchInput)
    onVisibleChanged: if (visible) Qt.callLater(focusSearchInput)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Style.wizardWindowMargin
        anchors.topMargin: Style.wizardWindowTopMargin
        spacing: Style.smallSpacing

        WindowAccountHeader {
            Layout.fillWidth: true
            title: qsTr("Search")
            user: root.account
        }

        UnifiedSearchInputContainer {
            id: searchInput
            Layout.fillWidth: true
            Layout.preferredHeight: Style.unifiedSearchInputContainerHeight
            enabled: root.searchModel !== null
            readOnly: !root.searchModel || !root.searchModel.canEditSearch
            text: root.searchModel ? root.searchModel.searchTerm : ""
            isSearchInProgress: root.searchModel
                && (root.searchModel.isSearchInProgress || root.searchModel.waitingForSearchTermEditEnd)
            placeholderText: root.searchModel && !root.searchModel.isAccountConnected
                ? qsTr("Search is available when this account is connected")
                : qsTr("Search files, messages, events …")
            onTextEdited: if (root.searchModel) root.searchModel.searchTerm = text
            onClearText: if (root.searchModel) root.searchModel.searchTerm = ""
            onToggleFilters: root.filtersRevealed = !root.filtersRevealed
            onMoveSelection: direction => root.searchModel.moveSelection(direction)
            onActivateSelection: root.searchModel.activateSelected()
        }

        Item {
            id: detailHeader

            objectName: "searchDetailHeader"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            visible: root.searchModel && !root.aggregateView

            ToolButton {
                id: backButton

                objectName: "searchDetailBackButton"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Back")
                icon.source: "image://svgimage-custom-color/"
                    + (root.LayoutMirroring.enabled ? "arrow-right.svg/" : "arrow-left.svg/")
                    + Style.wizardPrimaryText
                icon.width: Style.smallIconSize
                icon.height: Style.smallIconSize
                display: AbstractButton.TextBesideIcon
                Accessible.name: qsTr("Back to all search results")
                onClicked: {
                    root.searchModel.closeProviderDetail()
                    root.focusSearchInput()
                }
            }
            Label {
                objectName: "searchDetailProviderTitle"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: backButton.width + Style.smallSpacing
                anchors.rightMargin: backButton.width + Style.smallSpacing
                anchors.verticalCenter: parent.verticalCenter
                text: root.searchModel ? root.searchModel.detailProviderName : ""
                font.bold: true
                font.pixelSize: Style.wizardHeaderTitleFontPixelSize
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Flow {
            id: filterFlow
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? childrenRect.height : 0
            visible: root.filtersVisible
            spacing: Style.smallSpacing

            WizardButton {
                objectName: "typeFilterButton"
                width: Math.max(140, (filterFlow.width - 2 * filterFlow.spacing) / 3)
                text: qsTr("Type")
                trailingIconSource: "image://svgimage-custom-color/caret-down.svg/"
                    + (primary ? Style.wizardSelectedText : Style.wizardPrimaryText)
                iconBeforeText: true
                iconSource: "image://svgimage-custom-color/folder.svg/"
                    + (primary ? Style.wizardSelectedText : Style.wizardPrimaryText)
                primary: root.hasActiveFilter("provider")
                Accessible.name: qsTr("Filter by type")
                onClicked: typeMenu.open()
                Menu {
                    id: typeMenu
                    width: parent.width * 1.5
                    Repeater {
                        model: root.searchModel ? root.searchModel.providers : []
                        delegate: WizardMenuItem {
                            required property var modelData
                            text: (modelData.selected ? "✓ " : "") + modelData.name
                            icon.source: modelData.icon ? "image://tray-image-provider/" + modelData.icon : ""
                            tintIcon: true
                            iconTintColor: Style.wizardPrimaryText
                            onTriggered: root.searchModel.toggleProviderFilter(modelData.id)
                        }
                    }
                }
            }
            WizardButton {
                objectName: "dateFilterButton"
                width: Math.max(140, (filterFlow.width - 2 * filterFlow.spacing) / 3)
                text: qsTr("Date")
                trailingIconSource: "image://svgimage-custom-color/caret-down.svg/"
                    + (primary ? Style.wizardSelectedText : Style.wizardPrimaryText)
                iconBeforeText: true
                iconSource: "image://svgimage-custom-color/calendar.svg/"
                    + (primary ? Style.wizardSelectedText : Style.wizardPrimaryText)
                primary: root.hasActiveFilter("date")
                enabled: root.searchModel && root.searchModel.dateFilterAvailable
                Accessible.name: qsTr("Filter by date")
                Accessible.description: enabled ? "" : qsTr("No search source supports date filtering")
                onClicked: dateMenu.open()
                Menu {
                    id: dateMenu
                    WizardMenuItem {
                        objectName: "dateTodayMenuItem"
                        text: qsTr("Today")
                        onTriggered: root.searchModel.setDatePreset("today")
                    }
                    WizardMenuItem { text: qsTr("Last 7 days"); onTriggered: root.searchModel.setDatePreset("last7days") }
                    WizardMenuItem { text: qsTr("Last 30 days"); onTriggered: root.searchModel.setDatePreset("last30days") }
                    WizardMenuItem { text: qsTr("This year"); onTriggered: root.searchModel.setDatePreset("thisyear") }
                    WizardMenuItem { text: qsTr("Last year"); onTriggered: root.searchModel.setDatePreset("lastyear") }
                    MenuSeparator {}
                    WizardMenuItem {
                        text: qsTr("Custom range …")
                        onTriggered: {
                            customRangeDialog.validationError = false
                            customRangeDialog.open()
                        }
                    }
                    WizardMenuItem { text: qsTr("Clear date"); onTriggered: root.searchModel.clearDateFilter() }
                }
            }
            WizardButton {
                id: peopleButton
                objectName: "peopleFilterButton"
                width: Math.max(140, (filterFlow.width - 2 * filterFlow.spacing) / 3)
                text: qsTr("People")
                trailingIconSource: "image://svgimage-custom-color/caret-down.svg/"
                    + (primary ? Style.wizardSelectedText : Style.wizardPrimaryText)
                iconBeforeText: true
                iconSource: "image://svgimage-custom-color/account-group.svg/"
                    + (primary ? Style.wizardSelectedText : Style.wizardPrimaryText)
                primary: root.hasActiveFilter("person")
                enabled: root.searchModel && root.searchModel.peopleFilterAvailable
                Accessible.name: qsTr("Filter by person")
                Accessible.description: enabled ? "" : qsTr("No search source supports people filtering")
                onClicked: peoplePopup.open()
            }
        }

        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            visible: root.filtersVisible && root.searchModel && root.searchModel.activeFilters.length > 0
            spacing: Style.smallSpacing
            Repeater {
                model: root.searchModel ? root.searchModel.activeFilters : []
                delegate: WizardChipButton {
                    id: chipButton
                    objectName: "activeFilterChip"
                    required property var modelData
                    text: modelData.label
                    textSuffix: "×"
                    iconBeforeText: true
                    iconSource: modelData.icon ? "image://tray-image-provider/" + modelData.icon : ""
                    tintIcon: true
                    iconTintColor: Style.wizardPrimaryText
                    Accessible.name: qsTr("Remove %1 filter").arg(modelData.label)
                    onClicked: root.searchModel.removeFilter(modelData.type, modelData.id)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Style.normalBorderWidth
            color: Style.wizardRowBorder
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width, 420)
                visible: root.searchState === UnifiedSearchResultsListModel.SearchError
                ErrorBox { Layout.fillWidth: true; text: root.searchModel ? root.searchModel.errorString : "" }
                WizardButton {
                    Layout.alignment: Qt.AlignHCenter
                    primary: true
                    text: qsTr("Retry")
                    onClicked: root.searchModel.retry()
                }
            }

            UnifiedSearchPlaceholderView {
                anchors.fill: parent
                visible: root.searchState === UnifiedSearchResultsListModel.Placeholder
            }

            UnifiedSearchResultNothingFound {
                anchors.fill: parent
                visible: root.searchState === UnifiedSearchResultsListModel.NothingFound
                text: root.searchModel ? root.searchModel.searchTerm : ""
            }

            ScrollView {
                anchors.fill: parent
                contentWidth: availableWidth
                visible: root.searchState === UnifiedSearchResultsListModel.Results
                      || root.searchState === UnifiedSearchResultsListModel.Skeleton
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ListView {
                    id: resultsList

                    objectName: "searchResultsList"
                    clip: true
                    reuseItems: true
                    spacing: Style.extraSmallSpacing
                    model: root.searchModel
                    currentIndex: root.searchModel ? root.searchModel.selectedRow : -1
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Search results")

                    delegate: Item {
                        id: delegateData

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
                        required property int type
                        required property bool isSelected
                        required property bool isPartialMatch
                        required property bool hasOverflow
                        required property bool isLoading

                        readonly property alias loadedItem: resultDelegate.loadedItem

                        width: resultsList.width
                        height: resultDelegate.implicitHeight

                        UnifiedSearchResultDelegate {
                            id: resultDelegate

                            anchors.fill: parent
                            searchModel: root.searchModel
                            providerName: delegateData.providerName
                            providerId: delegateData.providerId
                            providerIcon: delegateData.providerIcon
                            resultTitle: delegateData.resultTitle
                            subline: delegateData.subline
                            resourceUrlRole: delegateData.resourceUrlRole
                            darkIcons: delegateData.darkIcons
                            lightIcons: delegateData.lightIcons
                            darkIconsIsThumbnail: delegateData.darkIconsIsThumbnail
                            lightIconsIsThumbnail: delegateData.lightIconsIsThumbnail
                            darkImagePlaceholder: delegateData.darkImagePlaceholder
                            lightImagePlaceholder: delegateData.lightImagePlaceholder
                            isRounded: delegateData.isRounded
                            resultType: delegateData.type
                            isSelected: delegateData.isSelected
                            isPartialMatch: delegateData.isPartialMatch
                            hasOverflow: delegateData.hasOverflow
                            isLoading: delegateData.isLoading
                        }
                    }

                    footer: Column {
                        objectName: "searchResultsLoadingFooter"
                        width: resultsList.width
                        height: visible ? implicitHeight : 0
                        spacing: Style.smallSpacing
                        visible: root.searchModel && root.searchModel.isSearchInProgress
                        Accessible.ignored: true
                        Repeater {
                            model: 3
                            Rectangle {
                                required property int index
                                width: resultsList.width * (0.72 + index * 0.07)
                                height: 44
                                radius: 8
                                color: palette.alternateBase
                                opacity: 0.55
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            objectName: "partialFailureFooter"
            Layout.fillWidth: true
            visible: root.aggregateView && root.searchModel && root.searchModel.hasPartialFailure
            Label { Layout.fillWidth: true; text: qsTr("Some sources unavailable"); color: palette.placeholderText }
            WizardButton { text: qsTr("Retry"); onClicked: root.searchModel.retryFailedProviders() }
        }

        WizardButton {
            Layout.fillWidth: true
            visible: root.searchModel && root.searchModel.showConnectedServicesAction
            text: root.searchModel && root.searchModel.externalProvidersEnabled
                ? qsTr("Less from connected services") : qsTr("More from connected services")
            onClicked: root.searchModel.setExternalProvidersEnabled(!root.searchModel.externalProvidersEnabled)
        }
    }

    Popup {
        id: peoplePopup
        parent: Overlay.overlay
        width: Math.min(root.width - 40, 420)
        height: 340
        x: (root.width - width) / 2
        y: 150
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: peopleSearch.forceActiveFocus()
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Style.smallSpacing
            TextField {
                id: peopleSearch
                Layout.fillWidth: true
                placeholderText: qsTr("Search people")
                onTextEdited: peopleModel.searchTerm = text
            }
            Label { visible: peopleModel.errorString.length > 0; text: peopleModel.errorString; wrapMode: Text.Wrap }
            WizardButton {
                visible: peopleModel.errorString.length > 0
                text: qsTr("Retry")
                onClicked: peopleModel.retry()
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: peopleModel
                delegate: ItemDelegate {
                    id: personDelegate
                    required property string userId
                    required property string displayName
                    required property string avatarUrl
                    width: ListView.view.width
                    height: 44
                    text: displayName
                    hoverEnabled: true
                    Accessible.description: userId
                    background: Rectangle {
                        color: personDelegate.hovered || personDelegate.down
                            ? Style.listItemHoverBackground
                            : "transparent"
                        radius: Style.mediumRoundedButtonRadius
                    }
                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                    contentItem: RowLayout {
                        Image {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            sourceSize.width: 32
                            sourceSize.height: 32
                            asynchronous: true
                            source: personDelegate.avatarUrl.length > 0
                                ? "image://tray-image-provider/" + personDelegate.avatarUrl : ""
                            Accessible.ignored: true
                        }
                        Label { Layout.fillWidth: true; text: personDelegate.displayName; elide: Text.ElideRight }
                    }
                    onClicked: {
                        root.searchModel.setPersonFilter(userId, displayName, avatarUrl)
                        peoplePopup.close()
                        root.focusSearchInput()
                    }
                }
            }
        }
    }

    Dialog {
        id: customRangeDialog
        property bool validationError: false
        anchors.centerIn: parent
        title: qsTr("Custom date range")
        modal: true

        footer: RowLayout {
            spacing: Style.wizardFooterSpacing

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                text: qsTr("Cancel")
                onClicked: customRangeDialog.close()
            }

            WizardButton {
                primary: true
                text: qsTr("Apply")
                onClicked: {
                    customRangeDialog.validationError = !root.searchModel.setCustomDateRange(customSince.text, customUntil.text)
                    if (!customRangeDialog.validationError) {
                        customRangeDialog.close()
                    }
                }
            }
        }

        ColumnLayout {
            Label { text: qsTr("Start date (YYYY-MM-DD)") }
            TextField { id: customSince; Layout.fillWidth: true; placeholderText: "YYYY-MM-DD" }
            Label { text: qsTr("End date (YYYY-MM-DD)") }
            TextField { id: customUntil; Layout.fillWidth: true; placeholderText: "YYYY-MM-DD" }
            Label {
                visible: customRangeDialog.validationError
                text: qsTr("Enter valid dates with the start date before the end date.")
                color: palette.accent
            }
        }
    }

}
