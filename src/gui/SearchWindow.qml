/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import Style
import "./tray"

WizardStyledWindow {
    id: root

    property int userIndex: -1
    property var currentUser: null
    property var searchModel: null
    readonly property string headline: qsTr("Search")
    readonly property bool isFetchMoreInProgress: searchModel !== null
        && searchModel.currentFetchMoreInProgressProviderId.length > 0
    readonly property bool isSearchInProgress: searchModel !== null
        && searchModel.isSearchInProgress
    readonly property bool waitingForSearchTermEditEnd: searchModel !== null
        && searchModel.waitingForSearchTermEditEnd
    readonly property bool hasSearchTerm: searchModel !== null
        && searchModel.searchTerm.length > 0
    readonly property bool hasSearchError: searchModel !== null
        && searchModel.errorString.length > 0
    readonly property bool canEditSearch: currentUser !== null
        && currentUser.isConnected
        && searchModel !== null
        && !isFetchMoreInProgress
    readonly property bool showResults: searchModel !== null
        && searchResultsListView.count > 0
    readonly property bool showNothingFound: hasSearchTerm
        && !isSearchInProgress
        && !waitingForSearchTermEditEnd
        && !hasSearchError
        && searchResultsListView.count === 0
    readonly property bool showPlaceholder: !hasSearchTerm
        && !hasSearchError
    readonly property bool showSearchError: hasSearchError
        && !showResults
        && !isSearchInProgress
        && !isFetchMoreInProgress
    readonly property bool showSkeleton: hasSearchTerm
        && !showNothingFound
        && !showResults
        && !hasSearchError

    title: ""
    width: Style.searchWindowWidth
    height: Style.searchWindowHeight
    minimumWidth: Style.wizardStandaloneWindowMinimumWidth
    minimumHeight: Style.wizardStandaloneWindowMinimumHeight

    function focusSearchInput() {
        if (visible && searchInput.enabled) {
            searchInput.forceActiveFocus()
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.close()
    }

    Component.onCompleted: Qt.callLater(focusSearchInput)
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(focusSearchInput)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Style.wizardWindowMargin
        anchors.rightMargin: Style.wizardWindowMargin
        anchors.topMargin: Style.wizardWindowTopMargin
        anchors.bottomMargin: Style.wizardWindowMargin
        spacing: Style.wizardSectionSpacing

        WindowAccountHeader {
            Layout.fillWidth: true
            title: root.headline
            user: root.currentUser
        }

        UnifiedSearchInputContainer {
            id: searchInput

            Layout.fillWidth: true
            Layout.preferredHeight: Style.unifiedSearchInputContainerHeight
            enabled: root.searchModel !== null
            readOnly: !root.canEditSearch
            text: root.searchModel ? root.searchModel.searchTerm : ""
            placeholderText: root.currentUser !== null && !root.currentUser.isConnected
                ? qsTr("Search is available when this account is connected")
                : qsTr("Search files, messages, events …")
            isSearchInProgress: root.isSearchInProgress
            onTextEdited: {
                if (root.searchModel) {
                    root.searchModel.searchTerm = searchInput.text
                }
            }
            onClearText: {
                if (root.searchModel) {
                    root.searchModel.searchTerm = ""
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

            ErrorBox {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                visible: root.showSearchError
                text: root.searchModel ? root.searchModel.errorString : ""
            }

            UnifiedSearchPlaceholderView {
                anchors.fill: parent
                visible: root.showPlaceholder
            }

            UnifiedSearchResultNothingFound {
                anchors.fill: parent
                visible: root.showNothingFound
                text: root.searchModel ? root.searchModel.searchTerm : ""
            }

            Loader {
                anchors.fill: parent
                anchors.margins: Style.smallSpacing
                active: root.showSkeleton
                asynchronous: true

                sourceComponent: UnifiedSearchResultItemSkeletonContainer {
                    anchors.fill: parent
                    spacing: searchResultsListView.spacing
                    animationRectangleWidth: root.width
                }
            }

            ScrollView {
                id: searchResultsScrollView

                anchors.fill: parent
                contentWidth: availableWidth
                visible: root.showResults

                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ListView {
                    id: searchResultsListView

                    spacing: Style.smallSpacing
                    clip: true
                    keyNavigationEnabled: true
                    reuseItems: true
                    model: root.searchModel

                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Search results list")

                    delegate: UnifiedSearchResultListItem {
                        width: searchResultsListView.width
                        isSearchInProgress: root.isSearchInProgress
                        currentFetchMoreInProgressProviderId: root.searchModel
                            ? root.searchModel.currentFetchMoreInProgressProviderId
                            : ""
                        fetchMoreTriggerClicked: root.searchModel
                            ? root.searchModel.fetchMoreTriggerClicked
                            : function() {}
                        resultClicked: root.searchModel
                            ? root.searchModel.resultClicked
                            : function() {}
                        ListView.onPooled: isPooled = true
                        ListView.onReused: isPooled = false
                    }

                    section.property: "providerName"
                    section.criteria: ViewSection.FullString
                    section.delegate: UnifiedSearchResultSectionItem {
                        width: searchResultsListView.width
                    }
                }
            }
        }
    }
}
