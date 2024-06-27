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
import QtQuick.Controls
import QtQuick.Layouts

import org.ownCloud.resources 1.0

ColumnLayout {
    property bool collapsed: true
    required property var errorMessages

    component ErrorItem: RowLayout {
        property alias text: label.text
        property alias maximumLineCount: label.maximumLineCount
        Image {
            Layout.alignment: Qt.AlignTop
            source: QMLResources.resourcePath("core", "warning", enabled)
            Layout.maximumHeight: 16
            Layout.maximumWidth: 16
            sourceSize.width: width
            sourceSize.height: height
        }
        Label {
            id: label
            Layout.fillWidth: true
            elide: Label.ElideLeft
            wrapMode: Label.WordWrap
        }
    }

    Component {
        id: expandedError
        ColumnLayout {
            Layout.fillWidth: true
            Repeater {
                model: errorMessages
                delegate: ErrorItem {
                    required property string modelData
                    text: modelData
                    Layout.fillWidth: true
                }
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "<a href='foo'>" + qsTr("Show less") + "</a>"
                onLinkActivated: {
                    collapsed = true;
                }
            }
        }
    }

    Component {
        id: collapsedError
        ColumnLayout {
            Layout.fillWidth: true
            ErrorItem {
                Layout.fillWidth: true
                text: errorMessages
                maximumLineCount: 1
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "<a href='foo'>" + qsTr("Show more") + "</a>"
                onLinkActivated: {
                    collapsed = false;
                }
            }
        }
    }

    function loadComponent() {
        if (errorMessages.length) {
            return collapsed ? collapsedError : expandedError;
        }
        return undefined;
    }

    Loader {
        id: loader
        Layout.fillHeight: true
        Layout.fillWidth: true
        sourceComponent: loadComponent()
    }

    onCollapsedChanged: {
        loader.sourceComponent = loadComponent();
    }
}
