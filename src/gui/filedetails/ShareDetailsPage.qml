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

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import com.ionos.hidrivenext.desktopclient
import Style
import "../tray"
import "../SesComponents/"
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


    font.family: Style.sesOpenSansRegular
    font.pixelSize: Style.sesFontPixelSize
    font.weight: Style.sesFontNormalWeight

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

    property bool waitingForExpireDateEnabledChange: false
    property bool waitingForPasswordProtectEnabledChange: false
    property bool waitingForExpireDateChange: false
    property bool waitingForLinkShareLabelChange: false
    property bool waitingForPasswordChange: false
    property bool waitingForNoteChange: false

    readonly property int titlePixelSize: Style.sesFontPixelSizeTitle
    readonly property int titleFontWeight: Style.sesFontNormalWeight

    readonly property int hintPixelSize: Style.sesFontHintPixelSize
    readonly property int hintFontWeight: Style.sesFontNormalWeight


    readonly property int pixelSize: Style.sesFontPixelSize
    readonly property int fontWeight: Style.sesFontNormalWeight

    function showPasswordSetError(message) {
        passwordErrorBoxLoader.message = message !== "" ?
                                         message : qsTr("An error occurred setting the share password.");
    }

    function resetNoteField() {
        noteTextArea.text = note;
        waitingForNoteChange = false;
    }

    function resetLinkShareLabelField() {
        // linkShareLabelTextField.text = linkShareLabel;
        // waitingForLinkShareLabelChange = false;
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
        waitingForNoteChange = false;
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
        color: palette.base
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
                id: fileNameLabel

                Layout.fillWidth: true
                Layout.rightMargin: headerGridLayout.textRightMargin

                text: root.fileDetails.name

                font.pixelSize: titlePixelSize
                font.weight: titleFontWeight

                wrapMode: Text.Wrap
            }

            SesCustomButton {
                id: placeholder

                Layout.rowSpan: headerGridLayout.rows
                Layout.preferredWidth: Style.iconButtonWidth
                Layout.preferredHeight: width
                Layout.rightMargin: root.padding

                icon.source: "image://svgimage-custom-color/clear.svg" + "/" + palette.buttonText
                bgColor: palette.highlight
                bgNormalOpacity: 0
                toolTipText: qsTr("Dismiss")

                font.pixelSize: pixelSize
                font.weight: fontWeight


                onClicked: root.closeShareDetails()
            }

            EnforcedPlainTextLabel {
                id: fileDetailsLabel

                Layout.fillWidth: true
                Layout.rightMargin: headerGridLayout.textRightMargin

                text: `${root.fileDetails.sizeString}, ${root.fileDetails.lastChangedString}`
                wrapMode: Text.Wrap

                font.pixelSize: hintPixelSize
                font.weight: hintFontWeight
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


            CheckBox {
                id: passwordProtectEnabledMenuItem

                Layout.fillWidth: true

                spacing: scrollContentsColumn.indicatorSpacing
                padding: scrollContentsColumn.itemPadding
                indicator.width: scrollContentsColumn.indicatorItemWidth
                indicator.height: scrollContentsColumn.indicatorItemWidth

                checkable: true
                checked: root.passwordProtectEnabled
                text: qsTr("Password protection")
                visible: root.shareSupportsPassword
                enabled: visible && 
                         !root.waitingForPasswordProtectEnabledChange && 
                         !root.passwordEnforced

                onClicked: {
                    root.togglePasswordProtect(checked);
                    root.waitingForPasswordProtectEnabledChange = true;
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

                    SesErrorBox {
                        id: passwordErrorBox
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter

                        text: passwordErrorBoxLoader.message
                    }
                }
            }

            TextEdit {
                id: passwordTextEdit
                visible: root.passwordProtectEnabled
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: visible ? 64 : 0
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                text: root.password !== "" ? root.password : root.passwordPlaceholder

                font.family: root.font.family
                font.pixelSize: pixelSize
                font.weight: fontWeight

                padding: scrollContentsColumn.itemPadding
                enabled: visible &&
                         root.passwordProtectEnabled &&
                         !root.waitingForPasswordChange &&
                         !root.waitingForPasswordProtectEnabledChange

                onEditingFinished: if(text !== root.password && text !== root.passwordPlaceholder) {
                    passwordErrorBoxLoader.message = "";
                    root.setPassword(text);
                    root.waitingForPasswordChange = true;
                }

                Rectangle {
                    id: passwordTextBorder
                    anchors.fill: parent
                    radius: Style.slightlyRoundedButtonRadius
                    border.width: Style.thickBorderWidth
                    border.color: Style.sesTrayInputField
                    color: palette.base
                    z: -1
                }
            }

            CheckBox {
                id: expireDateEnabledMenuItem

                Layout.fillWidth: true
                font.pixelSize: pixelSize
                font.weight: fontWeight

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
            }

            NCInputDateField {
                id: expireDateField

                font.pixelSize: pixelSize
                font.weight: fontWeight

                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: visible ? implicitHeight : 0
                leftPadding: 15

                visible: root.expireDateEnabled

                selectByMouse: true

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

                Rectangle {
                    id: dateTextBorder
                    anchors.fill: parent
                    radius: Style.slightlyRoundedButtonRadius
                    border.width: Style.thickBorderWidth
                    border.color: Style.sesTrayInputField
                    color: palette.base
                    z: -1
                }
            }
            
            ColumnLayout {
                Layout.fillWidth: true
                height: visible ? implicitHeight : 0
                spacing: Style.extraSmallSpacing

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

                font.pixelSize: pixelSize
                font.weight: fontWeight


                spacing: scrollContentsColumn.indicatorSpacing
                    padding: scrollContentsColumn.itemPadding
                indicator.width: scrollContentsColumn.indicatorItemWidth
                indicator.height: scrollContentsColumn.indicatorItemWidth

                checkable: true
                checked: root.noteEnabled
                text: qsTr("Note to recipient")
                enabled: !root.waitingForNoteChange

                onClicked: {
                    if (!checked && root.note !== "") {
                        root.setNote("");
                        root.waitingForNoteChange = true;
                    }
                }
            }
            
            Text{
                    text: qsTr("Enter the note to recipient")
                    color: Style.sesGray
                    padding: scrollContentsColumn.itemPadding
                    visible: root.noteEnabled
                    font.family: root.font.family
                    font.pixelSize: pixelSize
                    font.weight: fontWeight
            }

                TextEdit {
                    id: noteTextEdit
                    visible: root.noteEnabled
                    font.family: root.font.family
                    font.pixelSize: pixelSize
                    font.weight: fontWeight
                    Layout.fillWidth: true
                    Layout.leftMargin: 3
                    Layout.rightMargin: 3
                    height: visible ? 64 : 0
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    padding: scrollContentsColumn.itemPadding
                    enabled: root.noteEnabled &&
                             !root.waitingForNoteChange &&
                             !root.waitingForNoteEnabledChange

                    onEditingFinished: if(text !== "") {
                        root.setNote(text);
                        root.waitingForNoteChange = true;
                    }

                    Rectangle {
                        id: noteTextBorder
                        anchors.fill: parent
                        radius: Style.slightlyRoundedButtonRadius
                        border.width: Style.thickBorderWidth
                        border.color: Style.sesTrayInputField
                        color: palette.base
                        z: -1
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

                    font.pixelSize: pixelSize
                    font.weight: fontWeight

                    spacing: scrollContentsColumn.indicatorSpacing
                    padding: scrollContentsColumn.itemPadding
                    indicator.width: scrollContentsColumn.indicatorItemWidth
                    indicator.height: scrollContentsColumn.indicatorItemWidth

                    checkable: true
                    checked: root.editingAllowed
                    text: qsTr("Allow upload and editing")
                    enabled: !root.isSharePermissionChangeInProgress

                    onClicked: root.toggleAllowEditing(checked)
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

                    CheckBox {
                        id: customPermissionsCheckBox
                        Layout.fillWidth: true
                        enabled: !root.isSharePermissionChangeInProgress
                        checked: root.currentPermissionMode === permissionMode
                        text: qsTr("Custom Permissions")
                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        indicator.width: scrollContentsColumn.indicatorItemWidth
                        indicator.height: scrollContentsColumn.indicatorItemWidth
                        onClicked: root.permissionModeChanged(permissionMode)
                        font.pixelSize: pixelSize
                        font.weight: fontWeight
                    }

                    CheckBox {
                        readonly property int permissionMode: ShareModel.ModeViewOnly
                        Layout.fillWidth: true
                        Layout.leftMargin: 30
                        ButtonGroup.group: permissionModeRadioButtonsGroup
                        enabled: !root.isSharePermissionChangeInProgress
                        checked: root.currentPermissionMode === permissionMode
                        text: qsTr("View only")
                        indicator.width: scrollContentsColumn.indicatorItemWidth
                        indicator.height: scrollContentsColumn.indicatorItemWidth
                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        onClicked: root.permissionModeChanged(permissionMode)
                        visible: customPermissionsCheckBox.checked
                        font.pixelSize: pixelSize
                        font.weight: fontWeight
                    }

                    CheckBox {
                        readonly property int permissionMode: ShareModel.ModeUploadAndEditing
                        Layout.fillWidth: true
                        Layout.leftMargin: 30
                        ButtonGroup.group: permissionModeRadioButtonsGroup
                        enabled: !root.isSharePermissionChangeInProgress
                        checked: root.currentPermissionMode === permissionMode
                        text: qsTr("Allow upload and editing")
                        indicator.width: scrollContentsColumn.indicatorItemWidth
                        indicator.height: scrollContentsColumn.indicatorItemWidth
                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        onClicked: root.permissionModeChanged(permissionMode)
                        visible: customPermissionsCheckBox.checked
                        font.pixelSize: pixelSize
                        font.weight: fontWeight
                    }

                    CheckBox {
                        readonly property int permissionMode: ShareModel.ModeFileDropOnly
                        Layout.fillWidth: true
                        Layout.leftMargin: 30
                        ButtonGroup.group: permissionModeRadioButtonsGroup
                        enabled: !root.isSharePermissionChangeInProgress
                        checked: root.currentPermissionMode === permissionMode
                        text: qsTr("File drop (upload only)")
                        indicator.width: scrollContentsColumn.indicatorItemWidth
                        indicator.height: scrollContentsColumn.indicatorItemWidth
                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        onClicked: root.permissionModeChanged(permissionMode)
                        visible: customPermissionsCheckBox.checked & false // Removed SES-307
                        font.pixelSize: pixelSize
                        font.weight: fontWeight
                    }
                }
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

                font.pixelSize: pixelSize
                font.weight: fontWeight

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

                        font.pixelSize: pixelSize
                        font.weight: fontWeight

                        spacing: scrollContentsColumn.indicatorSpacing
                        padding: scrollContentsColumn.itemPadding
                        indicator.width: scrollContentsColumn.indicatorItemWidth
                        indicator.height: scrollContentsColumn.indicatorItemWidth

                        checked: root.hideDownload
                        text: qsTr("Hide download")
                        enabled: !root.isHideDownloadInProgress
                        onClicked: root.toggleHideDownload(checked);
                    }
                }
            }
        }
    }

    footer: GridLayout {
        id: buttonGrid

        columns: 1
        rows: 2

        SesCustomButton {
            Layout.columnSpan: buttonGrid.columns

            icon.source: Style.sesDarkPlus

            font.pixelSize: pixelSize
            font.weight: fontWeight
            text: qsTr("Add another link")

            bgColor: Style.sesActionPressed
            bgNormalOpacity: 1.0
            bgHoverOpacity: Style.hoverOpacity

            visible: root.isLinkShare && root.canCreateLinkShares
            enabled: visible

            Layout.leftMargin: 16
            Layout.bottomMargin: 16
            Layout.row: 0

            onClicked: root.createNewLinkShare()
        }

        SesCustomButton {
            id: unshareButton

            font.pixelSize: pixelSize
            font.weight: fontWeight
            text: qsTr("Unshare")
            textColor: Style.sesActionPressed

            bgColor: palette.highlight
            bgNormalOpacity: 1.0

            bgBorderWidth: 2
            bgBorderColor: Style.sesActionPressed
            bgHoverOpacity: Style.hoverOpacity

            Layout.bottomMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 60
            Layout.row: 1
            onClicked: root.deleteShare()
        }

        SesCustomButton {
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

            icon.source: Style.sesClipboard

            font.pixelSize: pixelSize
            font.weight: fontWeight
            text: shareLinkCopied ? qsTr("Share link copied!") : qsTr("Copy share link")

            bgColor: Style.sesActionPressed
            bgNormalOpacity: 1.0
            bgHoverOpacity: shareLinkCopied ? 1.0 : Style.hoverOpacity

            visible: root.isLinkShare
            enabled: visible

            onClicked: copyShareLink()

            Layout.alignment: Qt.AlignRight
            Layout.bottomMargin: 16
            Layout.rightMargin: 20
            Layout.row: 1

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
