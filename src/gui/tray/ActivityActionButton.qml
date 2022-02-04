import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.15
import Style 1.0
import com.nextcloud.desktopclient 1.0

Item {
    id: root

    property string text: ""
    property string toolTipText: ""

    property bool bold: false

    property string imageSource: ""
    property string imageSourceHover: ""

    property color textColor: Style.ncTextColor
    property color textColorHovered: Style.ncSecondaryTextColor

    signal clicked()

    Loader {
        active: root.imageSource === ""

        anchors.fill: parent

        sourceComponent: CustomTextButton {
             anchors.fill: parent
             text: root.text
             toolTipText: root.toolTipText

             textColor: root.textColor
             textColorHovered: root.textColorHovered

             onClicked: root.clicked()
        }
    }

    Loader {
        active: root.imageSource !== ""

        anchors.fill: parent

        sourceComponent: CustomButton {
            anchors.fill: parent
            anchors.topMargin: Style.roundedButtonBackgroundVerticalMargins
            anchors.bottomMargin: Style.roundedButtonBackgroundVerticalMargins

            text: root.text
            toolTipText: root.toolTipText

            textColor: root.textColor
            textColorHovered: root.textColorHovered

            bold: root.bold

            imageSource: root.imageSource
            imageSourceHover: root.imageSourceHover

            bgColor: UserModel.currentUser.headerColor

            onClicked: root.clicked()
        }
    }
}
