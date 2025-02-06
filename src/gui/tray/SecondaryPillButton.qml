import QtQuick
import QtQuick.Controls
import com.ionos.hidrivenext.desktopclient 

import Style

Button{
  id: root
  hoverEnabled: false // turn off default button hover

  property string iconSource

  property bool isMouseOver: false

  contentItem: Row {
    spacing: Style.sesPillButtonVerticalPadding
    padding: Style.sesPillButtonVerticalPadding
    leftPadding: Style.sesPillButtonHorizontalPadding
    rightPadding: Style.sesPillButtonHorizontalPadding
    anchors.centerIn: parent
    Text {
        text: root.text
        color: "black"
        font.weight: Style.sesFontNormalWeight
        font.pointSize: Style.sesFontPointSize
    }
    Image {
      visible: root.iconSource
      source: root.iconSource
      width: Style.sesPillIconSize
      height: Style.sesPillIconSize
    }
  }

  background: Rectangle {
    color: Style.sesPillButtonSecondaryBackgroundColor
    opacity: root.isMouseOver ? Style.sesPillButtonHoverOpacity : 1.0
    border.width: 2
    border.color: Style.sesPillButtonBorderColor
    radius: height / 2
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onExited: root.isMouseOver = false
        onEntered: root.isMouseOver = true
    }
  }
}