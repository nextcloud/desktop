import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0

Button {
    id: root

    property string imageSource: ""
    property string imageSourceHover: ""
    property Image iconItem: icon

    property string toolTipText: ""

    property color textColor: Style.ncTextColor
    property color textColorHovered: textColor

    property alias bgColor: bgRectangle.color

    property bool bold: false

    property alias bgNormalOpacity: bgRectangle.normalOpacity
    property alias bgHoverOpacity: bgRectangle.hoverOpacity

    background: Rectangle {
        id: bgRectangle

        property real normalOpacity: 0.3
        property real hoverOpacity: 1.0

        color: "transparent"
        opacity: parent.hovered ? hoverOpacity : normalOpacity
        radius: width / 2
    }

    leftPadding: root.text === "" ? 5 : 10
    rightPadding: root.text === "" ? 5 : 10

    NCToolTip {
        text: root.toolTipText
        visible: root.toolTipText !== "" && root.hovered
    }

    contentItem: RowLayout {
        Image {
            id: icon

            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

            source: root.hovered ? root.imageSourceHover : root.imageSource
            fillMode: Image.PreserveAspectFit
        }

        Label {
            Layout.maximumWidth: icon.width > 0 ? parent.width - icon.width - parent.spacing : parent.width
            Layout.fillWidth: icon.status !== Image.Ready

            text: root.text
            textFormat: Text.PlainText
            font.bold: root.bold

            visible: root.text !== ""

            color: root.hovered ? root.textColorHovered : root.textColor

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            elide: Text.ElideRight
        }
    }
}
