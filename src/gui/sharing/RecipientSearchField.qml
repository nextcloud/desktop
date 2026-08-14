/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import com.nextcloud.desktopclient as NC
import Style
import "qrc:/qml/src/gui/wizard/qml"

// Based on the old `ShareeSearchField` component from filedetails.
// While Qt 6.10+ has a `SearchField` type, it's still lacking some features
// such as a placeholder text.
WizardTextField {
    id: root

    signal recipientSelected(string recipientType, string recipientValue, string recipientInstance)

    required property var account
    required property string shareId
    property RecipientSearchModel recipientModel: RecipientSearchModel {
        account: root.account
        query: root.text
        shareId: root.shareId
    }

    readonly property int horizontalPaddingOffset: Style.trayHorizontalMargin
    readonly property double iconsScaleFactor: 0.6

    function triggerSuggestionsVisibility() {
        recipientListView.count > 0 ? suggestionsPopup.open() : suggestionsPopup.close()
    }

    placeholderText: enabled ? qsTr("Search for recipients") : qsTr("Sharing is not available for this folder")
    verticalAlignment: Qt.AlignVCenter
    onActiveFocusChanged: triggerSuggestionsVisibility()
    onTextChanged: triggerSuggestionsVisibility()
    Keys.onPressed: {
        if (suggestionsPopup.visible) {
            switch (event.key) {
            case Qt.Key_Escape:
                suggestionsPopup.close()
                recipientListView.currentIndex = -1
                event.accepted = true
                break
            case Qt.Key_Up:
                recipientListView.decrementCurrentIndex()
                event.accepted = true
                break
            case Qt.Key_Down:
                recipientListView.incrementCurrentIndex()
                event.accepted = true
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
                if (recipientListView.currentIndex > -1) {
                    recipientListView.itemAtIndex(recipientListView.currentIndex).selectItem()
                    event.accepted = true
                    break
                }
            }
        } else {
            switch (event.key) {
            case Qt.Key_Down:
                triggerSuggestionsVisibility()
                event.accepted = true
                break
            }
        }
    }

    leftPadding: searchIcon.width + searchIcon.anchors.leftMargin + horizontalPaddingOffset
    rightPadding: clearTextButton.width + clearTextButton.anchors.rightMargin + horizontalPaddingOffset

    Image {
        id: searchIcon
        anchors {
            top: parent.top
            left: parent.left
            bottom: parent.bottom
            margins: 4
        }

        width: height

        smooth: true
        antialiasing: true
        mipmap: true
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignLeft

        source: "image://svgimage-custom-color/search.svg" + "/" + palette.placeholderText
        sourceSize: Qt.size(parent.height * root.iconsScaleFactor, parent.height * root.iconsScaleFactor)

        visible: !root.recipientModel.fetchOngoing
    }
    Image {
        id: busyIndicator

        anchors {
            top: parent.top
            left: parent.left
            bottom: parent.bottom
        }

        width: height
        source: "image://svgimage-custom-color/change.svg/" + palette.placeholderText
        sourceSize: Qt.size(parent.height * root.iconsScaleFactor, parent.height * root.iconsScaleFactor)
        fillMode: Image.PreserveAspectFit
        visible: root.recipientModel.fetchOngoing

        RotationAnimator {
            target: busyIndicator
            running: busyIndicator.visible
            from: 0
            to: 360
            loops: Animation.Infinite
            duration: Style.shortAnimationDuration * 15
        }
    }

    Image {
        id: clearTextButton

        anchors {
            top: parent.top
            right: parent.right
            bottom: parent.bottom
            margins: 4
        }

        width: height

        smooth: true
        antialiasing: true
        mipmap: true
        fillMode: Image.PreserveAspectFit

        source: "image://svgimage-custom-color/clear.svg" + "/" + palette.placeholderText
        sourceSize: Qt.size(parent.height * root.iconsScaleFactor, parent.height * root.iconsScaleFactor)

        visible: root.text

        MouseArea {
            id: clearTextButtonMouseArea
            anchors.fill: parent
            onClicked: root.clear()
        }
    }

    Popup {
        id: suggestionsPopup

        width: root.width
        y: root.height

        contentItem: ScrollView {
            id: suggestionsScrollView

            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: recipientListView.contentHeight > recipientListView.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

            // need to take the popup's padding in account for the max height
            // remove bottomPadding twice to leave some space between the window border
            implicitHeight: Math.min(Window.height - parent.y - parent.topPadding - parent.bottomPadding * 2, contentHeight)

            ListView {
                id: recipientListView

                spacing: Style.extraSmallSpacing
                currentIndex: -1
                interactive: true
                highlightFollowsCurrentItem: true
                highlightMoveDuration: 0
                highlightResizeDuration: 0
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: 0
                preferredHighlightEnd: suggestionsScrollView.height

                onCountChanged: root.triggerSuggestionsVisibility()

                model: root.recipientModel
                delegate: WizardItemDelegate {
                    id: recipientDelegate
                    required property int index

                    required property string type
                    required property string value
                    required property string displayName
                    required property var instance
                    required property string iconSvgUrl
                    required property string iconLight
                    required property string iconDark

                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem

                    contentItem: RowLayout {
                        spacing: Style.standardSpacing

                        Image {
                            Layout.preferredWidth: Style.activityListButtonIconSize
                            Layout.preferredHeight: Style.activityListButtonIconSize
                            source: RecipientIcon.source(recipientDelegate.iconSvgUrl, recipientDelegate.iconLight, recipientDelegate.iconDark)
                            sourceSize: Qt.size(Style.activityListButtonIconSize, Style.activityListButtonIconSize)
                            fillMode: Image.PreserveAspectFit
                        }
                        EnforcedPlainTextLabel {
                            text: recipientDelegate.displayName
                            color: Style.wizardPrimaryText
                        }
                        EnforcedPlainTextLabel {
                            Layout.fillWidth: true
                            text: recipientDelegate.instance || ""
                            color: Style.wizardSecondaryText
                            elide: Text.ElideRight
                        }
                    }

                    // enabled: model.type !== NC.recipient.LookupServerSearchResults
                    // hoverEnabled: model.type !== NC.recipient.LookupServerSearchResults

                    function selectSharee() {
                        root.recipientSelected(recipientDelegate.type, recipientDelegate.value, recipientDelegate.instance || "")
                        suggestionsPopup.close()

                        root.clear()
                    }

                    function selectItem() {
                        // if (model.type === NC.recipient.LookupServerSearch) {
                        //     recipientListView.currentIndex = -1
                        //     root.recipientModel.searchGlobally()
                        // } else {
                        selectSharee()
                    // }
                    }

                    onHoveredChanged: if (hovered) {
                        // When we set the currentIndex the list view will scroll...
                        // unless we tamper with the preferred highlight points to stop this.
                        const savedPreferredHighlightBegin = recipientListView.preferredHighlightBegin
                        const savedPreferredHighlightEnd = recipientListView.preferredHighlightEnd
                        // Set overkill values to make sure no scroll happens when we hover with mouse
                        recipientListView.preferredHighlightBegin = -suggestionsScrollView.height
                        recipientListView.preferredHighlightEnd = suggestionsScrollView.height * 2

                        recipientListView.currentIndex = index;

                        // Reset original values so keyboard navigation makes list view scroll
                        recipientListView.preferredHighlightBegin = savedPreferredHighlightBegin
                        recipientListView.preferredHighlightEnd = savedPreferredHighlightEnd
                    }
                    onClicked: selectItem()
                }
            }
        }
    }
}
