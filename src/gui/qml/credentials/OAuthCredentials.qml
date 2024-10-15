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

import QtQuick

// specifically use the basic style, as we modify the palette
import QtQuick.Controls.Basic
import QtQuick.Layouts

import org.ownCloud.resources 1.0
import org.ownCloud.gui 1.0
import org.ownCloud.libsync 1.0

Credentials {
    readonly property QmlOAuthCredentials credentials: ocContext
    readonly property OCQuickWidget widget: ocQuickWidget

    Label {
        Layout.alignment: Qt.AlignHCenter
        horizontalAlignment: Text.AlignHCenter
        text: credentials.isValid ? qsTr("Log in with your web browser") : qsTr("Login failed, please try it again")
    }

    StackLayout {
        id: stackLayout
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: false
        Layout.fillHeight: false
        currentIndex: credentials.isValid ? 0 : 1
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Button {
                id: openBrowserButton
                property bool browserWasOpened: false
                horizontalPadding: 64
                enabled: credentials.ready
                visible: credentials.isValid
                icon.source: QMLResources.resourcePath("core", "open", true)
                text: browserWasOpened ? qsTr("Reopen web browser") : qsTr("Open web brower")
                onClicked: {
                    browserWasOpened = true;
                    credentials.openAuthenticationUrlInBrowser();
                }
                Keys.onBacktabPressed: {
                    widget.parentFocusWidget.focusPrevious();
                }
            }

            Button {
                id: copyToClipboardButton
                Layout.preferredWidth: openBrowserButton.implicitWidth
                visible: credentials.isValid

                text: qsTr("Copy url")
                icon.source: QMLResources.resourcePath("core", "copy", true)
                onClicked: credentials.copyAuthenticationUrlToClipboard()
                enabled: credentials.ready

                hoverEnabled: true
                ToolTip.text: copyToClipboardButton.text
                ToolTip.visible: hovered
                ToolTip.delay: 500

                Keys.onTabPressed: event => {
                    // there is no lougout button
                    if (!credentials.isRefresh) {
                        widget.parentFocusWidget.focusNext();
                        event.accepted = true;
                    }
                }
            }

            Loader {
                Layout.preferredWidth: openBrowserButton.implicitWidth
                Layout.alignment: Qt.AlignHCenter
                sourceComponent: logOutButton
            }
        }

        Button {
            id: restartButton
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: openBrowserButton.width
            visible: !credentials.isValid
            icon.source: QMLResources.resourcePath("core", "undo", true)
            text: qsTr("Restart authenticaion")
            onClicked: credentials.requestRestart()

            Keys.onBacktabPressed: {
                widget.parentFocusWidget.focusPrevious();
            }
        }
    }

    Connections {
        target: widget

        function onFocusFirst() {
            stackLayout.nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason);
        }
    }
}
