import QtQml 2.12
import QtQuick 2.9
import QtQuick.Layouts 1.2
import Style 1.0

RowLayout {
    id: unifiedSearchResultSectionItem
    required property string section

    Accessible.role: Accessible.Separator
    Accessible.name: qsTr("Search results section %1").arg(section)

    Column {
        Layout.topMargin: 8
        Layout.bottomMargin: 8
        Layout.leftMargin: 16
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

        Text {
            text: unifiedSearchResultSectionItem.section
            visible: parent.visible
            font.pixelSize: Style.topLinePixelSize
            color: Style.ncBlue
        }
    }
}
