import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0

ColumnLayout {
    id: unifiedSearchResultNothingFoundContainer

    required property string searchTerm
    required property bool isSearchRunning
    required property bool isSearchResultsEmpty

    spacing: 8
    anchors.leftMargin: 10
    anchors.rightMargin: 10

    Column {
        Layout.fillWidth: true
        Layout.preferredHeight: Style.trayWindowHeaderHeight / 2

        Image {
            id: unifiedSearchResultsNoResultsLabelIcon
            source: "qrc:///client/theme/magnifying-glass.svg"
            sourceSize.width: Style.trayWindowHeaderHeight / 2
            sourceSize.height: Style.trayWindowHeaderHeight / 2
            Layout.alignment: Qt.AlignCenter
            anchors.centerIn: parent
        }
    }
    Column {
        Layout.fillWidth: true
        Layout.preferredHeight: Style.trayWindowHeaderHeight / 2

        Label {
            id: unifiedSearchResultsNoResultsLabel
            text: qsTr("No results for")
            color: Style.menuBorder
            font.pixelSize: Style.subLinePixelSize * 1.25
            wrapMode: Text.Wrap
            anchors.left: parent.left
            anchors.right: parent.right
            horizontalAlignment: Text.AlignHCenter
        }
    }
    Column {
        Layout.fillWidth: true
        Layout.preferredHeight: Style.trayWindowHeaderHeight / 2

        Label {
            id: unifiedSearchResultsNoResultsLabelDetails
            text: unifiedSearchResultNothingFoundContainer.searchTerm
            color: "black"
            font.pixelSize: Style.topLinePixelSize * 1.25
            wrapMode: Text.Wrap
            anchors.left: parent.left
            anchors.right: parent.right
            maximumLineCount: 2
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }
    }

    onIsSearchRunningChanged: {
        if (isSearchRunning) {
            unifiedSearchResultNothingFoundContainer.visible = false;
        } else {
            if (searchTerm && isSearchResultsEmpty) {
                unifiedSearchResultNothingFoundContainer.visible = true;
            }
        }
    }

    onSearchTermChanged: {
        unifiedSearchResultNothingFoundContainer.visible = false;
    }
}
