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
import QtQuick.Layouts 1.15
import Qt5Compat.GraphicalEffects
import Style 1.0

RowLayout {
    id: unifiedSearchResultSkeletonItemDetails

    property int iconWidth: Style.unifiedSearchResultIconWidth
    property int titleFontSize: Style.unifiedSearchResultTitleFontSize
    property int sublineFontSize: Style.unifiedSearchResultSublineFontSize

    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("Search result skeleton.").arg(model.index)

    height: Style.trayWindowHeaderHeight
    spacing: Style.trayHorizontalMargin

    /*
    * An overview of what goes on here:
    *
    * We want to provide the user with a loading animation of a unified search result list item "skeleton".
    * This skeleton has a square and two rectangles that are meant to represent the icon and the text of a
    * real search result.
    *
    * We also want to provide a nice animation that has a dark gradient moving over these shapes. To do so,
    * we very simply create rectangles that have a gradient going transparent->dark->dark->transparent, and
    * overlay them over the shapes beneath.
    *
    * We then use NumberAnimations to move these gradients left-to-right over the base shapes below. Since
    * the gradient rectangles are child elements of the base-color rectangles which they move over, as long
    * as we ensure that the parent rectangle has the "clip" property enabled this will look nice and won't
    * spill over onto other elements.
    *
    * We also want to make sure that, even though these gradient rectangles are separate, it looks as it is
    * one single gradient sweeping over the base color components
    */

    property color baseGradientColor: palette.light
    property int animationRectangleWidth: Style.trayWindowWidth

    Item {
        property int whiteSpace: (Style.trayListItemIconSize - unifiedSearchResultSkeletonItemDetails.iconWidth)

        Layout.preferredWidth: unifiedSearchResultSkeletonItemDetails.iconWidth
        Layout.preferredHeight: unifiedSearchResultSkeletonItemDetails.iconWidth
        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
        Layout.leftMargin: Style.trayHorizontalMargin + (whiteSpace * (0.5 - Style.thumbnailImageSizeReduction))
        Layout.rightMargin: whiteSpace * (0.5 + Style.thumbnailImageSizeReduction)

        Rectangle {
            id: unifiedSearchResultSkeletonThumbnail
            anchors.fill: parent
            color: unifiedSearchResultSkeletonItemDetails.baseGradientColor
            clip: true
            visible: false

            Loader {
                x: mapFromItem(unifiedSearchResultSkeletonItemDetails, 0, 0).x
                height: parent.height
                sourceComponent: UnifiedSearchResultItemSkeletonGradientRectangle {
                    width: unifiedSearchResultSkeletonItemDetails.animationRectangleWidth
                    height: parent.height
                }
            }
        }

        Rectangle {
            id: unifiedSearchResultSkeletonThumbnailMask
            anchors.fill: unifiedSearchResultSkeletonThumbnail
            color: "white"
            radius: 100
            visible: false
        }

        OpacityMask {
            anchors.fill: unifiedSearchResultSkeletonThumbnail
            source: unifiedSearchResultSkeletonThumbnail
            maskSource: unifiedSearchResultSkeletonThumbnailMask
        }
    }

    Column {
        id: unifiedSearchResultSkeletonTextContainer

        Layout.fillWidth: true
        Layout.rightMargin: Style.trayHorizontalMargin
        spacing: Style.standardSpacing

        Item {
            height: unifiedSearchResultSkeletonItemDetails.titleFontSize
            width: parent.width

            Rectangle {
                id: unifiedSearchResultSkeletonTitleText
                anchors.fill: parent
                color: unifiedSearchResultSkeletonItemDetails.baseGradientColor
                clip: true
                visible: false

                Loader {
                    x: mapFromItem(unifiedSearchResultSkeletonItemDetails, 0, 0).x
                    height: parent.height
                    sourceComponent: UnifiedSearchResultItemSkeletonGradientRectangle {
                        width: unifiedSearchResultSkeletonItemDetails.animationRectangleWidth
                        height: parent.height
                    }
                }
            }

            Rectangle {
                id: unifiedSearchResultSkeletonTitleTextMask
                anchors.fill: unifiedSearchResultSkeletonTitleText
                color: "white"
                radius: Style.veryRoundedButtonRadius
                visible: false
            }

            OpacityMask {
                anchors.fill: unifiedSearchResultSkeletonTitleText
                source: unifiedSearchResultSkeletonTitleText
                maskSource: unifiedSearchResultSkeletonTitleTextMask
            }
        }

        Item {
            height: unifiedSearchResultSkeletonItemDetails.sublineFontSize
            width: parent.width

            Rectangle {
                id: unifiedSearchResultSkeletonTextSubline
                anchors.fill: parent
                color: unifiedSearchResultSkeletonItemDetails.baseGradientColor
                clip: true
                visible: false

                Loader {
                    x: mapFromItem(unifiedSearchResultSkeletonItemDetails, 0, 0).x
                    height: parent.height
                    sourceComponent: UnifiedSearchResultItemSkeletonGradientRectangle {
                        width: unifiedSearchResultSkeletonItemDetails.animationRectangleWidth
                        height: parent.height
                    }
                }
            }

            Rectangle {
                id: unifiedSearchResultSkeletonTextSublineMask
                anchors.fill: unifiedSearchResultSkeletonTextSubline
                color: "white"
                radius: Style.veryRoundedButtonRadius
                visible: false
            }

            OpacityMask {
                anchors.fill:unifiedSearchResultSkeletonTextSubline
                source: unifiedSearchResultSkeletonTextSubline
                maskSource: unifiedSearchResultSkeletonTextSublineMask
            }
        }
    }
}
