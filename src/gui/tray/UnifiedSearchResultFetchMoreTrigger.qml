import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0

ColumnLayout {
    id: unifiedSearchResultItemFetchMore

    property bool isFetchMoreInProgress: false
    property bool isWithinViewPort: false

    property int fontSize: Style.unifiedSearchResultTitleFontSize

    property string textColor: Style.ncSecondaryTextColor

    Accessible.role: Accessible.ListItem
    Accessible.name: unifiedSearchResultItemFetchMoreText.text
    Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

    Label {
        id: unifiedSearchResultItemFetchMoreText

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Style.trayHorizontalMargin
        Layout.rightMargin: Style.trayHorizontalMargin

        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: qsTr("Load more results")
        wrapMode: Text.Wrap
        font.pixelSize: unifiedSearchResultItemFetchMore.fontSize
        color: unifiedSearchResultItemFetchMore.textColor
        visible: !unifiedSearchResultItemFetchMore.isFetchMoreInProgress
    }

    BusyIndicator {
        id: unifiedSearchResultItemFetchMoreIconInProgress

        Layout.preferredWidth: parent.height * 0.70
        Layout.preferredHeight: parent.height * 0.70
        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter

        running: visible
        visible: unifiedSearchResultItemFetchMore.isFetchMoreInProgress && unifiedSearchResultItemFetchMore.isWithinViewPort
    }
}
