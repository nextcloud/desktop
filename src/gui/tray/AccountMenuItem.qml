import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Custom qml modules are in /theme (and included by resources.qrc)
import Style 1.0
import com.ionos.hidrivenext.desktopclient 1.0

MenuItem {
    id: accountMenuItem

    property bool isHovered: accountMenuItem.hovered || accountMenuItem.visualFocus
    property bool isActive: accountMenuItem.pressed

    font.pixelSize: Style.sesFontPixelSize
    hoverEnabled: true
    leftPadding: Style.sesMediumMargin
    topPadding: Style.sesAccountMenuItemPadding
    bottomPadding: Style.sesAccountMenuItemPadding
    spacing: Style.sesSmallMargin

    icon.height: Style.smallIconSize
    icon.width: Style.smallIconSize 
    icon.color: Style.sesIconDarkColor

    background: Item {
        height: parent.height
        width: parent.menu.width
        Rectangle {
            radius: 0
            anchors.fill: parent
            anchors.margins: 1
            color: accountMenuItem.isActive ? Style.sesButtonPressed : accountMenuItem.isHovered ? Style.sesAccountMenuHover : "transparent"
        }
    }

    Accessible.role: Accessible.MenuItem
    Accessible.name: text
    Accessible.onPressAction: accountMenuItem.clicked()
}