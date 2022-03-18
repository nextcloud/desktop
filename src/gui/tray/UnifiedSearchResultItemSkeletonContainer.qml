import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import Style 1.0

Column {
    id: unifiedSearchResultsListViewSkeletonColumn

    property int textLeftMargin: 18
    property int textRightMargin: 16
    property int iconWidth: 24
    property int iconLeftMargin: 12
    property int itemHeight: Style.trayWindowHeaderHeight
    property int titleFontSize: Style.topLinePixelSize
    property int sublineFontSize: Style.subLinePixelSize
    property color titleColor: Style.ncTextColor
    property color sublineColor: Style.ncSecondaryTextColor
    property color iconColor: "#afafaf"

    Repeater {
        model: 10
        UnifiedSearchResultItemSkeleton {
            textLeftMargin: unifiedSearchResultsListViewSkeletonColumn.textLeftMargin
            textRightMargin: unifiedSearchResultsListViewSkeletonColumn.textRightMargin
            iconWidth: unifiedSearchResultsListViewSkeletonColumn.iconWidth
            iconLeftMargin: unifiedSearchResultsListViewSkeletonColumn.iconLeftMargin
            width: unifiedSearchResultsListViewSkeletonColumn.width
            height: unifiedSearchResultsListViewSkeletonColumn.itemHeight
            index: model.index
            titleFontSize: unifiedSearchResultsListViewSkeletonColumn.titleFontSize
            sublineFontSize: unifiedSearchResultsListViewSkeletonColumn.sublineFontSize
            titleColor: unifiedSearchResultsListViewSkeletonColumn.titleColor
            sublineColor: unifiedSearchResultsListViewSkeletonColumn.sublineColor
            iconColor: unifiedSearchResultsListViewSkeletonColumn.iconColor
        }
    }

    OpacityAnimator {
        target: unifiedSearchResultsListViewSkeletonColumn;
        from: 0.5;
        to: 1;
        duration: 800
        running: unifiedSearchResultsListViewSkeletonColumn.visible
        loops: Animation.Infinite;
        easing {
            type: Easing.InOutBounce;
        }
    }
}
