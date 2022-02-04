import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0

ColumnLayout {
    id: unifiedSearchResultItemFetchMore

    property bool isFetchMoreInProgress: false

    property bool isWihinViewPort: false

    property int fontSize: Style.topLinePixelSize

    property string textColor: Style.ncSecondaryTextColor

    Accessible.role: Accessible.ListItem
    Accessible.name: unifiedSearchResultItemFetchMoreText.text
    Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

    Label {
        id: unifiedSearchResultItemFetchMoreText
        text: qsTr("Load more results")
        visible: !unifiedSearchResultItemFetchMore.isFetchMoreInProgress
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Layout.fillWidth: true
        Layout.fillHeight: true
        font.pixelSize: unifiedSearchResultItemFetchMore.fontSize
        color: unifiedSearchResultItemFetchMore.textColor
    }

    BusyIndicator {
        id: unifiedSearchResultItemFetchMoreIconInProgress
        running: visible
        visible: unifiedSearchResultItemFetchMore.isFetchMoreInProgress && unifiedSearchResultItemFetchMore.isWihinViewPort
        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
        Layout.preferredWidth: parent.height * 0.70
        Layout.preferredHeight: parent.height * 0.70
    }
}
