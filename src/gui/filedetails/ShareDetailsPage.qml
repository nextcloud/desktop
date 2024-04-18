/*
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
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

import com.nextcloud.desktopclient 1.0
import Style 1.0
import "../tray"
import "../"

Page {
    id: root

    signal closeShareDetails
    signal deleteShare
    signal createNewLinkShare

    signal toggleAllowEditing(bool enable)
    signal toggleAllowResharing(bool enable)
    signal toggleHideDownload(bool enable)
    signal togglePasswordProtect(bool enable)
    signal toggleExpirationDate(bool enable)
    signal toggleNoteToRecipient(bool enable)
    signal permissionModeChanged(int permissionMode)

    signal setLinkShareLabel(string label)
    signal setExpireDate(var milliseconds) // Since QML ints are only 32 bits, use a variant
    signal setPassword(string password)
    signal setNote(string note)

    property bool backgroundsVisible: true
    property color accentColor: Style.ncBlue

    property FileDetails fileDetails: FileDetails {}
    property var shareModelData: ({})

    property bool canCreateLinkShares: true
    property bool serverAllowsResharing: true

    readonly property var share: shareModelData.share ?? ({})

    readonly property string iconUrl: shareModelData.iconUrl ?? ""
    readonly property string avatarUrl: shareModelData.avatarUrl ?? ""
    readonly property string text: shareModelData.display ?? ""
    readonly property string detailText: shareModelData.detailText ?? ""
    readonly property string link: shareModelData.link ?? ""
    readonly property string note: shareModelData.note ?? ""
    readonly property string password: shareModelData.password ?? ""
    readonly property string passwordPlaceholder: "●●●●●●●●●●"

    readonly property var expireDate: shareModelData.expireDate // Don't use int as we are limited
    readonly property var maximumExpireDate: shareModelData.enforcedMaximumExpireDate // msecs epoch

    readonly property string linkShareLabel: shareModelData.linkShareLabel ?? ""

    readonly property bool resharingAllowed: shareModelData.resharingAllowed
    readonly property bool editingAllowed: shareModelData.editingAllowed
    readonly property bool hideDownload: shareModelData.hideDownload
    readonly property bool noteEnabled: shareModelData.noteEnabled
    readonly property bool expireDateEnabled: shareModelData.expireDateEnabled
    readonly property bool expireDateEnforced: shareModelData.expireDateEnforced
    readonly property bool passwordProtectEnabled: shareModelData.passwordProtectEnabled
    readonly property bool passwordEnforced: shareModelData.passwordEnforced
    readonly property bool isSharePermissionChangeInProgress: shareModelData.isSharePermissionChangeInProgress
    readonly property bool isHideDownloadInProgress: shareModelData.isHideDownloadInProgress
    readonly property int  currentPermissionMode: shareModelData.currentPermissionMode

    readonly property bool isLinkShare: shareModelData.shareType === ShareModel.ShareTypeLink
    readonly property bool isEmailShare: shareModelData.shareType === ShareModel.ShareTypeEmail
    readonly property bool shareSupportsPassword: isLinkShare || isEmailShare

    readonly property bool isFolderItem: shareModelData.sharedItemType === ShareModel.SharedItemTypeFolder
    readonly property bool isEncryptedItem: shareModelData.sharedItemType === ShareModel.SharedItemTypeEncryptedFile || shareModelData.sharedItemType === ShareModel.SharedItemTypeEncryptedFolder || shareModelData.sharedItemType === ShareModel.SharedItemTypeEncryptedTopLevelFolder

    property bool waitingForNoteEnabledChange: false
    property bool waitingForExpireDateEnabledChange: false
    property bool waitingForPasswordProtectEnabledChange: false
    property bool waitingForExpireDateChange: false
    property bool waitingForLinkShareLabelChange: false
    property bool waitingForPasswordChange: false
    property bool waitingForNoteChange: false

    function showPasswordSetError(message) {
        passwordErrorBoxLoader.message = message !== "" ?
                                         message : qsTr("An error occurred setting the share password.");
    }

    function resetNoteField() {
        noteTextEdit.text = note;
        waitingForNoteChange = false;
    }

    function resetLinkShareLabelField() {
        linkShareLabelTextField.text = linkShareLabel;
        waitingForLinkShareLabelChange = false;
    }

    function resetPasswordField() {
        passwordTextField.text = password !== "" ? password : passwordPlaceholder;
        waitingForPasswordChange = false;
    }

    function resetExpireDateField() {
        // Expire date changing is handled through expireDateChanged listening in the expireDateSpinBox.
        //
        // When the user edits the expire date field they are changing the text, but the expire date
        // value is only changed according to updates from the server.
        //
        // Sometimes the new expire date is the same -- say, because we were on the maximum allowed
        // expire date already and we tried to push it beyond this, leading the server to just return
        // the maximum allowed expire date.
        //
        // So to ensure that the text of the spin box is correctly updated, force an update of the
        // contents of the expire date text field.
        expireDateField.updateText();
        waitingForExpireDateChange = false;
    }

    function resetNoteEnabledField() {
        noteEnabledMenuItem.checked = noteEnabled;
        waitingForNoteEnabledChange = false;
    }

    function resetExpireDateEnabledField() {
        expireDateEnabledMenuItem.checked = expireDateEnabled;
        waitingForExpireDateEnabledChange = false;
    }

    function resetPasswordProtectEnabledField() {
        passwordProtectEnabledMenuItem.checked = passwordProtectEnabled;
        waitingForPasswordProtectEnabledChange = false;
    }

    function resetMenu() {
        resetNoteField();
        resetPasswordField();
        resetLinkShareLabelField();
        resetExpireDateField();
        resetNoteEnabledField();
        resetExpireDateEnabledField();
        resetPasswordProtectEnabledField();
    }

    // Renaming a link share can lead to the model being reshuffled.
    // This can cause a situation where this delegate is assigned to
    // a new row and it doesn't have its properties signalled as
    // changed by the model, leading to bugs. We therefore reset all
    // the fields here when we detect the share has been changed
    onShareChanged: resetMenu()

    // Reset value after property binding broken by user interaction
    onNoteChanged: resetNoteField()
    onPasswordChanged: resetPasswordField()
    onLinkShareLabelChanged: resetLinkShareLabelField()
    onExpireDateChanged: resetExpireDateField()
    onNoteEnabledChanged: resetNoteEnabledField()
    onExpireDateEnabledChanged: resetExpireDateEnabledField()
    onPasswordProtectEnabledChanged: resetPasswordProtectEnabledField()

    padding: Style.standardSpacing * 2

    background: Rectangle {
        color: palette.window
        visible: root.backgroundsVisible
    }

    header: ColumnLayout {
        spacing: root.padding

        GridLayout {
            id: headerGridLayout

            Layout.fillWidth: parent
            Layout.topMargin: root.topPadding

            columns: 3
            rows: 2

            rowSpacing: Style.standardSpacing / 2
            columnSpacing: Style.standardSpacing

            Image {
                id: fileIcon

                Layout.rowSpan: headerGridLayout.rows
                Layout.preferredWidth: Style.trayListItemIconSize
                Layout.leftMargin: root.padding
                Layout.fillHeight: true

                verticalAlignment: Image.AlignVCenter
                horizontalAlignment: Image.AlignHCenter
                source: root.fileDetails.iconUrl
                sourceSize.width: Style.trayListItemIconSize
                sourceSize.height: Style.trayListItemIconSize
                fillMode: Image.PreserveAspectFit
            }

            EnforcedPlainTextLabel {
                id: headLabel

                Layout.fillWidth: true

                text: qsTr("Edit share")
                font.bold: true
                elide: Text.ElideRight
            }

            CustomButton {
                id: closeButton

                Layout.rowSpan: headerGridLayout.rows
                Layout.preferredWidth: Style.iconButtonWidth
                Layout.preferredHeight: width
                Layout.rightMargin: root.padding

                icon.source: "image://svgimage-custom-color/clear.svg" + "/" + palette.buttonText
                bgColor: palette.highlight
                bgNormalOpacity: 0
                toolTipText: qsTr("Dismiss")

                onClicked: root.closeShareDetails()
            }

            EnforcedPlainTextLabel {
                id: secondaryLabel

                Layout.fillWidth: true
                Layout.rightMargin: root.padding

                text: root.fileDetails.name
                color: palette.midlight
                wrapMode: Text.Wrap
            }
        }
    }

    contentItem: ScrollView {
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            id: scrollContentsColumn

            readonly property int rowIconWidth: Style.smallIconSize
            readonly property int indicatorItemWidth: 20
            readonly property int indicatorSpacing: Style.standardSpacing
            readonly property int itemPadding: Style.smallSpacing

            width: parent.width

            RowLayout {
                Layout.fillWidth: true
                height: visible ? implicitHeight : 0
                spacing: scrollContentsColumn.indicatorSpacing

                visible: root.isLinkShare

                Image {
                    Layout.preferredWidth: scrollContentsColumn.indicatorItemWidth
                    Layout.fillHeight: true

                    verticalAlignment: Image.AlignVCenter
                    horizontalAlignment: Image.AlignHCenter
                    fillMode: Image.Pad

                    source: "image://svgimage-custom-color/edit.svg/" + palette.dark
                    sourceSize.width: scrollContentsColumn.rowIconWidth
                    sourceSize.height: scrollContentsColumn.rowIconWidth
                }

                NCInputTextField {
                    id: linkShareLabelTextField

                    Layout.fillWidth: true
                    height: visible ? implicitHeight : 0

                    text: root.linkShareLabel
                    placeholderText: qsTr("Share label")

                    enabled: root.isLinkShare &&
                             !root.waitingForLinkShareLabelChange

                    onAccepted: if(text !== root.linkShareLabel) {
                        root.setLinkShareLabel(text);
                        root.waitingForLinkShareLabelChange = true;
                    }

                    NCBusyIndicator {
                        anchors.fill: parent
                        visible: root.waitingForLinkShareLabelChange
                        running: visible
                        z: 1
                    }
                }
            }

            Loader {
                Layout.fillWidth: true
                active: !root.isFolderItem && !root.isEncryptedItem
                visible: active
                sourceComponent: CheckBox {
                    // TODO: Rather than setting all these palette colours manually,
                    // create a custom style and do it for all components globally.
                    //
                    // Additionally, we need to override the entire palette when we
                    // set one palette property, as otherwise we default back to the
                    // theme palette -- not the parent palette
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
                        window: Style.menuBorder
                        base: Style.backgroundColor
                        toolTipBase: Style.backgroundColor
                        toolTipText: Style.ncTextColor
                    }

                    spacing: scrollContentsColumn.indicatorSpacing
                    padding: scrollContentsColumn.itemPadding
                    indicator.width: scrollContentsColumn.indicatorItemWidth
                    indicator.height: scrollContentsColumn.indicatorItemWidth

                    checkable: true
                    checked: root.editingAllowed
                    text: qsTr("Allow upload and editing")
                    enabled: !root.isSharePermissionChangeInProgress

                    onClicked: root.toggleAllowEditing(checked)

                    NCBusyIndicator {
                        anchors.fill: parent
                        visible: root.isSharePermissionChangeInProgress
                        running: visible
                        z: 1
                    }
                }
            }

            Loader {
                Layout.fillWidth: true
                active: root.isFolderItem && !root.isEncryptedItem
                visible: active
                sourceComponent: ColumnLayout {
                    id: permissionRadioButtonsLayout
                    spacing: 0
                    width: parent.width

                    ButtonGroup {
                        id: permissionModeRadioButtonsGroup
                    }

                    NCRadioButton {
                        readonly property int permissionMode: ShareModel.ModeViewOnly
                        Layout.fillWidth: true
                        ButtonGroup.group: permissionModeRadioButtonsGroup
                        enabled: !root.isSharePermissionChangeInProgress
                        checked: root.currentPermissionMode === permissionMode
                        text: qsTr("View only")
                        indicatorItemWidth: scrollContentsColumn.indicatorItemWidth
                        indicatorItemHeight: scrollContentsColumn.indicatorItemWidth
                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        onClicked: root.permissionModeChanged(permissionMode)
                    }

                    NCRadioButton {
                        readonly property int permissionMode: ShareModel.ModeUploadAndEditing
                        Layout.fillWidth: true
                        ButtonGroup.group: permissionModeRadioButtonsGroup
                        enabled: !root.isSharePermissionChangeInProgress
                        checked: root.currentPermissionMode === permissionMode
                        text: qsTr("Allow upload and editing")
                        indicatorItemWidth: scrollContentsColumn.indicatorItemWidth
                        indicatorItemHeight: scrollContentsColumn.indicatorItemWidth
                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        onClicked: root.permissionModeChanged(permissionMode)
                    }

                    NCRadioButton {
                        readonly property int permissionMode: ShareModel.ModeFileDropOnly
                        Layout.fillWidth: true
                        ButtonGroup.group: permissionModeRadioButtonsGroup
                        enabled: !root.isSharePermissionChangeInProgress
                        checked: root.currentPermissionMode === permissionMode
                        text: qsTr("File drop (upload only)")
                        indicatorItemWidth: scrollContentsColumn.indicatorItemWidth
                        indicatorItemHeight: scrollContentsColumn.indicatorItemWidth
                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        onClicked: root.permissionModeChanged(permissionMode)
                    }

                    CheckBox {
                        id: allowResharingCheckBox

                        Layout.fillWidth: true

                        // TODO: Rather than setting all these palette colours manually,
                        // create a custom style and do it for all components globally.
                        //
                        // Additionally, we need to override the entire palette when we
                        // set one palette property, as otherwise we default back to the
                        // theme palette -- not the parent palette
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
                            window: palette.dark // NOTE: Fusion theme uses darker window colour for the border of the checkbox
                            base: Style.backgroundColor
                            toolTipBase: Style.backgroundColor
                            toolTipText: Style.ncTextColor
                        }

                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        indicator.width: scrollContentsColumn.indicatorItemWidth
                        indicator.height: scrollContentsColumn.indicatorItemWidth

                        checkable: true
                        checked: root.resharingAllowed
                        text: qsTr("Allow resharing")
                        enabled: !root.isSharePermissionChangeInProgress && root.serverAllowsResharing
                        visible: root.serverAllowsResharing
                        onClicked: root.toggleAllowResharing(checked);

                        Connections {
                            target: root
                            onResharingAllowedChanged: allowResharingCheckBox.checked = root.resharingAllowed
                        }
                    }
                }

                NCBusyIndicator {
                    anchors.fill: parent
                    visible: root.isSharePermissionChangeInProgress
                    running: visible
                    z: 1
                }
            }

            Loader {
                Layout.fillWidth: true

                active: root.isLinkShare
                visible: active
                sourceComponent: ColumnLayout {
                    CheckBox {
                        id: hideDownloadEnabledMenuItem

                        anchors.left: parent.left
                        anchors.right: parent.right

                        // TODO: Rather than setting all these palette colours manually,
                        // create a custom style and do it for all components globally.
                        //
                        // Additionally, we need to override the entire palette when we
                        // set one palette property, as otherwise we default back to the
                        // theme palette -- not the parent palette
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
                            window: palette.dark // NOTE: Fusion theme uses darker window colour for the border of the checkbox
                            base: Style.backgroundColor
                            toolTipBase: Style.backgroundColor
                            toolTipText: Style.ncTextColor
                        }

                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        indicator.width: scrollContentsColumn.indicatorItemWidth
                        indicator.height: scrollContentsColumn.indicatorItemWidth

                        checked: root.hideDownload
                        text: qsTr("Hide download")
                        enabled: !root.isHideDownloadInProgress
                        onClicked: root.toggleHideDownload(checked);

                        NCBusyIndicator {
                            anchors.fill: parent
                            visible: root.isHideDownloadInProgress
                            running: visible
                            z: 1
                        }
                    }
                }
            }

            CheckBox {
                id: passwordProtectEnabledMenuItem

                Layout.fillWidth: true

                // TODO: Rather than setting all these palette colours manually,
                // create a custom style and do it for all components globally.
                //
                // Additionally, we need to override the entire palette when we
                // set one palette property, as otherwise we default back to the
                // theme palette -- not the parent palette
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
                    window: Style.menuBorder
                    base: Style.backgroundColor
                    toolTipBase: Style.backgroundColor
                    toolTipText: Style.ncTextColor
                }

                spacing: scrollContentsColumn.indicatorSpacing
                padding: scrollContentsColumn.itemPadding
                indicator.width: scrollContentsColumn.indicatorItemWidth
                indicator.height: scrollContentsColumn.indicatorItemWidth

                checkable: true
                checked: root.passwordProtectEnabled
                text: qsTr("Password protect")
                visible: root.shareSupportsPassword
                enabled: visible && 
                         !root.waitingForPasswordProtectEnabledChange && 
                         !root.passwordEnforced

                onClicked: {
                    root.togglePasswordProtect(checked);
                    root.waitingForPasswordProtectEnabledChange = true;
                }

                NCBusyIndicator {
                    anchors.fill: parent
                    visible: root.waitingForPasswordProtectEnabledChange
                    running: visible
                    z: 1
                }
            }

            RowLayout {
                Layout.fillWidth: true

                height: visible ? implicitHeight : 0
                spacing: scrollContentsColumn.indicatorSpacing

                visible: root.shareSupportsPassword && root.passwordProtectEnabled

                Image {
                    Layout.preferredWidth: scrollContentsColumn.indicatorItemWidth
                    Layout.fillHeight: true

                    verticalAlignment: Image.AlignVCenter
                    horizontalAlignment: Image.AlignHCenter
                    fillMode: Image.Pad

                    source: "image://svgimage-custom-color/lock-https.svg/" + palette.dark
                    sourceSize.width: scrollContentsColumn.rowIconWidth
                    sourceSize.height: scrollContentsColumn.rowIconWidth
                }

                NCInputTextField {
                    id: passwordTextField

                    Layout.fillWidth: true
                    height: visible ? implicitHeight : 0

                    text: root.password !== "" ? root.password : root.passwordPlaceholder
                    enabled: visible &&
                             root.passwordProtectEnabled &&
                             !root.waitingForPasswordChange &&
                             !root.waitingForPasswordProtectEnabledChange

                    onAccepted: if(text !== root.password && text !== root.passwordPlaceholder) {
                        passwordErrorBoxLoader.message = "";
                        root.setPassword(text);
                        root.waitingForPasswordChange = true;
                    }

                    NCBusyIndicator {
                        anchors.fill: parent
                        visible: root.waitingForPasswordChange ||
                                 root.waitingForPasswordProtectEnabledChange
                        running: visible
                        z: 1
                    }
                }
            }

            Loader {
                id: passwordErrorBoxLoader

                property string message: ""

                Layout.fillWidth: true
                height: message !== "" ? implicitHeight : 0

                active: message !== ""
                visible: active

                sourceComponent: Item {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    // Artificially add vertical padding
                    implicitHeight: passwordErrorBox.implicitHeight + (Style.smallSpacing * 2)

                    ErrorBox {
                        id: passwordErrorBox
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter

                        text: passwordErrorBoxLoader.message
                    }
                }
            }

            CheckBox {
                id: expireDateEnabledMenuItem

                Layout.fillWidth: true

                // TODO: Rather than setting all these palette colours manually,
                // create a custom style and do it for all components globally.
                //
                // Additionally, we need to override the entire palette when we
                // set one palette property, as otherwise we default back to the
                // theme palette -- not the parent palette
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
                    window: Style.menuBorder
                    base: Style.backgroundColor
                    toolTipBase: Style.backgroundColor
                    toolTipText: Style.ncTextColor
                }

                spacing: scrollContentsColumn.indicatorSpacing
                padding: scrollContentsColumn.itemPadding
                indicator.width: scrollContentsColumn.indicatorItemWidth
                indicator.height: scrollContentsColumn.indicatorItemWidth

                checkable: true
                checked: root.expireDateEnabled
                text: qsTr("Set expiration date")
                enabled: !root.waitingForExpireDateEnabledChange && !root.expireDateEnforced

                onClicked: {
                    root.toggleExpirationDate(checked);
                    root.waitingForExpireDateEnabledChange = true;
                }

                NCBusyIndicator {
                    anchors.fill: parent
                    visible: root.waitingForExpireDateEnabledChange
                    running: visible
                    z: 1
                }
            }

            RowLayout {
                Layout.fillWidth: true
                height: visible ? implicitHeight : 0
                spacing: scrollContentsColumn.indicatorSpacing

                visible: root.expireDateEnabled

                Image {
                    Layout.preferredWidth: scrollContentsColumn.indicatorItemWidth
                    Layout.fillHeight: true

                    verticalAlignment: Image.AlignVCenter
                    horizontalAlignment: Image.AlignHCenter
                    fillMode: Image.Pad

                    source: "image://svgimage-custom-color/calendar.svg/" + palette.dark
                    sourceSize.width: scrollContentsColumn.rowIconWidth
                    sourceSize.height: scrollContentsColumn.rowIconWidth
                }

                NCInputDateField {
                    id: expireDateField

                    Layout.fillWidth: true
                    height: visible ? implicitHeight : 0

                    dateInMs: root.expireDate
                    maximumDateMs: root.maximumExpireDate
                    minimumDateMs: {
                        const currentDate = new Date();
                        const currentYear = currentDate.getFullYear();
                        const currentMonth = currentDate.getMonth();
                        const currentMonthDay = currentDate.getDate();
                        // Start of day at 00:00:0000 UTC
                        return Date.UTC(currentYear, currentMonth, currentMonthDay + 1);
                    }

                    enabled: root.expireDateEnabled &&
                             !root.waitingForExpireDateChange &&
                             !root.waitingForExpireDateEnabledChange

                    onUserAcceptedDate: {
                        root.setExpireDate(dateInMs);
                        root.waitingForExpireDateChange = true;
                    }

                    NCBusyIndicator {
                        anchors.fill: parent
                        visible: root.waitingForExpireDateEnabledChange ||
                                 root.waitingForExpireDateChange
                        running: visible
                        z: 1
                    }
                }
            }

            CheckBox {
                id: noteEnabledMenuItem

                Layout.fillWidth: true

                // TODO: Rather than setting all these palette colours manually,
                // create a custom style and do it for all components globally.
                //
                // Additionally, we need to override the entire palette when we
                // set one palette property, as otherwise we default back to the
                // theme palette -- not the parent palette
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
                    window: Style.menuBorder
                    base: Style.backgroundColor
                    toolTipBase: Style.backgroundColor
                    toolTipText: Style.ncTextColor
                }

                spacing: scrollContentsColumn.indicatorSpacing
                padding: scrollContentsColumn.itemPadding
                indicator.width: scrollContentsColumn.indicatorItemWidth
                indicator.height: scrollContentsColumn.indicatorItemWidth

                checkable: true
                checked: root.noteEnabled
                text: qsTr("Note to recipient")
                enabled: !root.waitingForNoteEnabledChange

                onClicked: {
                    root.toggleNoteToRecipient(checked);
                    root.waitingForNoteEnabledChange = true;
                }

                NCBusyIndicator {
                    anchors.fill: parent
                    visible: root.waitingForNoteEnabledChange
                    running: visible
                    z: 1
                }
            }

            RowLayout {
                Layout.fillWidth: true
                height: visible ? implicitHeight : 0
                spacing: scrollContentsColumn.indicatorSpacing

                visible: root.noteEnabled

                Image {
                    Layout.preferredWidth: scrollContentsColumn.indicatorItemWidth
                    Layout.fillHeight: true

                    verticalAlignment: Image.AlignVCenter
                    horizontalAlignment: Image.AlignHCenter
                    fillMode: Image.Pad

                    source: "image://svgimage-custom-color/edit.svg/" + palette.dark
                    sourceSize.width: scrollContentsColumn.rowIconWidth
                    sourceSize.height: scrollContentsColumn.rowIconWidth
                }

                NCInputTextEdit {
                    id: noteTextEdit

                    Layout.fillWidth: true
                    height: visible ? Math.max(Style.talkReplyTextFieldPreferredHeight, contentHeight) : 0
                    submitButton.height: Math.min(Style.talkReplyTextFieldPreferredHeight, height - 2)

                    text: root.note
                    enabled: root.noteEnabled &&
                             !root.waitingForNoteChange &&
                             !root.waitingForNoteEnabledChange

                    onEditingFinished: if(text !== root.note) {
                        root.setNote(text);
                        root.waitingForNoteChange = true;
                    }

                    NCBusyIndicator {
                        anchors.fill: parent
                        visible: root.waitingForNoteChange ||
                                 root.waitingForNoteEnabledChange
                        running: visible
                        z: 1
                    }
                }
            }

            CustomButton {
                height: Style.standardPrimaryButtonHeight

                icon.source: "image://svgimage-custom-color/close.svg/" + Style.errorBoxBackgroundColor
                imageSourceHover: "image://svgimage-custom-color/close.svg/" + palette.brightText
                text: qsTr("Unshare")
                textColor: Style.errorBoxBackgroundColor
                textColorHovered: "white"
                contentsFont.bold: true
                bgNormalColor: palette.button
                bgHoverColor: Style.errorBoxBackgroundColor
                bgNormalOpacity: 1.0
                bgHoverOpacity: 1.0

                onClicked: root.deleteShare()
            }

            CustomButton {
                height: Style.standardPrimaryButtonHeight

                icon.source: "image://svgimage-custom-color/add.svg/" + root.accentColor
                imageSourceHover: "image://svgimage-custom-color/add.svg/" + palette.brightText
                text: qsTr("Add another link")
                textColor: root.accentColor
                textColorHovered: palette.brightText
                contentsFont.bold: true
                bgNormalColor: palette.button
                bgHoverColor: root.accentColor
                bgNormalOpacity: 1.0
                bgHoverOpacity: 1.0

                visible: root.isLinkShare && root.canCreateLinkShares
                enabled: visible

                onClicked: root.createNewLinkShare()
            }
        }
    }

    footer: DialogButtonBox {
        topPadding: 0
        bottomPadding: root.padding
        rightPadding: root.padding
        leftPadding: root.padding
        alignment: Qt.AlignRight | Qt.AlignVCenter
        contentWidth: (contentItem as ListView).contentWidth
        visible: copyShareLinkButton.visible

        CustomButton {
            id: copyShareLinkButton

            function copyShareLink() {
                clipboardHelper.text = root.link;
                clipboardHelper.selectAll();
                clipboardHelper.copy();
                clipboardHelper.clear();

                shareLinkCopied = true;
                shareLinkCopyTimer.start();
            }

            property bool shareLinkCopied: false

            height: Style.standardPrimaryButtonHeight

            icon.source: "image://svgimage-custom-color/copy.svg/" + palette.brightText
            text: shareLinkCopied ? qsTr("Share link copied!") : qsTr("Copy share link")
            textColor: palette.brightText
            contentsFont.bold: true
            bgColor: shareLinkCopied ? Style.positiveColor : root.accentColor
            bgNormalOpacity: 1.0
            bgHoverOpacity: shareLinkCopied ? 1.0 : Style.hoverOpacity

            visible: root.isLinkShare
            enabled: visible

            onClicked: copyShareLink()

            Behavior on bgColor {
                ColorAnimation { duration: Style.shortAnimationDuration }
            }

            Behavior on bgHoverOpacity {
                NumberAnimation { duration: Style.shortAnimationDuration }
            }

            Behavior on Layout.preferredWidth {
                SmoothedAnimation { duration: Style.shortAnimationDuration }
            }

            TextEdit {
                id: clipboardHelper
                visible: false
            }

            Timer {
                id: shareLinkCopyTimer
                interval: Style.veryLongAnimationDuration
                onTriggered: copyShareLinkButton.shareLinkCopied = false
            }
        }
    }
}
