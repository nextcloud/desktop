import QtQml 2.15
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

import Style 1.0

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
            color: Style.lightHover
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
