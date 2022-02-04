import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0

Button {
    id: root

    property string imageSource: ""
    property string imageSourceHover: ""

    property string toolTipText: ""

    property color textColor
    property color textColorHovered

    property color bgColor: "transparent"

    property bool bold: false

    background: Rectangle {
        color: root.bgColor
        opacity: parent.hovered ? 1.0 : 0.3
        radius: width / 2
    }

    leftPadding: root.text === "" ? 5 : 10
    rightPadding: root.text === "" ? 5 : 10

    ToolTip {
        id: customButtonTooltip
        text: root.toolTipText
        delay: Qt.styleHints.mousePressAndHoldInterval
        visible: root.toolTipText !== "" && root.hovered
        contentItem: Label {
            text: customButtonTooltip.text
            color: Style.ncTextColor
        }
        background: Rectangle {
            border.color: Style.menuBorder
            color: Style.backgroundColor
        }
    }

    contentItem: RowLayout {
        Image {
            id: icon

            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

            source: root.hovered ? root.imageSourceHover : root.imageSource
        }

        Label {
            Layout.maximumWidth: icon.width > 0 ? parent.width - icon.width - parent.spacing : parent.width
            Layout.fillWidth: icon.status !== Image.Ready

            text: root.text
            font.bold: root.bold

            visible: root.text !== ""

            color: root.hovered ? root.textColorHovered : root.textColor

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            elide: Text.ElideRight
        }
    }
}
