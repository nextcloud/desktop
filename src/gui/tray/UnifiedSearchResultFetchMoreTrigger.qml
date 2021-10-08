import QtQml 2.12
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.0

RowLayout {
    id: unifiedSearchResultItemFetchMore

    property bool isFetchMoreInProgress: false

    property bool isWihinViewPort: false

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

        BusyIndicator {
            id: unifiedSearchResultItemFetchMoreIconInProgress
            running: visible
            visible: isFetchMoreInProgress
            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
            }
            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            width: Style.trayWindowHeaderHeight * 0.75
            height: Style.trayWindowHeaderHeight * 0.75
        }
    }
}
