import QtQml 2.15
import QtQuick 2.15
import Style 1.0

Column {
    id: unifiedSearchResultsListViewSkeletonColumn

    Repeater {
        model: 10
        UnifiedSearchResultItemSkeleton {
            width: unifiedSearchResultsListViewSkeletonColumn.width
        }
    }

    OpacityAnimator {
        target: unifiedSearchResultsListViewSkeletonColumn
        from: 0.5
        to: 1
        duration: 800
        running: unifiedSearchResultsListViewSkeletonColumn.visible
        loops: Animation.Infinite
        easing {
            type: Easing.InOutBounce
        }
    }
}
