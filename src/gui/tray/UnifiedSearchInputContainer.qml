import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtGraphicalEffects 1.0
import Style 1.0

Item {
    id: trayWindowUnifiedSearchContainer

    property string text: ""
    property bool readOnly: false
    property var onTextEdited: function(){}
    property bool isSearchInProgress: false

    TextField {
        id: trayWindowUnifiedSearchTextField

        text: trayWindowUnifiedSearchContainer.text

        readOnly: trayWindowUnifiedSearchContainer.readOnly

        readonly property color textFieldIconsColor: Style.menuBorder

        readonly property int textFieldIconsOffset: 10

        readonly property double textFieldIconsScaleFactor: 0.6

        readonly property int textFieldHorizontalPaddingOffset: 14

        anchors.fill: parent

        leftPadding: trayWindowUnifiedSearchTextFieldSearchIcon.width + trayWindowUnifiedSearchTextFieldSearchIcon.anchors.leftMargin + textFieldHorizontalPaddingOffset
        rightPadding: trayWindowUnifiedSearchTextFieldClearTextButton.width + trayWindowUnifiedSearchTextFieldClearTextButton.anchors.rightMargin + textFieldHorizontalPaddingOffset

        placeholderText: qsTr("Search files, messages, events...")

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

            visible: !trayWindowUnifiedSearchContainer.isSearchInProgress

            smooth: true;
            antialiasing: true
            mipmap: true

            source: "qrc:///client/theme/black/search.svg"
            sourceSize: Qt.size(parent.height * parent.textFieldIconsScaleFactor, parent.height * parent.textFieldIconsScaleFactor)

            ColorOverlay {
                anchors.fill: parent
                source: parent
				cached: true
                color: parent.parent.textFieldIconsColor
            }
        }

        BusyIndicator {
            id: trayWindowUnifiedSearchTextFieldIconInProgress
            running: visible
            visible: trayWindowUnifiedSearchContainer.isSearchInProgress
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

            source: "qrc:///client/theme/black/clear.svg"
            sourceSize: Qt.size(parent.height * parent.textFieldIconsScaleFactor, parent.height * parent.textFieldIconsScaleFactor)

            ColorOverlay {
                anchors.fill: parent
				cached: true
                source: parent
                color: parent.parent.textFieldIconsColor
            }

            MouseArea {
                id: trayWindowUnifiedSearchTextFieldClearTextButtonMouseArea

                anchors.fill: parent

                onClicked: trayWindowUnifiedSearchContainer.onTextEdited("")
            }
        }

        RotationAnimator {
            target: trayWindowUnifiedSearchTextFieldIconInProgress
            running: trayWindowUnifiedSearchTextFieldIconInProgress.visible
            from: 0
            to: 360
            loops: Animation.Infinite
            duration: 1250
        }
        onTextEdited: trayWindowUnifiedSearchContainer.onTextEdited(text)
    }
}
