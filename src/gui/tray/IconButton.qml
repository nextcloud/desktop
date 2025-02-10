import QtQuick
import QtQuick.Controls
import com.ionos.hidrivenext.desktopclient 

import Style

Button{
  id: root
  property string iconSource
  property string iconSourceHovered

  property bool isMouseOver: false
  property bool isActive: root.pressed
  property bool customHoverEnabled: true
  property string toolTipText

  hoverEnabled: false // turn off default button hover

  Image {
    id: icon
    visible: true
    source: root.isMouseOver ? root.iconSourceHovered : root.iconSource
    anchors.centerIn: parent
    fillMode: Image.PreserveAspectFit
    width: Style.sesPillIconSize
    height: Style.sesPillIconSize
  }

  ToolTip {
      text: root.toolTipText
      visible: root.isMouseOver
  }

  background: Rectangle {
    anchors.centerIn: parent
    color: root.isMouseOver ? root.pressed ? Style.sesActionPressed : Style.sesActionHover : "transparent"
    opacity: 1.0
    radius: height / 2
    width: 24
    height: 24

    Behavior on color {
        ColorAnimation { duration: Style.shortAnimationDuration }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: root.customHoverEnabled
        onExited: root.isMouseOver = false
        onEntered: root.isMouseOver = true
    }
  }
}