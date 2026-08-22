/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest
import com.nextcloud.desktopclient
import com.nextcloud.desktopclient.search
import Style
import "qrc:/qml/src/gui/wizard/qml"

Item {
    width: 640
    height: 220

    Component {
        id: productionSearchWindow

        SearchWindow {}
    }

    Component {
        id: windowSearchModel

        ListModel {
            property int viewMode: UnifiedSearchResultsListModel.Aggregate
            property int searchState: UnifiedSearchResultsListModel.Results
            property int selectedRow: 0
            property string searchTerm: "calendar"
            property string accessibilityStatus: ""
            property string detailProviderName: "Files"
            property string errorString: ""
            property var accountState: null
            property var activeFilters: []
            property var providers: []
            property bool providersReady: true
            property bool canEditSearch: true
            property bool waitingForSearchTermEditEnd: false
            property bool isSearchInProgress: false
            property bool isAccountConnected: true
            property bool dateFilterAvailable: false
            property bool peopleFilterAvailable: false
            property bool hasPartialFailure: false
            property bool showConnectedServicesAction: false
            property bool externalProvidersEnabled: false
            property string currentFetchMoreInProgressProviderId: ""
            readonly property bool isFetchMoreInProgress: currentFetchMoreInProgressProviderId.length > 0

            function moveSelection(direction) {}
            function activateSelected() {}
            function closeProviderDetail() {}
            function retry() {}
            function retryFailedProviders() {}
            function setExternalProvidersEnabled(enabled) {}
            function removeFilter(type, id) {}

            Component.onCompleted: {
                const row = {
                    providerName: "Files",
                    providerId: "files",
                    providerIcon: "",
                    resultTitle: "Calendar",
                    subline: "Documents",
                    resourceUrlRole: "https://cloud.example.test/f/1",
                    darkIcons: "",
                    lightIcons: "",
                    darkIconsIsThumbnail: false,
                    lightIconsIsThumbnail: false,
                    darkImagePlaceholder: "",
                    lightImagePlaceholder: "",
                    isRounded: false,
                    type: UnifiedSearchResultsListModel.Default,
                    isSelected: true,
                    isPartialMatch: false,
                    hasOverflow: false,
                    isLoading: false
                }
                append(row)
                row.resultTitle = "Calendar 2"
                row.resourceUrlRole = "https://cloud.example.test/f/2"
                row.isSelected = false
                append(row)
            }
        }
    }

    QtObject {
        id: fakeSearchModel

        property int openedProviderDetails: 0
        property int activatedResults: 0
        property int loadedPages: 0
        property int retriedPages: 0

        function openProviderDetail(providerId) {
            if (providerId === "files") {
                ++openedProviderDetails
            }
        }
        function resultClicked(providerId, resourceUrl) {
            if (providerId === "files" && resourceUrl.toString().length > 0) {
                ++activatedResults
            }
        }
        function retryLoadMore(providerId) {
            if (providerId === "files") {
                ++retriedPages
            }
        }
        function loadMore(providerId) {
            if (providerId === "files") {
                ++loadedPages
            }
        }
    }

    UnifiedSearchInputContainer {
        id: input
        width: parent.width
        height: 44
    }

    UnifiedSearchResultDelegate {
        id: resultDelegate

        y: 60
        width: 500
        searchModel: fakeSearchModel
        providerName: "Files"
        providerId: "files"
        providerIcon: ""
        resultTitle: "README.md"
        subline: "Documents"
        resourceUrlRole: "https://cloud.example.test/f/1"
        darkIcons: ""
        lightIcons: ""
        darkIconsIsThumbnail: false
        lightIconsIsThumbnail: false
        darkImagePlaceholder: ""
        lightImagePlaceholder: ""
        isRounded: false
        resultType: UnifiedSearchResultsListModel.ProviderHeader
        isSelected: false
        isPartialMatch: false
        hasOverflow: true
        isLoading: false
    }

    WizardButton {
        id: wizardHoverButton

        x: 500
        y: 170
        width: 120
        text: qsTr("Hover")
    }

    WizardMenuItem {
        id: wizardMenuHoverItem

        x: 350
        y: 170
        width: 120
        text: qsTr("Menu hover")
        icon.source: "qrc:/client/theme/black/folder.svg"
        tintIcon: true
        iconTintColor: Style.wizardPrimaryText
    }

    SignalSpy { id: clearSpy; target: input; signalName: "clearText" }
    SignalSpy { id: moveSpy; target: input; signalName: "moveSelection" }
    SignalSpy { id: activateSpy; target: input; signalName: "activateSelection" }

    TestCase {
        name: "SearchQmlModule"
        when: windowShown

        function init() {
            resultDelegate.resultType = UnifiedSearchResultsListModel.ProviderHeader
            resultDelegate.isSelected = false
            resultDelegate.isLoading = false
            fakeSearchModel.loadedPages = 0
            fakeSearchModel.retriedPages = 0
        }

        function test_searchWindowLoadsFromPackagedModule() {
            const searchWindow = productionSearchWindow.createObject(null)
            verify(searchWindow !== null)
            compare(searchWindow.height, Style.searchWindowHeight)
            verify(searchWindow.height >= 620 + Style.unifiedSearchItemHeight)
            searchWindow.destroy()
        }

        function test_selectionBindingSurvivesViewAndModelChanges() {
            const firstModel = windowSearchModel.createObject(this, { selectedRow: 0 })
            const secondModel = windowSearchModel.createObject(this, { selectedRow: 1 })
            const searchWindow = productionSearchWindow.createObject(null, {
                searchModel: firstModel,
                visible: true
            })
            const resultsList = findChild(searchWindow, "searchResultsList")

            verify(resultsList !== null)
            tryCompare(resultsList, "count", 2)
            tryVerify(() => resultsList.itemAtIndex(0) !== null)
            verify(resultsList.itemAtIndex(0).loadedItem !== null)
            compare(resultsList.itemAtIndex(0).loadedItem.objectName, "searchResultRow")
            compare(resultsList.currentIndex, 0)

            firstModel.viewMode = UnifiedSearchResultsListModel.ProviderDetail
            wait(0)
            searchWindow.searchModel = secondModel
            tryCompare(resultsList, "currentIndex", 1)

            searchWindow.destroy()
            firstModel.destroy()
            secondModel.destroy()
        }

        function test_partialFailureFooterIsAggregateOnly() {
            const model = windowSearchModel.createObject(this, { hasPartialFailure: true })
            const searchWindow = productionSearchWindow.createObject(null, { searchModel: model })
            const footer = findChild(searchWindow, "partialFailureFooter")

            verify(footer !== null)
            compare(footer.visible, true)
            model.viewMode = UnifiedSearchResultsListModel.ProviderDetail
            tryCompare(footer, "visible", false)
            model.viewMode = UnifiedSearchResultsListModel.Aggregate
            tryCompare(footer, "visible", true)

            searchWindow.destroy()
            model.destroy()
        }

        function test_hiddenLoadingFooterDoesNotReserveSpace() {
            const model = windowSearchModel.createObject(this)
            const searchWindow = productionSearchWindow.createObject(null, {
                searchModel: model,
                visible: true
            })
            const footer = findChild(searchWindow, "searchResultsLoadingFooter")

            verify(footer !== null)
            compare(footer.visible, false)
            compare(footer.height, 0)
            model.isSearchInProgress = true
            tryVerify(() => footer.height > 0)

            searchWindow.destroy()
            model.destroy()
        }

        function test_fetchMoreShowsSearchProgressIndicator() {
            const model = windowSearchModel.createObject(this)
            const searchWindow = productionSearchWindow.createObject(null, { searchModel: model })
            const progressIndicator = findChild(searchWindow, "searchProgressIndicator")

            verify(progressIndicator !== null)
            compare(progressIndicator.visible, false)
            model.currentFetchMoreInProgressProviderId = "files"
            tryCompare(progressIndicator, "visible", true)
            compare(progressIndicator.running, true)

            searchWindow.destroy()
            model.destroy()
        }

        function test_detailHeaderCentersProviderAndShowsBackArrow() {
            const model = windowSearchModel.createObject(this, {
                viewMode: UnifiedSearchResultsListModel.ProviderDetail,
                detailProviderName: "A provider name that is intentionally much too long for the available search header"
            })
            const searchWindow = productionSearchWindow.createObject(null, {
                searchModel: model,
                visible: true
            })
            const header = findChild(searchWindow, "searchDetailHeader")
            const title = findChild(searchWindow, "searchDetailProviderTitle")
            const backButton = findChild(searchWindow, "searchDetailBackButton")

            verify(header !== null)
            verify(title !== null)
            verify(backButton !== null)
            compare(title.font.bold, true)
            verify(title.font.pixelSize > Style.unifiedSearchResultTitleFontSize)
            const titleCenter = title.mapToItem(header, title.width / 2, title.height / 2)
            fuzzyCompare(titleCenter.x, header.width / 2, 1)
            const backButtonRight = backButton.mapToItem(header, backButton.width, 0).x
            const titleLeft = title.mapToItem(header, 0, 0).x
            verify(titleLeft > backButtonRight)
            verify(title.implicitWidth > title.width)
            verify(backButton.icon.source.toString().includes("arrow-left.svg"))
            compare(backButton.icon.width, Style.smallIconSize)
            compare(backButton.icon.height, Style.smallIconSize)

            searchWindow.destroy()
            model.destroy()
        }

        function test_filterButtonsUseWizardHoverAndTrailingCarets() {
            const model = windowSearchModel.createObject(this)
            const searchWindow = productionSearchWindow.createObject(null, {
                searchModel: model,
                visible: true
            })
            const typeButton = findChild(searchWindow, "typeFilterButton")
            const dateButton = findChild(searchWindow, "dateFilterButton")
            const peopleButton = findChild(searchWindow, "peopleFilterButton")

            verify(typeButton !== null)
            verify(dateButton !== null)
            verify(peopleButton !== null)
            const todayMenuItem = findChild(searchWindow, "dateTodayMenuItem")
            verify(todayMenuItem !== null)
            compare(todayMenuItem.hoverEnabled, true)
            for (const button of [typeButton, dateButton, peopleButton]) {
                verify(button.trailingIconSource.toString().includes("caret-down.svg"))
                const caret = findChild(button, "wizardButtonTrailingIcon")
                verify(caret !== null)
                const caretPosition = caret.mapToItem(button, 0, 0)
                verify(caretPosition.x > button.width / 2)
                fuzzyCompare(caretPosition.y + caret.height / 2, button.height / 2, 1)
            }

            searchWindow.destroy()
            model.destroy()
        }

        function test_filterMenusUseStandardDropdownGeometry() {
            const model = windowSearchModel.createObject(this, { dateFilterAvailable: true })
            const searchWindow = productionSearchWindow.createObject(null, {
                searchModel: model,
                visible: true
            })
            const typeButton = findChild(searchWindow, "typeFilterButton")
            const typeMenu = findChild(searchWindow, "typeFilterMenu")
            const dateButton = findChild(searchWindow, "dateFilterButton")
            const dateMenu = findChild(searchWindow, "dateFilterMenu")

            verify(typeButton !== null)
            verify(typeMenu !== null)
            verify(dateButton !== null)
            verify(dateMenu !== null)
            compare(dateMenu.width, dateButton.width)
            verify(dateMenu.width > Style.standardPrimaryButtonHeight)
            compare(typeMenu.background.radius, Style.mediumRoundedButtonRadius)
            compare(dateMenu.background.radius, Style.mediumRoundedButtonRadius)
            compare(typeMenu.background.border.width, Style.normalBorderWidth)
            compare(dateMenu.background.border.width, Style.normalBorderWidth)

            mouseClick(typeButton)
            tryCompare(typeMenu, "opened", true)
            compare(typeMenu.y, typeButton.height + Style.smallSpacing)
            mouseClick(typeButton)
            tryCompare(typeMenu, "opened", false)
            mouseClick(dateButton)
            tryCompare(dateMenu, "opened", true)
            compare(dateMenu.y, dateButton.height + Style.smallSpacing)
            verify(dateMenu.height > Style.standardPrimaryButtonHeight)
            mouseClick(dateButton)
            tryCompare(dateMenu, "opened", false)

            searchWindow.destroy()
            model.destroy()
        }

        function test_categoryFiltersAreVisibleBeforeSearchBegins() {
            const model = windowSearchModel.createObject(this, { searchTerm: "" })
            const searchWindow = productionSearchWindow.createObject(null, {
                searchModel: model,
                visible: true
            })
            const filterFlow = findChild(searchWindow, "categoryFilterFlow")
            const typeButton = findChild(searchWindow, "typeFilterButton")
            const dateButton = findChild(searchWindow, "dateFilterButton")
            const peopleButton = findChild(searchWindow, "peopleFilterButton")

            verify(filterFlow !== null)
            compare(filterFlow.visible, true)
            compare(typeButton.visible, true)
            compare(dateButton.visible, true)
            compare(peopleButton.visible, true)

            searchWindow.destroy()
            model.destroy()
        }

        function test_activeFilterUsesCompactPillButton() {
            const model = windowSearchModel.createObject(this, {
                activeFilters: [{
                    type: "date",
                    id: "last7days",
                    label: "Last 7 days",
                    icon: ""
                }]
            })
            const searchWindow = productionSearchWindow.createObject(null, {
                searchModel: model,
                visible: true
            })
            const activeFilterFlow = findChild(searchWindow, "activeFilterFlow")
            const dateButton = findChild(searchWindow, "dateFilterButton")
            function activeFilterChip() {
                for (let index = 0; index < activeFilterFlow.children.length; ++index) {
                    if (activeFilterFlow.children[index].objectName === "activeFilterChip") {
                        return activeFilterFlow.children[index]
                    }
                }
                return null
            }

            verify(activeFilterFlow !== null)
            tryVerify(() => activeFilterChip() !== null)
            const chip = activeFilterChip()
            verify(dateButton !== null)
            compare(chip.implicitHeight, Style.wizardChipButtonHeight)
            verify(chip.implicitHeight < dateButton.implicitHeight)
            compare(chip.background.radius, Style.veryRoundedButtonRadius)
            verify(chip.leftPadding < dateButton.leftPadding)
            compare(chip.rightPadding, chip.leftPadding)

            searchWindow.destroy()
            model.destroy()
        }

    }

    TestCase {
        name: "UnifiedSearchInput"
        when: windowShown

        function init() {
            input.text = ""
            input.forceActiveFocus()
            clearSpy.clear()
            moveSpy.clear()
            activateSpy.clear()
        }

        function test_keyboardNavigationKeepsInputFocus() {
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_End)
            keyClick(Qt.Key_Return)
            compare(moveSpy.count, 2)
            compare(activateSpy.count, 1)
            verify(input.activeFocus)
        }

        function test_activeFocusKeepsNeutralFrame() {
            verify(input.activeFocus)
            compare(input.background.border.width, 1)
            compare(input.background.border.color, Style.wizardFieldBorder)
        }

        function test_textEditingSignalIsIndependentFromClear() {
            keyClick(Qt.Key_C)
            keyClick(Qt.Key_A)
            keyClick(Qt.Key_L)
            keyClick(Qt.Key_E)
            keyClick(Qt.Key_N)
            keyClick(Qt.Key_D)
            keyClick(Qt.Key_A)
            keyClick(Qt.Key_R)
            compare(input.text, "calendar")
            compare(clearSpy.count, 0)
        }

        function test_clearActionReplacesFilterAction() {
            const clearButton = findChild(input, "clearSearchButton")

            verify(clearButton !== null)
            compare(clearButton.visible, false)
            keyClick(Qt.Key_C)
            compare(clearButton.visible, true)
            verify(clearButton.icon.source.toString().includes("clear.svg"))
            mouseClick(clearButton)
            compare(clearSpy.count, 1)
        }
    }

    TestCase {
        name: "WizardButton"
        when: windowShown

        function test_hoverUsesSharedWizardSurface() {
            mouseMove(input, 1, 1)
            wait(0)
            const restingColor = wizardHoverButton.background.color.toString()
            const hoverArea = findChild(wizardHoverButton, "wizardButtonHoverArea")

            verify(hoverArea !== null)
            mouseMove(wizardHoverButton, 2, 2)
            tryCompare(hoverArea, "containsMouse", true)
            verify(wizardHoverButton.background.color.toString() !== restingColor)
        }

        function test_tintedLeadingIconUsesRequestedColor() {
            wizardHoverButton.iconBeforeText = true
            wizardHoverButton.tintIcon = true
            wizardHoverButton.iconTintColor = Style.wizardPrimaryText
            wizardHoverButton.iconSource = "qrc:/client/theme/black/folder.svg"
            wait(0)

            const iconTint = findChild(wizardHoverButton, "wizardButtonLeadingIconTint")
            verify(iconTint !== null)
            compare(iconTint.visible, true)
            compare(iconTint.color.toString(), Style.wizardPrimaryText.toString())
        }

        function test_trailingIconCannotOverlapLongText() {
            wizardHoverButton.iconBeforeText = false
            wizardHoverButton.iconSource = ""
            wizardHoverButton.trailingIconSource = "qrc:/client/theme/black/caret-down.svg"
            wizardHoverButton.text = "An intentionally long translated button label"
            wait(0)

            const textItem = findChild(wizardHoverButton, "wizardButtonText")
            const trailingIcon = findChild(wizardHoverButton, "wizardButtonTrailingIcon")
            verify(textItem !== null)
            verify(trailingIcon !== null)
            tryVerify(() => {
                const textRight = textItem.mapToItem(wizardHoverButton, textItem.width, 0).x
                const iconLeft = trailingIcon.mapToItem(wizardHoverButton, 0, 0).x
                return textRight < iconLeft
            })
        }
    }

    TestCase {
        name: "WizardMenuItem"
        when: windowShown

        function test_hoverUsesSharedWizardMenuSurface() {
            mouseMove(input, 1, 1)
            wait(0)
            const restingColor = wizardMenuHoverItem.background.color.toString()

            mouseMove(wizardMenuHoverItem, 2, 2)
            tryCompare(wizardMenuHoverItem, "hovered", true)
            verify(wizardMenuHoverItem.background.color.toString() !== restingColor)
        }

        function test_tintedIconUsesRequestedColor() {
            const iconTint = findChild(wizardMenuHoverItem, "wizardMenuItemIconTint")
            verify(iconTint !== null)
            compare(iconTint.visible, true)
            compare(iconTint.color.toString(), Style.wizardPrimaryText.toString())
        }
    }

    TestCase {
        name: "UnifiedSearchResultDelegate"
        when: windowShown

        function init() {
            resultDelegate.resultType = UnifiedSearchResultsListModel.ProviderHeader
            resultDelegate.hasOverflow = true
            resultDelegate.isSelected = false
            fakeSearchModel.openedProviderDetails = 0
            fakeSearchModel.activatedResults = 0
            fakeSearchModel.loadedPages = 0
            fakeSearchModel.retriedPages = 0
            wait(0)
        }

        function test_providerHeaderReceivesDelegateDataAndGeometry() {
            verify(resultDelegate.loadedItem !== null)
            compare(resultDelegate.loadedItem.objectName, "providerHeaderRow")
            compare(resultDelegate.loadedItem.width, resultDelegate.width)
            compare(resultDelegate.height, 44)
            compare(resultDelegate.loadedItem.text, "More from Files  →")
            compare(resultDelegate.loadedItem.font.bold, false)
            compare(resultDelegate.loadedItem.font.pixelSize, Style.unifiedSearchResultTitleFontSize)
            compare(resultDelegate.loadedItem.leftPadding, 0)
            compare(resultDelegate.loadedItem.hoverEnabled, true)
            compare(findChild(resultDelegate.loadedItem, "providerHeaderIconTint"), null)

            mouseMove(input, 1, 1)
            mouseMove(input, 2, 2)
            tryCompare(resultDelegate.loadedItem, "hovered", false)
            compare(resultDelegate.loadedItem.background.color.toString(), "#00000000")
            mouseMove(resultDelegate.loadedItem, 1, 1)
            mouseMove(resultDelegate.loadedItem, 2, 2)
            tryCompare(resultDelegate.loadedItem, "hovered", true)
            compare(resultDelegate.loadedItem.background.color.toString(), Style.listItemHoverBackground.toString())

            mousePress(resultDelegate.loadedItem)
            compare(fakeSearchModel.openedProviderDetails, 1)
            mouseRelease(resultDelegate.loadedItem)
            compare(fakeSearchModel.openedProviderDetails, 1)
        }

        function test_plainProviderHeaderStaysRegularAndNonInteractive() {
            resultDelegate.hasOverflow = false
            wait(0)

            compare(resultDelegate.loadedItem.text, "Files")
            compare(resultDelegate.loadedItem.font.bold, false)
            compare(resultDelegate.loadedItem.font.pixelSize, Style.unifiedSearchResultTitleFontSize)
            compare(resultDelegate.loadedItem.leftPadding, 0)
            compare(resultDelegate.loadedItem.hoverEnabled, false)
        }

        function test_resultRowReceivesDelegateDataAndGeometry() {
            resultDelegate.resultType = UnifiedSearchResultsListModel.Default
            wait(0)

            verify(resultDelegate.loadedItem !== null)
            compare(resultDelegate.loadedItem.objectName, "searchResultRow")
            compare(resultDelegate.loadedItem.width, resultDelegate.width)
            compare(resultDelegate.height, Style.unifiedSearchItemHeight)
            verify(resultDelegate.height <= 44)
            compare(resultDelegate.loadedItem.leftPadding, 0)
            compare(resultDelegate.loadedItem.rightPadding, 0)
            compare(resultDelegate.loadedItem.topPadding, 0)
            compare(resultDelegate.loadedItem.bottomPadding, 0)
            compare(resultDelegate.loadedItem.contentItem.height, resultDelegate.loadedItem.height)
            compare(resultDelegate.loadedItem.background.height, resultDelegate.loadedItem.height)
            compare(resultDelegate.loadedItem.contentItem.Accessible.ignored, true)

            const title = findChild(resultDelegate.loadedItem, "searchResultTitle")
            const subline = findChild(resultDelegate.loadedItem, "searchResultSubline")
            const textContainer = findChild(resultDelegate.loadedItem, "searchResultTextContainer")
            verify(title !== null)
            verify(subline !== null)
            verify(textContainer !== null)
            compare(title.font.pixelSize, Style.unifiedSearchResultTitleFontSize)
            compare(subline.font.pixelSize, title.font.pixelSize)
            compare(textContainer.spacing, Style.unifiedSearchResultTextSpacing)

            resultDelegate.isSelected = true
            tryCompare(resultDelegate.loadedItem.background, "color", Style.wizardSecondaryButtonPressed)
            resultDelegate.isSelected = false
            mouseMove(resultDelegate.loadedItem, 2, 2)
            tryCompare(resultDelegate.loadedItem, "hovered", true)
            compare(resultDelegate.loadedItem.background.color.toString(), Style.listItemHoverBackground.toString())

            mouseClick(resultDelegate.loadedItem)
            compare(fakeSearchModel.activatedResults, 1)
        }

        function test_loadMoreUsesAFlatItemRow() {
            resultDelegate.resultType = UnifiedSearchResultsListModel.FetchMoreTrigger
            wait(0)

            verify(resultDelegate.loadedItem !== null)
            compare(resultDelegate.loadedItem.objectName, "searchPagingRow")
            compare(resultDelegate.loadedItem.text, qsTr("Load more results"))
            const pagingLabel = findChild(resultDelegate.loadedItem, "searchPagingLabel")
            verify(pagingLabel !== null)
            compare(pagingLabel.font.bold, false)
            verify(resultDelegate.loadedItem.icon.source.toString().includes("more.svg"))
            verify(!resultDelegate.loadedItem.hasOwnProperty("primary"))
            compare(resultDelegate.loadedItem.height, 44)
            mouseMove(input, 1, 1)
            wait(0)
            compare(resultDelegate.loadedItem.background.color.toString(), "#00000000")

            mouseMove(resultDelegate.loadedItem, 2, 2)
            tryCompare(resultDelegate.loadedItem, "hovered", true)
            compare(resultDelegate.loadedItem.background.color.toString(), Style.listItemHoverBackground.toString())

            mousePress(resultDelegate.loadedItem)
            compare(fakeSearchModel.loadedPages, 1)
            mouseRelease(resultDelegate.loadedItem)
            compare(fakeSearchModel.loadedPages, 1)
        }

        function test_retryPagingUsesAFlatItemRow() {
            resultDelegate.resultType = UnifiedSearchResultsListModel.RetryFetchMoreTrigger
            wait(0)

            verify(resultDelegate.loadedItem !== null)
            compare(resultDelegate.loadedItem.objectName, "searchPagingRow")
            compare(resultDelegate.loadedItem.text, qsTr("Retry loading more results"))
            verify(!resultDelegate.loadedItem.hasOwnProperty("primary"))
            compare(resultDelegate.loadedItem.height, 44)

            mousePress(resultDelegate.loadedItem)
            compare(fakeSearchModel.retriedPages, 1)
            mouseRelease(resultDelegate.loadedItem)
            compare(fakeSearchModel.retriedPages, 1)
        }
    }
}
