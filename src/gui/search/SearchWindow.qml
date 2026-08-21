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

WizardStyledWindow {
    id: root

    property var account: null
    property var searchModel: null
    readonly property string headline: qsTr("Search")
    readonly property int searchState: searchModel
        ? searchModel.searchState
        : UnifiedSearchResultsListModel.Placeholder
    readonly property bool isSearchInProgress: searchModel !== null && searchModel.isSearchInProgress
    readonly property bool canEditSearch: searchModel !== null && searchModel.canEditSearch
    readonly property bool isAccountConnected: searchModel !== null && searchModel.isAccountConnected

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
            user: root.account
        }

        UnifiedSearchInputContainer {
            id: searchInput

            Layout.fillWidth: true
            Layout.preferredHeight: Style.unifiedSearchInputContainerHeight
            enabled: root.searchModel !== null
            readOnly: !root.canEditSearch
            text: root.searchModel ? root.searchModel.searchTerm : ""
            placeholderText: root.account !== null && !root.isAccountConnected
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
                visible: root.searchState === UnifiedSearchResultsListModel.SearchError
                text: root.searchModel ? root.searchModel.errorString : ""
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

            Loader {
                anchors.fill: parent
                anchors.margins: Style.smallSpacing
                active: root.searchState === UnifiedSearchResultsListModel.Skeleton
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
                visible: root.searchState === UnifiedSearchResultsListModel.Results

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
