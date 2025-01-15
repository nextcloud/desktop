import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15
import Qt.labs.platform 1.1 as NativeDialogs

import "../"
import "../filedetails/"

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
        anchors.leftMargin: 20
        anchors.rightMargin: 20

        TrayWindowAccountMenu{   
            Layout.preferredWidth:  Style.currentAccountButtonWidth
            Layout.preferredHeight: Style.trayWindowHeaderHeight
        }

        HeaderButton {
            id: trayWindowWebsiteButton

            icon.source: Style.sesWebsiteIcon
            icon.color: Style.sesIconColor 
            onClicked: UserModel.openCurrentAccountServer()

            text: qsTr("Website")

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Open Nextcloud in browser")
            Accessible.onPressAction: trayWindowWebsiteButton.clicked()

            Layout.alignment: Qt.AlignRight
            Layout.preferredWidth:  Style.trayWindowHeaderHeight
            Layout.preferredHeight: Style.trayWindowHeaderHeight 
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

            Layout.alignment: Qt.AlignRight
            Layout.preferredWidth:  Style.trayWindowHeaderHeight
            Layout.preferredHeight: Style.trayWindowHeaderHeight
        }

        HeaderButton {
            id: trayWindowTalkButton

            visible: false //SES-4 removed  
            icon.source: "qrc:///client/theme/white/talk-app.svg"
            icon.color: Style.currentUserHeaderTextColor
            onClicked: UserModel.openCurrentAccountTalk()

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Open Nextcloud Talk in browser")
            Accessible.onPressAction: trayWindowTalkButton.clicked()

            Layout.alignment: Qt.AlignRight
            Layout.preferredWidth:  Style.trayWindowHeaderHeight
            Layout.preferredHeight: Style.trayWindowHeaderHeight

        }

        HeaderButton {
            id: trayWindowAppsButton
            icon.source: "qrc:///client/theme/white/more-apps.svg"
            icon.color: Style.currentUserHeaderTextColor
            
            visible: false //SES-4 removed

            onClicked: {
                if(appsMenuListView.count <= 0) {
                    UserModel.openCurrentAccountServer()
                } else if (appsMenu.visible) {
                    appsMenu.close()
                } else {
                    appsMenu.open()
                }
            }

            Accessible.role: Accessible.ButtonMenu
            Accessible.name: qsTr("More apps")
            Accessible.onPressAction: trayWindowAppsButton.clicked()

            Menu {
                id: appsMenu
                x: Style.trayWindowMenuOffsetX
                y: (trayWindowAppsButton.y + trayWindowAppsButton.height + Style.trayWindowMenuOffsetY)
                width: Style.trayWindowWidth * Style.trayWindowMenuWidthFactor
                height: implicitHeight + y > Style.trayWindowHeight ? Style.trayWindowHeight - y : implicitHeight
                closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                background: Rectangle {
                    border.color: Style.sesBorderColor
                    color: Style.sesWhite
                    radius: 2
                }

                contentItem: ScrollView {
                    id: appsMenuScrollView
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    data: WheelHandler {
                        target: appsMenuScrollView.contentItem
                    }
                    ListView {
                        id: appsMenuListView
                        implicitHeight: contentHeight
                        model: UserAppsModel
                        interactive: true
                        clip: true
                        currentIndex: appsMenu.currentIndex
                        delegate: MenuItem {
                            id: appEntry
                            anchors.left: parent.left
                            anchors.right: parent.right

                            text: model.appName
                            font.pixelSize: Style.topLinePixelSize
                            icon.source: model.appIconUrl
                            icon.color: Style.ncTextColor
                            onTriggered: UserAppsModel.openAppUrl(appUrl)
                            hoverEnabled: true

                            background: Item {
                                height: parent.height
                                width: parent.width
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    color: parent.parent.hovered || parent.parent.visualFocus ? Style.sesHover : "transparent"
                                }
                            }

                            Accessible.role: Accessible.MenuItem
                            Accessible.name: qsTr("Open %1 in browser").arg(model.appName)
                            Accessible.onPressAction: appEntry.triggered()
                        }
                    }
                }
            }
        }
    }
}   // Rectangle trayWindowHeaderBackground
