import QtQml 2.15
import QtQuick 2.15
import Style 1.0

Column {
    id: unifiedSearchResultsListViewSkeletonColumn

    property int animationRectangleWidth: Style.trayWindowWidth

    Repeater {
        model: Math.ceil(unifiedSearchResultsListViewSkeletonColumn.height / Style.trayWindowHeaderHeight)
        UnifiedSearchResultItemSkeleton {
            width: unifiedSearchResultsListViewSkeletonColumn.width
            height: Style.trayWindowHeaderHeight
            animationRectangleWidth: unifiedSearchResultsListViewSkeletonColumn.animationRectangleWidth
        }
    }
}
