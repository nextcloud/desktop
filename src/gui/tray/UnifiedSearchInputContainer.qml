import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtGraphicalEffects 1.0
import Style 1.0

import com.nextcloud.desktopclient 1.0

TextField {
    id: root

    signal clearText()

    property bool isSearchInProgress: false

    readonly property color textFieldIconsColor: Style.menuBorder

    readonly property int textFieldIconsPadding: 4
    readonly property int textFieldIconsLeftOffset: Style.trayHorizontalMargin + leftInset
    readonly property int textFieldIconsRightOffset: Style.trayHorizontalMargin + rightInset
    readonly property int textFieldIconsTopOffset: topInset
    readonly property int textFieldIconsBottomOffset: bottomInset

    readonly property double textFieldIconsScaleFactor: 0.6

    readonly property int textFieldTextLeftOffset: Style.trayHorizontalMargin + leftInset
    readonly property int textFieldTextRightOffset: Style.trayHorizontalMargin + rightInset

    topPadding: topInset
    bottomPadding: bottomInset
    leftPadding: trayWindowUnifiedSearchTextFieldSearchIcon.width +
                 trayWindowUnifiedSearchTextFieldSearchIcon.anchors.leftMargin +
                 textFieldTextLeftOffset - 1
    rightPadding: trayWindowUnifiedSearchTextFieldClearTextButton.width +
                  trayWindowUnifiedSearchTextFieldClearTextButton.anchors.rightMargin +
                  textFieldTextRightOffset
    verticalAlignment: Qt.AlignVCenter

    placeholderText: qsTr("Search files, messages, events …")

    selectByMouse: true

    palette.text: Style.ncSecondaryTextColor

    background: Rectangle {
        radius: 5
        border.color: root.activeFocus ? UserModel.currentUser.accentColor : Style.menuBorder
        border.width: 1
        color: Style.backgroundColor
    }

    Image {
        id: trayWindowUnifiedSearchTextFieldSearchIcon

        anchors {
            left: root.left
            leftMargin: root.textFieldIconsLeftOffset
            top: root.top
            topMargin: root.textFieldIconsTopOffset + root.textFieldIconsPadding
            bottom: root.bottom
            bottomMargin: root.textFieldIconsBottomOffset + root.textFieldIconsPadding
        }

        width: Style.trayListItemIconSize - anchors.leftMargin
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignLeft

        smooth: true;
        antialiasing: true
        mipmap: true
        source: "image://svgimage-custom-color/search.svg" + "/" + root.textFieldIconsColor
        sourceSize: Qt.size(root.height * root.textFieldIconsScaleFactor, root.height * root.textFieldIconsScaleFactor)

        visible: !root.isSearchInProgress
    }

    NCBusyIndicator {
        id: busyIndicator

        anchors {
            top: root.top
            topMargin: root.textFieldIconsTopOffset + root.textFieldIconsPadding
            bottom: root.bottom
            bottomMargin: root.textFieldIconsBottomOffset + root.textFieldIconsPadding
            left: root.left
            leftMargin: root.textFieldIconsLeftOffset
        }

        width: height
        color: root.textFieldIconsColor
        visible: root.isSearchInProgress
        running: visible
    }

    Image {
        id: trayWindowUnifiedSearchTextFieldClearTextButton

        anchors {
            top: root.top
            topMargin: root.textFieldIconsTopOffset + root.textFieldIconsPadding
            bottom: root.bottom
            bottomMargin: root.textFieldIconsBottomOffset + root.textFieldIconsPadding
            right: root.right
            rightMargin: root.textFieldIconsRightOffset
        }

        fillMode: Image.PreserveAspectFit
        smooth: true
        antialiasing: true
        mipmap: true

        visible: root.text
        source: "image://svgimage-custom-color/clear.svg" + "/" + root.textFieldIconsColor
        sourceSize: Qt.size(root.height * root.textFieldIconsScaleFactor, root.height * root.textFieldIconsScaleFactor)

        MouseArea {
            id: trayWindowUnifiedSearchTextFieldClearTextButtonMouseArea
            anchors.fill: parent
            onClicked: root.clearText()
        }
    }
}
