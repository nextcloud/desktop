import QtQml 2.12
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtGraphicalEffects 1.0
import Style 1.0

TextField {
    id: trayWindowUnifiedSearchTextField

    property bool isSearchInProgress: false

    readonly property color textFieldIconsColor: Style.menuBorder

    readonly property int textFieldIconsOffset: 10

    readonly property double textFieldIconsScaleFactor: 0.6

    readonly property int textFieldHorizontalPaddingOffset: 14

    signal clearText()

    leftPadding: trayWindowUnifiedSearchTextFieldSearchIcon.width + trayWindowUnifiedSearchTextFieldSearchIcon.anchors.leftMargin + textFieldHorizontalPaddingOffset
    rightPadding: trayWindowUnifiedSearchTextFieldClearTextButton.width + trayWindowUnifiedSearchTextFieldClearTextButton.anchors.rightMargin + textFieldHorizontalPaddingOffset

    placeholderText: qsTr("Search files, messages, events …")

    selectByMouse: true

    background: Rectangle {
        radius: 5
        border.color: parent.activeFocus ? Style.ncBlue : Style.menuBorder
        border.width: 1
    }

    Image {
        id: trayWindowUnifiedSearchTextFieldSearchIcon

        anchors {
            left: parent.left
            leftMargin: parent.textFieldIconsOffset
            verticalCenter: parent.verticalCenter
        }

        visible: !trayWindowUnifiedSearchTextField.isSearchInProgress

        smooth: true;
        antialiasing: true
        mipmap: true
        source: "image://svgimage-custom-color/search.svg" + "/" + trayWindowUnifiedSearchTextField.textFieldIconsColor
        sourceSize: Qt.size(parent.height * parent.textFieldIconsScaleFactor, parent.height * parent.textFieldIconsScaleFactor)
    }

    BusyIndicator {
        id: trayWindowUnifiedSearchTextFieldIconInProgress
        running: visible
        visible: trayWindowUnifiedSearchTextField.isSearchInProgress
        anchors {
            left: trayWindowUnifiedSearchTextField.left
            bottom: trayWindowUnifiedSearchTextField.bottom
            leftMargin: trayWindowUnifiedSearchTextField.textFieldIconsOffset - 4
            topMargin: 4
            bottomMargin: 4
            verticalCenter: trayWindowUnifiedSearchTextField.verticalCenter
        }
        width: height
    }

    Image {
        id: trayWindowUnifiedSearchTextFieldClearTextButton

        anchors {
            right: parent.right
            rightMargin: parent.textFieldIconsOffset
            verticalCenter: parent.verticalCenter
        }

        smooth: true;
        antialiasing: true
        mipmap: true

        visible: parent.text
        source: "image://svgimage-custom-color/clear.svg" + "/" + trayWindowUnifiedSearchTextField.textFieldIconsColor
        sourceSize: Qt.size(parent.height * parent.textFieldIconsScaleFactor, parent.height * parent.textFieldIconsScaleFactor)

        MouseArea {
            id: trayWindowUnifiedSearchTextFieldClearTextButtonMouseArea

            anchors.fill: parent

            onClicked: trayWindowUnifiedSearchTextField.clearText()
        }
    }
}
