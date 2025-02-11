import QtQuick
import QtQuick.Controls
import com.ionos.hidrivenext.desktopclient 

import Style

Button{
  id: root
  hoverEnabled: false // turn off default button hover

  property string iconSource
  property string toolTipText

  property bool isMouseOver: false
  property bool removeBorder: false
  property color textColor: "black"

  contentItem: Row {
    spacing: Style.sesPillButtonVerticalPadding
    padding: Style.sesPillButtonVerticalPadding
    leftPadding: Style.sesPillButtonHorizontalPadding
    rightPadding: Style.sesPillButtonHorizontalPadding
    anchors.centerIn: parent
    Text {
        text: root.text
        color: textColor
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
    border.width: root.removeBorder ? 0 : 2
    border.color: Style.sesPillButtonBorderColor
    radius: height / 2

    Behavior on opacity {
        NumberAnimation { duration: Style.shortAnimationDuration }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onExited: root.isMouseOver = false
        onEntered: root.isMouseOver = true
    }
  }

  ToolTip {
      text: root.toolTipText
      visible: root.toolTipText && root.isMouseOver
  }
}