import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0

Button {
    id: root

    property string imageSource: ""
    property string imageSourceHover: imageSource
    property var iconItem: icon

    property string toolTipText: ""

    property color textColor: Style.ncTextColor
    property color textColorHovered: textColor

    property alias contentsFont: contents.font

    property alias bgColor: bgRectangle.color
    property alias bgNormalOpacity: bgRectangle.normalOpacity
    property alias bgHoverOpacity: bgRectangle.hoverOpacity

    background: NCButtonBackground {
        id: bgRectangle
        hovered: root.hovered
    }

    leftPadding: root.text === "" ? Style.smallSpacing : Style.standardSpacing
    rightPadding: root.text === "" ? Style.smallSpacing : Style.standardSpacing
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding

    NCToolTip {
        text: root.toolTipText
        visible: root.toolTipText !== "" && root.hovered
    }

    contentItem: NCButtonContents {
        id: contents
        hovered: root.hovered
        imageSourceHover: root.imageSourceHover
        imageSource: root.imageSource
        text: root.text
        textColor: root.textColor
        textColorHovered: root.textColorHovered
    }
}
