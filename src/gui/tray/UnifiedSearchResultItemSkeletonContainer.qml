import QtQml 2.12
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.0

Column {
    id: unifiedSearchResultsListViewSkeletonColumn

    Repeater {
        model: 10
        UnifiedSearchResultItemSkeleton {
            width: parent.width
            height: Style.trayWindowHeaderHeight
            index: model.index
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
