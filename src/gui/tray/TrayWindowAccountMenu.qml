import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as NativeDialogs

import "../"
import "../filedetails/"

// Custom qml modules are in /theme (and included by resources.qrc)
import Style

import com.ionos.hidrivenext.desktopclient

Button {
    id: currentAccountButton

    display:                AbstractButton.IconOnly
    flat:                   true

    Accessible.role: Accessible.ButtonMenu
    Accessible.name: qsTr("Current account")
    Accessible.onPressAction: currentAccountButton.clicked()

    property bool isHovered: currentAccountButton.hovered || currentAccountButton.visualFocus
    property bool isActive: currentAccountButton.pressed

    background: Rectangle {
        color: currentAccountButton.isActive ? Style.sesButtonPressed : 
               currentAccountButton.isHovered ? Style.sesAccountMenuHover : 
               accountMenu.visible? Style.sesSelectedColor : "transparent"
        radius: Style.sesCornerRadius
    }

    // We call open() instead of popup() because we want to position it
    // exactly below the dropdown button, not the mouse
    onClicked: {
        syncPauseButton.text = Systray.syncIsPaused ? qsTr("Resume sync for all") : qsTr("Pause sync for all")
        if (accountMenu.visible) {
            accountMenu.close()
        } else {
            accountMenu.open()
        }
    }

    onVisibleChanged: {
        // HACK: reload account Instantiator immediately by restting it - could be done better I guess
        // see also id:accountMenu below
        userLineInstantiator.active = false;
        userLineInstantiator.active = true;
    }

    Menu {
        id: accountMenu

        // x coordinate grows towards the right
        // y coordinate grows towards the bottom
        x: (currentAccountButton.x + 2)
        y: (currentAccountButton.y + Style.trayWindowHeaderHeight + 2)

        width: Style.sesAccountMenuWidth
        height: Math.min(implicitHeight, maxMenuHeight)
        closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

        clip: true

        background: Rectangle {
            border.color: Style.sesBorderColor
            color: Style.sesWhite
            radius: Style.sesCornerRadius
        }

        contentItem: ScrollView {
            id: accMenuScrollView
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            data: WheelHandler {
                target: accMenuScrollView.contentItem
            }
            ListView {
                implicitHeight: contentHeight
                model: accountMenu.contentModel
                interactive: true
                clip: true
                currentIndex: accountMenu.currentIndex
            }
        }

        onClosed: {
            // HACK: reload account Instantiator immediately by restting it - could be done better I guess
            // see also onVisibleChanged above
            userLineInstantiator.active = false;
            userLineInstantiator.active = true;
        }

        Instantiator {
            id: userLineInstantiator
            model: UserModel
            delegate: UserLine {
                onShowUserStatusSelector: {
                    userStatusDrawer.openUserStatusDrawer(model.index);
                    accountMenu.close();
                }
                onClicked: UserModel.currentUserId = model.index;
            }
            onObjectAdded: accountMenu.insertItem(index, object)
            onObjectRemoved: accountMenu.removeItem(object)
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            implicitHeight: 1
            color: Style.sesBorderColor
        }

        AccountMenuItem{
            id: addAccountButton
            icon.source: Style.sesDarkPlus
            text: qsTr("Add account")
            onClicked: UserModel.addAccount()
            visible: Systray.enableAddAccount
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            implicitHeight: 1
            color: Style.sesBorderColor
        }

        AccountMenuItem{
            id: syncPauseButton
            onClicked: Systray.syncIsPaused = !Systray.syncIsPaused
            icon.source: Systray.syncIsPaused ? Style.sesAccountResume : Style.sesAccountPause
        }

        AccountMenuItem{
            id: settingsButton
            text: qsTr("Settings")
            onClicked: Systray.openSettings()
            icon.source: Style.sesAccountSettings
        }

        AccountMenuItem{
            id: exitButton
            text: qsTr("Exit")
            onClicked: Systray.shutdown()
            icon.source: Style.sesAccountQuit
        }
    }

    RowLayout {
        id: accountControlRowLayout

        height: Style.sesAccountButtonHeight
        width:  Style.sesAccountButtonWidth
        spacing: 0

        Image {
            id: currentAccountAvatar

            Layout.leftMargin: Style.sesAccountButtonLeftMargin
            verticalAlignment: Qt.AlignCenter
            cache: false
            source: Style.sesAvatar
            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Current account avatar")

            Rectangle {
                id: currentAccountStatusIndicatorBackground
                // SES-50 Remove Inidcator
                // visible: UserModel.currentUser.isConnected
                //             && UserModel.currentUser.serverHasUserStatus
                visible: false
                width: Style.accountAvatarStateIndicatorSize +  + Style.trayFolderStatusIndicatorSizeOffset
                height: width
                anchors.bottom: currentAccountAvatar.bottom
                anchors.right: currentAccountAvatar.right
                anchors.bottomMargin: -5
                anchors.rightMargin: -5
                color: Style.currentUserHeaderColor
                radius: width * Style.trayFolderStatusIndicatorRadiusFactor
            }

            Rectangle {
                id: currentAccountStatusIndicatorMouseHover
                // SES-50 Remove Inidcator
                // visible: UserModel.currentUser.isConnected
                //             && UserModel.currentUser.serverHasUserStatus
                visible: false
                width: Style.accountAvatarStateIndicatorSize +  + Style.trayFolderStatusIndicatorSizeOffset
                height: width
                anchors.bottom: currentAccountAvatar.bottom
                anchors.right: currentAccountAvatar.right
                anchors.bottomMargin: -5
                anchors.rightMargin: -5
                color: currentAccountButton.hovered ? Style.sesHover : "transparent"
                opacity: Style.trayFolderStatusIndicatorMouseHoverOpacityFactor
                radius: width * Style.trayFolderStatusIndicatorRadiusFactor
            }

            Image {
                id: currentAccountStatusIndicator
                // SES-50 Remove Inidcator
                // visible: UserModel.currentUser.isConnected
                //             && UserModel.currentUser.serverHasUserStatus
                visible: false
                source: UserModel.currentUser.statusIcon
                cache: false
                x: currentAccountStatusIndicatorBackground.x + 1
                y: currentAccountStatusIndicatorBackground.y + 1
                sourceSize.width: Style.accountAvatarStateIndicatorSize
                sourceSize.height: Style.accountAvatarStateIndicatorSize

                Accessible.role: Accessible.Indicator
                Accessible.name: UserModel.desktopNotificationsAllowed ? qsTr("Current account status is online") : qsTr("Current account status is do not disturb")
            }
        }

        Column {
            id: accountLabels
            spacing: 0
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            Layout.leftMargin: Style.sesSmallMargin
            Layout.fillWidth: true
            Layout.maximumWidth: parent.width

            EnforcedPlainTextLabel {
                id: currentAccountUser
                Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                width: Style.sesAccountLabelWidth
                text: UserModel.currentUser.name
                elide: Text.ElideRight
                color: Style.currentUserHeaderTextColor
                font: root.font
            }

            RowLayout {
                id: currentUserStatus
                visible: UserModel.currentUser.isConnected &&
                            UserModel.currentUser.serverHasUserStatus
                spacing: Style.accountLabelsSpacing
                width: parent.width

                EnforcedPlainTextLabel {
                    id: emoji
                    visible: UserModel.currentUser.statusEmoji !== ""
                    width: Style.userStatusEmojiSize
                    text: UserModel.currentUser.statusEmoji
                }
                EnforcedPlainTextLabel {
                    id: message
                    Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                    Layout.fillWidth: true
                    visible: UserModel.currentUser.statusMessage !== ""
                    width: Style.currentAccountLabelWidth
                    text: UserModel.currentUser.statusMessage !== ""
                            ? UserModel.currentUser.statusMessage
                            : UserModel.currentUser.server
                    elide: Text.ElideRight
                    color: Style.currentUserHeaderTextColor
                }
            }
        }

        // ColorOverlay {
        //     cached: true
        //     color: Style.currentUserHeaderTextColor
        //     width: source.width
        //     height: source.height
        //     Layout.rightMargin: Style.sesAccountButtonRightMargin
        //     source: Image {
        //         Layout.alignment: Qt.AlignRight
        //         verticalAlignment: Qt.AlignCenter
        //         source: Style.sesChevron
        //         sourceSize.width: 12
        //         sourceSize.height: 7
        //         Accessible.role: Accessible.PopupMenu
        //         Accessible.name: qsTr("Account switcher and settings menu")
        //     }
        // }
    }
}