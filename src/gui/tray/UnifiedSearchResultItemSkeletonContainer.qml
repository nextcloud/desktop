/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Style

ColumnLayout {
    id: unifiedSearchResultsListViewSkeletonColumn

    property int animationRectangleWidth: Style.trayWindowWidth

    // Single shared animation driver for all child skeleton items.
    property real sharedAnimationX: -animationRectangleWidth

    NumberAnimation on sharedAnimationX {
        from: -unifiedSearchResultsListViewSkeletonColumn.animationRectangleWidth
        to: unifiedSearchResultsListViewSkeletonColumn.animationRectangleWidth
        duration: 1000
        loops: Animation.Infinite
        running: true
    }

    Item {
        id: placeholderSectionHeader

        property rect textRect: fontMetrics.boundingRect("Dummy text")

        Layout.topMargin: Style.unifiedSearchResultSectionItemVerticalPadding / 2
        Layout.bottomMargin: Style.unifiedSearchResultSectionItemVerticalPadding / 2
        Layout.leftMargin: Style.unifiedSearchResultSectionItemLeftPadding

        width: textRect.width
        height: textRect.height

        FontMetrics {
            id: fontMetrics
            font.pixelSize: Style.unifiedSearchResultTitleFontSize
        }

        Rectangle {
            id: placeholderSectionHeaderRectangle
            anchors.fill: parent
            radius: Style.veryRoundedButtonRadius
            color: palette.light
            clip: true
            visible: false

            Loader {
                x: mapFromItem(placeholderSectionHeader, 0, 0).x
                height: parent.height
                sourceComponent: UnifiedSearchResultItemSkeletonGradientRectangle {
                    width: unifiedSearchResultsListViewSkeletonColumn.animationRectangleWidth
                    height: parent.height
                    sharedAnimationX: unifiedSearchResultsListViewSkeletonColumn.sharedAnimationX
                }
            }
        }

        Rectangle {
            id: placeholderSectionHeaderMask
            anchors.fill: placeholderSectionHeaderRectangle
            color: "white"
            radius: Style.veryRoundedButtonRadius
            visible: false
        }

        OpacityMask {
            anchors.fill: placeholderSectionHeaderRectangle
            source: placeholderSectionHeaderRectangle
            maskSource: placeholderSectionHeaderMask
        }
    }

    Repeater {
        model: Math.ceil(unifiedSearchResultsListViewSkeletonColumn.height / Style.trayWindowHeaderHeight)
        UnifiedSearchResultItemSkeleton {
            width: unifiedSearchResultsListViewSkeletonColumn.width
            height: Style.trayWindowHeaderHeight
            animationRectangleWidth: unifiedSearchResultsListViewSkeletonColumn.animationRectangleWidth
            sharedAnimationX: unifiedSearchResultsListViewSkeletonColumn.sharedAnimationX
        }
    }
}
