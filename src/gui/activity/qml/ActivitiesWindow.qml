/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Style
import "../.."

WizardStyledWindow {
    id: root

    property var account: null
    property var activityUser: null
    property var activityModel: null
    property var syncStatusModel: null
    readonly property string headline: qsTr("Activities")

    title: ""
    width: Style.activitiesWindowWidth
    height: Style.activitiesWindowHeight
    minimumWidth: Style.wizardStandaloneWindowMinimumWidth
    minimumHeight: Style.wizardStandaloneWindowMinimumHeight

    function resetActivityList() {
        newActivitiesButtonLoader.active = false
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.close()
    }

    Component.onCompleted: resetActivityList()

    onVisibleChanged: {
        if (visible) {
            resetActivityList()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Style.wizardWindowMargin
        anchors.rightMargin: Style.wizardWindowMargin
        anchors.topMargin: Style.wizardWindowTopMargin
        anchors.bottomMargin: Style.wizardWindowMargin
        spacing: Style.wizardSectionSpacing

        WindowAccountHeader {
            Layout.fillWidth: true
            title: root.headline
            user: root.account
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Style.normalBorderWidth
            color: Style.wizardRowBorder
        }

        SyncStatus {
            id: syncStatus

            Layout.fillWidth: true
            accentColor: root.account ? root.account.accentColor : Style.accentColor
            syncStatusModel: root.syncStatusModel
            user: root.activityUser
            activityListModel: root.activityModel
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Style.normalBorderWidth
            color: Style.wizardRowBorder
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ActivityList {
                id: activityList

                anchors.fill: parent
                activeFocusOnTab: true
                accentColor: root.account ? root.account.accentColor : Style.accentColor
                model: root.activityModel
                onOpenFile: Qt.openUrlExternally(filePath)
                onActivityItemClicked: {
                    if (root.activityModel) {
                        root.activityModel.slotTriggerDefaultAction(index)
                    }
                }

                Connections {
                    target: root.activityModel

                    function onInteractiveActivityReceived() {
                        if (!activityList.atYBeginning) {
                            newActivitiesButtonLoader.active = true
                        }
                    }
                }
            }

            Loader {
                id: newActivitiesButtonLoader

                anchors.top: activityList.top
                anchors.topMargin: Style.smallSpacing
                anchors.horizontalCenter: activityList.horizontalCenter
                width: Style.newActivitiesButtonWidth
                height: Style.newActivitiesButtonHeight
                z: 1
                active: false

                sourceComponent: Button {
                    id: newActivitiesButton

                    anchors.fill: parent
                    hoverEnabled: true
                    padding: Style.smallSpacing
                    text: qsTr("New activities")
                    icon.source: "image://svgimage-custom-color/expand-less-black.svg/" + Style.currentUserHeaderTextColor
                    icon.width: Style.activityListButtonIconSize
                    icon.height: Style.activityListButtonIconSize
                    Accessible.role: Accessible.Button
                    Accessible.name: text
                    Accessible.onPressAction: newActivitiesButton.clicked()
                    onClicked: {
                        activityList.scrollToTop()
                        newActivitiesButtonLoader.active = false
                    }

                    Timer {
                        id: newActivitiesButtonDisappearTimer

                        interval: Style.newActivityButtonDisappearTimeout
                        running: newActivitiesButtonLoader.active && !newActivitiesButton.hovered
                        repeat: false
                        onTriggered: fadeoutActivitiesButtonDisappear.running = true
                    }

                    OpacityAnimator {
                        id: fadeoutActivitiesButtonDisappear

                        target: newActivitiesButton
                        from: 1
                        to: 0
                        duration: Style.newActivityButtonDisappearFadeTimeout
                        loops: 1
                        running: false
                        onFinished: newActivitiesButtonLoader.active = false
                    }
                }
            }
        }
    }
}
