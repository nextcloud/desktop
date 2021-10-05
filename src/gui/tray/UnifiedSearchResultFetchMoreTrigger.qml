import QtQml 2.12
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.0

RowLayout {
    id: unifiedSearchResultItemFetchMore

    property bool isFetchMoreInProgress: false

    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("Load more results")
    Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

    Column {
        id: unifiedSearchResultItemFetchMoreColumn
        Layout.fillWidth: true
        Layout.fillHeight: true

        Text {
            id: unifiedSearchResultItemFetchMoreText
            text: qsTr("Load more results")
            visible: !isFetchMoreInProgress
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            width: parent.width
            height: parent.height
            font.pixelSize: Style.topLinePixelSize
            color: "grey"
        }
        Image {
            id: unifiedSearchResultItemFetchMoreIconInProgress
            visible: isFetchMoreInProgress
            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
            }
            cache: true
            source: "qrc:///client/theme/change.svg"
            sourceSize.height: Style.trayWindowHeaderHeight / 2
            sourceSize.width: Style.trayWindowHeaderHeight / 2
            Layout.preferredWidth: Style.trayWindowHeaderHeight / 2
            Layout.preferredHeight: Style.trayWindowHeaderHeight / 2

            ColorOverlay {
                anchors.fill: parent
                source: parent
                color: Style.menuBorder
            }
        }

        RotationAnimator {
            target: unifiedSearchResultItemFetchMoreIconInProgress
            running: unifiedSearchResultItemFetchMoreIconInProgress.visible
            from: 0
            to: 360
            loops: Animation.Infinite
            duration: 1250
        }
    }
}
