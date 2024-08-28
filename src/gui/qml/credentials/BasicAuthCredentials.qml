/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

// specifically use the basic style, as we modify the palette
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import org.ownCloud.resources 1.0
import org.ownCloud.gui 1.0
import org.ownCloud.libsync 1.0

Credentials {
    readonly property QmlBasicCredentials credentials: ocContext

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: credentials.isReadOnlyName ? qsTr("Please enter your password to log in.") : qsTr("Please enter %1 and password to log in.").arg(credentials.userNameLabel)
    }

    Label {
        text: credentials.userNameLabel
    }
    TextField {
        id: userNameField
        placeholderText: qsTr("Enter %1").arg(credentials.userNameLabel)
        horizontalAlignment: TextField.AlignHCenter
        text: credentials.userName
        enabled: !credentials.isReadOnlyName
        onTextChanged: {
            credentials.userName = text;
        }

        Keys.onBacktabPressed: {
            ocParentWidget.focusPrevious();
        }
    }

    Label {
        text: qsTr("Password")
    }
    TextField {
        id: passwordField
        horizontalAlignment: TextField.AlignHCenter
        placeholderText: qsTr("Enter Password")
        text: credentials.password
        echoMode: TextField.PasswordEchoOnEdit
        onTextChanged: {
            credentials.password = text;
        }
        Keys.onTabPressed: event => {
            // there is no lougout button
            if (!credentials.isRefresh) {
                ocParentWidget.focusNext();
                event.accepted = true;
            }
        }
    }

    Item {
        Layout.maximumHeight: 40
        Layout.fillHeight: true
    }

    Button {
        id: loginButton
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: passwordField.width
        // don't show this button in the wizard
        visible: credentials.isRefresh
        text: qsTr("Log in")
        enabled: credentials.ready
        onClicked: credentials.loginRequested()
    }

    Loader {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: passwordField.width
        sourceComponent: logOutButton
    }

    Connections {
        target: ocParentWidget

        function onFocusFirst() {
            if (userNameField.enabled) {
                userNameField.forceActiveFocus(Qt.TabFocusReason);
            } else {
                userNameField.nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason);
            }
        }
    }
}
