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

import QtQml
import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Style

ColumnLayout {
    id: unifiedSearchResultsListViewSkeletonColumn

    property int animationRectangleWidth: Style.trayWindowWidth

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
        }
    }
}
