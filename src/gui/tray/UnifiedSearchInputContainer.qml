import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtGraphicalEffects 1.0
import Style 1.0

import com.nextcloud.desktopclient 1.0

TextField {
    id: trayWindowUnifiedSearchTextField

    property bool isSearchInProgress: false

    readonly property color textFieldIconsColor: Style.menuBorder

    readonly property int textFieldIconsOffset: Style.trayHorizontalMargin

    readonly property double textFieldIconsScaleFactor: 0.6

    readonly property int textFieldHorizontalPaddingOffset: Style.trayHorizontalMargin

    signal clearText()

    leftPadding: trayWindowUnifiedSearchTextFieldSearchIcon.width + trayWindowUnifiedSearchTextFieldSearchIcon.anchors.leftMargin + textFieldHorizontalPaddingOffset - 1
    rightPadding: trayWindowUnifiedSearchTextFieldClearTextButton.width + trayWindowUnifiedSearchTextFieldClearTextButton.anchors.rightMargin + textFieldHorizontalPaddingOffset

    placeholderText: qsTr("Search files, messages, events …")

    selectByMouse: true

    palette.text: Style.ncSecondaryTextColor

    background: Rectangle {
        radius: 5
        border.color: parent.activeFocus ? UserModel.currentUser.accentColor : Style.menuBorder
        border.width: 1
        color: Style.backgroundColor
    }

    Image {
        id: trayWindowUnifiedSearchTextFieldSearchIcon
        width: Style.trayListItemIconSize - anchors.leftMargin
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignLeft

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
