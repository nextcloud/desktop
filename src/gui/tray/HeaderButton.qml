import QtQml 2.1
import QtQml.Models 2.1
import QtQuick 2.9
import QtQuick.Window 2.3
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.0

// Custom qml modules are in /theme (and included by resources.qrc)
import Style 1.0
import com.nextcloud.desktopclient 1.0

Button {
    id: root

    display: AbstractButton.IconOnly
    flat: true
    hoverEnabled: Style.hoverEffectsEnabled

    icon.width: Style.headerButtonIconSize
    icon.height: Style.headerButtonIconSize
    icon.color: Style.ncHeaderTextColor

    Layout.alignment: Qt.AlignRight
    Layout.preferredWidth:  Style.trayWindowHeaderHeight
    Layout.preferredHeight: Style.trayWindowHeaderHeight

    background: Rectangle {
        color: root.hovered || root.visualFocus ? UserModel.currentUser.headerTextColor : "transparent"
        opacity: 0.2
    }
}
