/*
 * Copyright (C) 2022 by Camila Ayres <camila@nextcloud.com>
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import QtQuick 2.15
import QtQuick.Window 2.15
import Style 1.0
import com.nextcloud.desktopclient 1.0
import QtQuick.Layouts 1.2
import QtMultimedia 5.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

ApplicationWindow {
    id: root
    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    readonly property int windowSpacing: 10
    readonly property int windowWidth: 240

    readonly property string svgImage: "image://svgimage-custom-color/%1.svg" + "/"
    readonly property string talkIcon: svgImage.arg("wizard-talk")
    readonly property string deleteIcon: svgImage.arg("delete")

    // We set talkNotificationData, subject, and links properties in C++
    property var accountState: ({})
    property var talkNotificationData: ({})
    property string subject: ""
    property var links: []
    property string link: ""
    property string ringtonePath: "qrc:///client/theme/call-notification.wav"

    readonly property bool usingUserAvatar: root.talkNotificationData.userAvatar !== ""

    function closeNotification() {
        callStateChecker.checking = false;
        ringSound.stop();
        root.close();

        Systray.destroyDialog(root);
    }

    width: root.windowWidth
    height: rootBackground.height

    // TODO: Rather than setting all these palette colours manually,
    // create a custom style and do it for all components globally
    palette {
        text: Style.ncTextColor
        windowText: Style.ncTextColor
        buttonText: Style.ncTextColor
        brightText: Style.ncTextBrightColor
        highlight: Style.lightHover
        highlightedText: Style.ncTextColor
        light: Style.lightHover
        midlight: Style.ncSecondaryTextColor
        mid: Style.darkerHover
        dark: Style.menuBorder
        button: Style.buttonBackgroundColor
        window: Style.backgroundColor
        base: Style.backgroundColor
        toolTipBase: Style.backgroundColor
        toolTipText: Style.ncTextColor
    }

    Component.onCompleted: {
        Systray.forceWindowInit(root);
        Systray.positionNotificationWindow(root);

        root.show();
        root.raise();
        root.requestActivate();

        ringSound.play();
        callStateChecker.checking = true;
    }

    CallStateChecker {
        id: callStateChecker
        token: root.talkNotificationData.conversationToken
        accountState: root.accountState

        onStopNotifying: root.closeNotification()
    }

    Audio {
        id: ringSound
        source: root.ringtonePath
        loops: 9 // about 45 seconds of audio playing
        audioRole: Audio.RingtoneRole
    }

    Rectangle {
        id: rootBackground
        width: parent.width
        height: contentLayout.height + (root.windowSpacing * 2)
        radius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius
        color: Style.backgroundColor
        border.width: Style.trayWindowBorderWidth
        border.color: palette.dark
        clip: true

        Loader {
            id: backgroundLoader
            anchors.fill: parent
            active: root.usingUserAvatar
            sourceComponent: Item {
                anchors.fill: parent

                Image {
                    id: backgroundImage
                    anchors.fill: parent
                    cache: true
                    source: root.talkNotificationData.userAvatar
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    visible: false
                }

                FastBlur {
                    id: backgroundBlur
                    anchors.fill: backgroundImage
                    source: backgroundImage
                    radius: 50
                    visible: false
                }

                Rectangle {
                    id: backgroundMask
                    color: "white"
                    radius: rootBackground.radius
                    anchors.fill: backgroundImage
                    visible: false
                    width: backgroundImage.paintedWidth
                    height: backgroundImage.paintedHeight
                }

                OpacityMask {
                    id: backgroundOpacityMask
                    anchors.fill: backgroundBlur
                    source: backgroundBlur
                    maskSource: backgroundMask
                }

                Rectangle {
                    id: darkenerRect
                    anchors.fill: parent
                    color: "black"
                    opacity: 0.4
                    visible: backgroundOpacityMask.visible
                    radius: rootBackground.radius
                }
            }
        }

        ColumnLayout {
            id: contentLayout
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: root.windowSpacing
            spacing: root.windowSpacing

            Item {
                width: Style.accountAvatarSize
                height: Style.accountAvatarSize
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                Image {
                    id: callerAvatar
                    anchors.fill: parent
                    cache: true

                    source: root.usingUserAvatar ? root.talkNotificationData.userAvatar :
                                                   Theme.darkMode ? root.talkIcon + palette.windowText : root.talkIcon + Style.ncBlue
                    sourceSize.width: Style.accountAvatarSize
                    sourceSize.height: Style.accountAvatarSize

                    visible: !root.usingUserAvatar

                    Accessible.role: Accessible.Indicator
                    Accessible.name: qsTr("Talk notification caller avatar")
                }

                Rectangle {
                    id: mask
                    color: "white"
                    radius: width * 0.5
                    anchors.fill: callerAvatar
                    visible: false
                    width: callerAvatar.paintedWidth
                    height: callerAvatar.paintedHeight
                }

                OpacityMask {
                    anchors.fill: callerAvatar
                    source: callerAvatar
                    maskSource: mask
                    visible: root.usingUserAvatar
                }
            }

            EnforcedPlainTextLabel {
                id: message
                text: root.subject
                color: root.usingUserAvatar ? palette.brightText : palette.windowText
                font.pixelSize: Style.topLinePixelSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: root.windowSpacing / 2
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                Repeater {
                    id: linksRepeater
                    model: root.links

                    CustomButton {
                        id: answerCall
                        readonly property string verb: modelData.verb
                        readonly property bool isAnswerCallButton: verb === "WEB"

                        visible: isAnswerCallButton
                        text: modelData.label
                        contentsFont.bold: true
                        bgColor: Style.ncBlue
                        bgNormalOpacity: 0.8

                        textColor: palette.brightText

                        icon.source: root.talkIcon + palette.brightText
                        imageSourceHover: root.talkIcon + palette.brightText

                        Layout.fillWidth: true
                        Layout.preferredHeight: Style.callNotificationPrimaryButtonMinHeight

                        onClicked: {
                            Qt.openUrlExternally(root.link);
                            root.closeNotification();
                        }

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Answer Talk call notification")
                        Accessible.onPressAction: answerCall.clicked()
                    }

                }

                CustomButton {
                    id: declineCall
                    text: qsTr("Decline")
                    contentsFont.bold: true
                    bgColor: Style.errorBoxBackgroundColor
                    bgNormalOpacity: 0.8

                    textColor: palette.brightText

                    icon.source: root.deleteIcon + "white"
                    imageSourceHover: root.deleteIcon + "white"

                    Layout.fillWidth: true
                    Layout.preferredHeight: Style.callNotificationPrimaryButtonMinHeight

                    onClicked: root.closeNotification()

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Decline Talk call notification")
                    Accessible.onPressAction: declineCall.clicked()
                }
            }
        }

    }
}
