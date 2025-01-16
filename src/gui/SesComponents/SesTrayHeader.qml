import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform 1.1 as NativeDialogs

import "../"
import "../filedetails/"
import "../tray/"

// Custom qml modules are in /theme (and included by resources.qrc)
import Style 1.0

import com.ionos.hidrivenext.desktopclient


Rectangle {

    height:         Style.trayWindowHeaderHeight
    color:          Style.sesWhite
    radius: 0.0

    RowLayout {
        id: trayWindowHeaderLayout

        anchors.fill:   parent
        anchors.leftMargin: Style.sesTrayHeaderMargin
        anchors.rightMargin: Style.sesTrayHeaderMargin

        TrayWindowAccountMenu{
            Layout.preferredWidth:  Style.sesAccountButtonWidth
            Layout.preferredHeight: Style.sesAccountButtonHeight
        }

        HeaderButton {
            id: trayWindowWebsiteButton

            icon.source: Style.sesWebsiteIcon
            icon.color: Style.sesIconColor
            onClicked: UserModel.openCurrentAccountServer()

            text: qsTr("Website")

            Layout.rightMargin: 2

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Open Nextcloud in browser")
            Accessible.onPressAction: trayWindowWebsiteButton.clicked()
        }

        TrayFoldersMenuButton {
            id: openLocalFolderButton

            visible: currentUser.hasLocalFolder
            currentUser: UserModel.currentUser

            onClicked: openLocalFolderButton.userHasGroupFolders ? openLocalFolderButton.toggleMenuOpen() : UserModel.openCurrentAccountLocalFolder()

            onFolderEntryTriggered: isGroupFolder ? UserModel.openCurrentAccountFolderFromTrayInfo(fullFolderPath) : UserModel.openCurrentAccountLocalFolder()

            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Open local or group folders")
            Accessible.onPressAction: openLocalFolderButton.userHasGroupFolders ? openLocalFolderButton.toggleMenuOpen() : UserModel.openCurrentAccountLocalFolder()
        }
    }
}   // Rectangle trayWindowHeaderBackground
