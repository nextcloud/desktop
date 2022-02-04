import QtQuick 2.15
import QtQuick.Controls 2.3
import Style 1.0

Label {
    id: root

    property string toolTipText: ""
    property Action action: null
    property alias acceptedButtons: mouseArea.acceptedButtons
    property bool hovered: mouseArea.containsMouse

    height: implicitHeight

    property color textColor: Style.ncTextColor
    property color textColorHovered: Style.ncSecondaryTextColor

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: root.clicked(null)

    text: action ? action.text : ""
    enabled: !action || action.enabled
    onClicked: if (action) action.trigger()

    font.underline: true
    color: root.hovered ? root.textColorHovered : root.textColor
    horizontalAlignment: Text.AlignLeft
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight

    signal pressed(QtObject mouse)
    signal clicked(QtObject mouse)

    ToolTip {
        id: customTextButtonTooltip
        text: root.toolTipText
        delay: Qt.styleHints.mousePressAndHoldInterval
        visible: root.toolTipText !== "" && root.hovered
        contentItem: Label {
            text: customTextButtonTooltip.text
            color: Style.ncTextColor
        }
        background: Rectangle {
            border.color: Style.menuBorder
            color: Style.backgroundColor
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true

        onClicked: root.clicked(mouse)
        onPressed: root.pressed(mouse)
    }
}
