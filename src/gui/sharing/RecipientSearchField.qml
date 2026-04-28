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

// Based on the old `ShareeSearchField` component from filedetails.
// While Qt 6.10+ has a `SearchField` type, it's still lacking some features
// such as a placeholder text.
TextField {
    id: root

    signal recipientSelected(string recipientType, string recipientValue)

    required property var account
    // property bool isShareeFetchOngoing: recipientModel.fetchOngoing
    property RecipientSearchModel recipientModel: RecipientSearchModel {
        account: root.account
        query: root.text
    }

    readonly property int horizontalPaddingOffset: Style.trayHorizontalMargin
    readonly property double iconsScaleFactor: 0.6

    function triggerSuggestionsVisibility() {
        recipientListView.count > 0 ? suggestionsPopup.open() : suggestionsPopup.close();
    }

    placeholderText: enabled ? qsTr("Search for recipients") : qsTr("Sharing is not available for this folder")
    verticalAlignment: Qt.AlignVCenter
    implicitHeight: Math.max(Style.talkReplyTextFieldPreferredHeight, contentHeight)

    onActiveFocusChanged: triggerSuggestionsVisibility()
    onTextChanged: triggerSuggestionsVisibility()
    Keys.onPressed: {
        if(suggestionsPopup.visible) {
            switch(event.key) {
            case Qt.Key_Escape:
                suggestionsPopup.close();
                recipientListView.currentIndex = -1;
                event.accepted = true;
                break;

            case Qt.Key_Up:
                recipientListView.decrementCurrentIndex();
                event.accepted = true;
                break;

            case Qt.Key_Down:
                recipientListView.incrementCurrentIndex();
                event.accepted = true;
                break;

            case Qt.Key_Enter:
            case Qt.Key_Return:
                if(recipientListView.currentIndex > -1) {
                    recipientListView.itemAtIndex(recipientListView.currentIndex).selectItem();
                    event.accepted = true;
                    break;
                }
            }
        } else {
            switch(event.key) {
            case Qt.Key_Down:
                triggerSuggestionsVisibility();
                event.accepted = true;
                break;
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
/*
    NCBusyIndicator {
        id: busyIndicator

        anchors {
            top: parent.top
            left: parent.left
            bottom: parent.bottom
        }

        width: height
        color: palette.placeholderText
        visible: root.recipientModel.fetchOngoing
        running: visible
    }
 */
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

                spacing: 0
                currentIndex: -1
                interactive: true

                highlight: Rectangle {
                    anchors.fill: recipientListView.currentItem
                    color: palette.highlight
                }
                highlightFollowsCurrentItem: true
                highlightMoveDuration: 0
                highlightResizeDuration: 0
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: 0
                preferredHighlightEnd: suggestionsScrollView.height

                onCountChanged: root.triggerSuggestionsVisibility()

                model: root.recipientModel
                delegate: ItemDelegate {
                    id: recipientDelegate
                    required property int index

                    required property string type
                    required property string value
                    required property string displayName
                    required property string iconUrl

                    width: recipientListView.contentItem.width

                    text: displayName

                    contentItem: RowLayout {
                        Image {
                            source: recipientDelegate.iconUrl
                        }
                        Label {
                            text: recipientDelegate.displayName
                        }
                        Label {
                            text: recipientDelegate.type
                        }
                    }

                    // enabled: model.type !== NC.recipient.LookupServerSearchResults
                    // hoverEnabled: model.type !== NC.recipient.LookupServerSearchResults

                    function selectSharee() {
                        console.log(`recipientSelected: ${JSON.stringify([recipientDelegate.type, recipientDelegate.value])})`)
                        root.recipientSelected(recipientDelegate.type, recipientDelegate.value);
                        suggestionsPopup.close();

                        root.clear();
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
                        const savedPreferredHighlightBegin = recipientListView.preferredHighlightBegin;
                        const savedPreferredHighlightEnd = recipientListView.preferredHighlightEnd;
                        // Set overkill values to make sure no scroll happens when we hover with mouse
                        recipientListView.preferredHighlightBegin = -suggestionsScrollView.height;
                        recipientListView.preferredHighlightEnd = suggestionsScrollView.height * 2;

                        recipientListView.currentIndex = index

                        // Reset original values so keyboard navigation makes list view scroll
                        recipientListView.preferredHighlightBegin = savedPreferredHighlightBegin;
                        recipientListView.preferredHighlightEnd = savedPreferredHighlightEnd;
                    }
                    onClicked: selectItem()
                }
            }
        }
    }
}
