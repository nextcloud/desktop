/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

import Style
import "qrc:/qml/src/gui/wizard/qml"

Flow {
    id: root

    required property var searchModel

    readonly property bool opened: typeMenu.opened || dateMenu.opened

    signal customDateRangeRequested()
    signal peopleRequested()

    objectName: "categoryFilterFlow"
    spacing: Style.smallSpacing

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

    WizardButton {
        id: typeFilterButton

        objectName: "typeFilterButton"
        width: Math.max(Style.unifiedSearchFilterButtonMinimumWidth,
                        (root.width - 2 * root.spacing) / 3)
        text: qsTr("Type")
        trailingIconSource: "image://svgimage-custom-color/caret-down.svg/"
            + (primary ? Style.wizardSelectedText : Style.wizardPrimaryText)
        iconBeforeText: true
        iconSource: "image://svgimage-custom-color/folder.svg/"
            + (primary ? Style.wizardSelectedText : Style.wizardPrimaryText)
        primary: root.hasActiveFilter("provider")
        Accessible.name: qsTr("Filter by type")
        onClicked: typeMenu.toggle()

        WizardMenu {
            id: typeMenu

            objectName: "typeFilterMenu"
            anchorItem: typeFilterButton
            width: anchorItem.width * Style.unifiedSearchFilterMenuWidthFactor

            Binding on height {
                when: root.searchModel
                    && root.searchModel.providers.length
                        > Style.unifiedSearchProviderMenuMaximumVisibleRows
                value: Style.unifiedSearchProviderMenuMaximumVisibleRows
                    * Style.standardPrimaryButtonHeight
                    + typeMenu.topPadding + typeMenu.bottomPadding
            }

            Repeater {
                model: root.searchModel ? root.searchModel.providers : []

                delegate: WizardMenuItem {
                    id: providerMenuItem

                    required property var modelData

                    text: (providerMenuItem.modelData.selected ? "✓ " : "") + providerMenuItem.modelData.name
                    icon.source: providerMenuItem.modelData.icon
                        ? "image://tray-image-provider/" + providerMenuItem.modelData.icon
                        : ""
                    tintIcon: true
                    iconTintColor: Style.wizardPrimaryText
                    onTriggered: root.searchModel.toggleProviderFilter(providerMenuItem.modelData.id)
                }
            }
        }
    }

    WizardButton {
        id: dateFilterButton

        objectName: "dateFilterButton"
        width: Math.max(Style.unifiedSearchFilterButtonMinimumWidth,
                        (root.width - 2 * root.spacing) / 3)
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
        onClicked: dateMenu.toggle()

        WizardMenu {
            id: dateMenu

            objectName: "dateFilterMenu"
            anchorItem: dateFilterButton

            WizardMenuItem {
                objectName: "dateTodayMenuItem"
                text: qsTr("Today")
                onTriggered: root.searchModel.setDatePreset("today")
            }
            WizardMenuItem {
                text: qsTr("Last 7 days")
                onTriggered: root.searchModel.setDatePreset("last7days")
            }
            WizardMenuItem {
                text: qsTr("Last 30 days")
                onTriggered: root.searchModel.setDatePreset("last30days")
            }
            WizardMenuItem {
                text: qsTr("This year")
                onTriggered: root.searchModel.setDatePreset("thisyear")
            }
            WizardMenuItem {
                text: qsTr("Last year")
                onTriggered: root.searchModel.setDatePreset("lastyear")
            }
            MenuSeparator {}
            WizardMenuItem {
                text: qsTr("Custom range …")
                onTriggered: root.customDateRangeRequested()
            }
            WizardMenuItem {
                text: qsTr("Clear date")
                onTriggered: root.searchModel.clearDateFilter()
            }
        }
    }

    WizardButton {
        id: peopleButton

        objectName: "peopleFilterButton"
        width: Math.max(Style.unifiedSearchFilterButtonMinimumWidth,
                        (root.width - 2 * root.spacing) / 3)
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
        onClicked: root.peopleRequested()
    }
}
