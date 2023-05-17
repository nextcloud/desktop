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

    property FileDetails fileDetails: FileDetails {}
    property var shareModelData: ({})

    property bool canCreateLinkShares: true

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
    readonly property var maximumExpireDate: shareModelData.enforcedMaximumExpireDate

    readonly property string linkShareLabel: shareModelData.linkShareLabel ?? ""

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
        // Expire date changing is handled by the expireDateSpinBox
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
        moreMenu.close();

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

    // TODO: Rather than setting all these palette colours manually,
    // create a custom style and do it for all components globally
    palette {
        text: Style.ncTextColor
        windowText: Style.ncTextColor
        buttonText: Style.ncTextColor
        light: Style.lightHover
        midlight: Style.lightHover
        mid: Style.ncSecondaryTextColor
        dark: Style.menuBorder
        button: Style.menuBorder
        window: Style.backgroundColor
        base: Style.backgroundColor
    }

    background: Rectangle {
        color: Style.backgroundColor
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
                color: Style.ncTextColor
                font.bold: true
                elide: Text.ElideRight
            }

            CustomButton {
                id: closeButton

                Layout.rowSpan: headerGridLayout.rows
                Layout.preferredWidth: Style.iconButtonWidth
                Layout.preferredHeight: width
                Layout.rightMargin: root.padding

                icon.source: "image://svgimage-custom-color/clear.svg" + "/" + Style.ncTextColor
                bgColor: Style.lightHover
                bgNormalOpacity: 0
                toolTipText: qsTr("Dismiss")

                onClicked: root.closeShareDetails()
            }

            EnforcedPlainTextLabel {
                id: secondaryLabel

                Layout.fillWidth: true
                Layout.rightMargin: root.padding

                text: root.fileDetails.name
                color: Style.ncSecondaryTextColor
                wrapMode: Text.Wrap
            }
        }
    }

    contentItem: ScrollView {
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            id: moreMenu

            property int rowIconWidth: 16
            property int indicatorItemWidth: 20
            property int indicatorSpacing: Style.standardSpacing
            property int itemPadding: Style.smallSpacing

            width: parent.width

            RowLayout {
                Layout.fillWidth: true
                height: visible ? implicitHeight : 0
                spacing: moreMenu.indicatorSpacing

                visible: root.isLinkShare

                Image {
                    Layout.preferredWidth: moreMenu.indicatorItemWidth
                    Layout.fillHeight: true

                    verticalAlignment: Image.AlignVCenter
                    horizontalAlignment: Image.AlignHCenter
                    fillMode: Image.Pad

                    source: "image://svgimage-custom-color/edit.svg/" + Style.menuBorder
                    sourceSize.width: moreMenu.rowIconWidth
                    sourceSize.height: moreMenu.rowIconWidth
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
                    spacing: moreMenu.indicatorSpacing
                    padding: moreMenu.itemPadding
                    indicator.width: moreMenu.indicatorItemWidth
                    indicator.height: moreMenu.indicatorItemWidth

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
                        indicatorItemWidth: moreMenu.indicatorItemWidth
                        indicatorItemHeight: moreMenu.indicatorItemWidth
                        spacing: moreMenu.indicatorSpacing
                        padding: moreMenu.itemPadding
                        onClicked: root.permissionModeChanged(permissionMode)
                    }

                    NCRadioButton {
                        readonly property int permissionMode: ShareModel.ModeUploadAndEditing
                        Layout.fillWidth: true
                        ButtonGroup.group: permissionModeRadioButtonsGroup
                        enabled: !root.isSharePermissionChangeInProgress
                        checked: root.currentPermissionMode === permissionMode
                        text: qsTr("Allow upload and editing")
                        indicatorItemWidth: moreMenu.indicatorItemWidth
                        indicatorItemHeight: moreMenu.indicatorItemWidth
                        spacing: moreMenu.indicatorSpacing
                        padding: moreMenu.itemPadding
                        onClicked: root.permissionModeChanged(permissionMode)

                        NCBusyIndicator {
                            anchors.fill: parent
                            visible: root.isSharePermissionChangeInProgress
                            running: visible
                            z: 1
                        }
                    }

                    NCRadioButton {
                        readonly property int permissionMode: ShareModel.ModeFileDropOnly
                        Layout.fillWidth: true
                        ButtonGroup.group: permissionModeRadioButtonsGroup
                        enabled: !root.isSharePermissionChangeInProgress
                        checked: root.currentPermissionMode === permissionMode
                        text: qsTr("File drop (upload only)")
                        indicatorItemWidth: moreMenu.indicatorItemWidth
                        indicatorItemHeight: moreMenu.indicatorItemWidth
                        spacing: moreMenu.indicatorSpacing
                        padding: moreMenu.itemPadding
                        onClicked: root.permissionModeChanged(permissionMode)
                    }
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

                        spacing: moreMenu.indicatorSpacing
                        padding: moreMenu.itemPadding
                        indicator.width: moreMenu.indicatorItemWidth
                        indicator.height: moreMenu.indicatorItemWidth
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

                spacing: moreMenu.indicatorSpacing
                padding: moreMenu.itemPadding
                indicator.width: moreMenu.indicatorItemWidth
                indicator.height: moreMenu.indicatorItemWidth

                checkable: true
                checked: root.passwordProtectEnabled
                text: qsTr("Password protect")
                enabled: !root.waitingForPasswordProtectEnabledChange && !root.passwordEnforced

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
                spacing: moreMenu.indicatorSpacing

                visible: root.passwordProtectEnabled

                Image {
                    Layout.preferredWidth: moreMenu.indicatorItemWidth
                    Layout.fillHeight: true

                    verticalAlignment: Image.AlignVCenter
                    horizontalAlignment: Image.AlignHCenter
                    fillMode: Image.Pad

                    source: "image://svgimage-custom-color/lock-https.svg/" + Style.menuBorder
                    sourceSize.width: moreMenu.rowIconWidth
                    sourceSize.height: moreMenu.rowIconWidth
                }

                NCInputTextField {
                    id: passwordTextField

                    Layout.fillWidth: true
                    height: visible ? implicitHeight : 0

                    text: root.password !== "" ? root.password : root.passwordPlaceholder
                    enabled: root.passwordProtectEnabled &&
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

                spacing: moreMenu.indicatorSpacing
                padding: moreMenu.itemPadding
                indicator.width: moreMenu.indicatorItemWidth
                indicator.height: moreMenu.indicatorItemWidth

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
                spacing: moreMenu.indicatorSpacing

                visible: root.expireDateEnabled

                Image {
                    Layout.preferredWidth: moreMenu.indicatorItemWidth
                    Layout.fillHeight: true

                    verticalAlignment: Image.AlignVCenter
                    horizontalAlignment: Image.AlignHCenter
                    fillMode: Image.Pad

                    source: "image://svgimage-custom-color/calendar.svg/" + Style.menuBorder
                    sourceSize.width: moreMenu.rowIconWidth
                    sourceSize.height: moreMenu.rowIconWidth
                }

                // QML dates are essentially JavaScript dates, which makes them very finicky and unreliable.
                // Instead, we exclusively deal with msecs from epoch time to make things less painful when editing.
                // We only use the QML Date when showing the nice string to the user.
                SpinBox {
                    id: expireDateSpinBox

                    // Work arounds the limitations of QML's 32 bit integer when handling msecs from epoch
                    // Instead, we handle everything as days since epoch
                    readonly property int dayInMSecs: 24 * 60 * 60 * 1000
                    readonly property int expireDateReduced: Math.floor(root.expireDate / dayInMSecs)
                    // Reset the model data after binding broken on user interact
                    onExpireDateReducedChanged: value = expireDateReduced

                    // We can't use JS's convenient Infinity or Number.MAX_VALUE as
                    // JS Number type is 64 bits, whereas QML's int type is only 32 bits
                    readonly property IntValidator intValidator: IntValidator {}
                    readonly property int maximumExpireDateReduced: root.expireDateEnforced ?
                                                                        Math.floor(root.maximumExpireDate / dayInMSecs) :
                                                                        intValidator.top
                    readonly property int minimumExpireDateReduced: {
                        const currentDate = new Date();
                        const minDateUTC = new Date(Date.UTC(currentDate.getFullYear(),
                                                             currentDate.getMonth(),
                                                             currentDate.getDate() + 1));
                        return Math.floor(minDateUTC / dayInMSecs) // Start of day at 00:00:0000 UTC
                    }

                    // Taken from Kalendar 22.08
                    // https://invent.kde.org/pim/kalendar/-/blob/release/22.08/src/contents/ui/KalendarUtils/dateutils.js
                    function parseDateString(dateString) {
                        function defaultParse() {
                            const defaultParsedDate = Date.fromLocaleDateString(Qt.locale(), dateString, Locale.NarrowFormat);
                            // JS always generates date in system locale, eliminate timezone difference to UTC
                            const msecsSinceEpoch = defaultParsedDate.getTime() - (defaultParsedDate.getTimezoneOffset() * 60 * 1000);
                            return new Date(msecsSinceEpoch);
                        }

                        const dateStringDelimiterMatches = dateString.match(/\D/);
                        if(dateStringDelimiterMatches.length === 0) {
                            // Let the date method figure out this weirdness
                            return defaultParse();
                        }

                        const dateStringDelimiter = dateStringDelimiterMatches[0];

                        const localisedDateFormatSplit = Qt.locale().dateFormat(Locale.NarrowFormat).split(dateStringDelimiter);
                        const localisedDateDayPosition = localisedDateFormatSplit.findIndex((x) => /d/gi.test(x));
                        const localisedDateMonthPosition = localisedDateFormatSplit.findIndex((x) => /m/gi.test(x));
                        const localisedDateYearPosition = localisedDateFormatSplit.findIndex((x) => /y/gi.test(x));

                        let splitDateString = dateString.split(dateStringDelimiter);
                        let userProvidedYear = splitDateString[localisedDateYearPosition]

                        const dateNow = new Date();
                        const stringifiedCurrentYear = dateNow.getFullYear().toString();

                        // If we have any input weirdness, or if we have a fully-written year
                        // (e.g. 2022 instead of 22) then use default parse
                        if(splitDateString.length === 0 ||
                                splitDateString.length > 3 ||
                                userProvidedYear.length >= stringifiedCurrentYear.length) {

                            return defaultParse();
                        }

                        let fullyWrittenYear = userProvidedYear.split("");
                        const digitsToAdd = stringifiedCurrentYear.length - fullyWrittenYear.length;
                        for(let i = 0; i < digitsToAdd; i++) {
                            fullyWrittenYear.splice(i, 0, stringifiedCurrentYear[i])
                        }
                        fullyWrittenYear = fullyWrittenYear.join("");

                        const fixedYearNum = Number(fullyWrittenYear);
                        const monthIndexNum = Number(splitDateString[localisedDateMonthPosition]) - 1;
                        const dayNum = Number(splitDateString[localisedDateDayPosition]);

                        console.log(dayNum, monthIndexNum, fixedYearNum);

                        // Modification: return date in UTC
                        return new Date(Date.UTC(fixedYearNum, monthIndexNum, dayNum));
                    }

                    Layout.fillWidth: true
                    height: visible ? implicitHeight : 0


                    // We want all the internal benefits of the spinbox but don't actually want the
                    // buttons, so set an empty item as a dummy
                    up.indicator: Item {}
                    down.indicator: Item {}

                    padding: 0
                    background: null
                    contentItem: NCInputTextField {
                        text: expireDateSpinBox.textFromValue(expireDateSpinBox.value, expireDateSpinBox.locale)
                        readOnly: !expireDateSpinBox.editable
                        validator: expireDateSpinBox.validator
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        onAccepted: {
                            expireDateSpinBox.value = expireDateSpinBox.valueFromText(text, expireDateSpinBox.locale);
                            expireDateSpinBox.valueModified();
                        }
                    }

                    value: expireDateReduced
                    from: minimumExpireDateReduced
                    to: maximumExpireDateReduced

                    textFromValue: (value, locale) => {
                        const dateFromValue = new Date(value * dayInMSecs);
                        return dateFromValue.toLocaleDateString(Qt.locale(), Locale.NarrowFormat);
                    }
                    valueFromText: (text, locale) => {
                        const dateFromText = parseDateString(text);
                        return Math.floor(dateFromText.getTime() / dayInMSecs);
                    }

                    editable: true
                    inputMethodHints: Qt.ImhDate | Qt.ImhFormattedNumbersOnly

                    enabled: root.expireDateEnabled &&
                             !root.waitingForExpireDateChange &&
                             !root.waitingForExpireDateEnabledChange

                    onValueModified: {
                        if (!enabled || !activeFocus) {
                            return;
                        }

                        root.setExpireDate(value * dayInMSecs);
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

                spacing: moreMenu.indicatorSpacing
                padding: moreMenu.itemPadding
                indicator.width: moreMenu.indicatorItemWidth
                indicator.height: moreMenu.indicatorItemWidth

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
                spacing: moreMenu.indicatorSpacing

                visible: root.noteEnabled

                Image {
                    Layout.preferredWidth: moreMenu.indicatorItemWidth
                    Layout.fillHeight: true

                    verticalAlignment: Image.AlignVCenter
                    horizontalAlignment: Image.AlignHCenter
                    fillMode: Image.Pad

                    source: "image://svgimage-custom-color/edit.svg/" + Style.menuBorder
                    sourceSize.width: moreMenu.rowIconWidth
                    sourceSize.height: moreMenu.rowIconWidth
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
                imageSourceHover: "image://svgimage-custom-color/close.svg/" + Style.ncHeaderTextColor
                text: qsTr("Unshare")
                textColor: Style.errorBoxBackgroundColor
                textColorHovered: "white"
                contentsFont.bold: true
                bgNormalColor: Style.buttonBackgroundColor
                bgHoverColor: Style.errorBoxBackgroundColor
                bgNormalOpacity: 1.0
                bgHoverOpacity: 1.0

                onClicked: root.deleteShare()
            }

            CustomButton {
                height: Style.standardPrimaryButtonHeight

                icon.source: "image://svgimage-custom-color/add.svg/" + Style.ncBlue
                imageSourceHover: "image://svgimage-custom-color/add.svg/" + Style.ncHeaderTextColor
                text: qsTr("Add another link")
                textColor: Style.ncBlue
                textColorHovered: Style.ncHeaderTextColor
                contentsFont.bold: true
                bgNormalColor: Style.buttonBackgroundColor
                bgHoverColor: Style.ncBlue
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

            icon.source: "image://svgimage-custom-color/copy.svg/" + Style.ncHeaderTextColor
            text: shareLinkCopied ? qsTr("Share link copied!") : qsTr("Copy share link")
            textColor: Style.ncHeaderTextColor
            contentsFont.bold: true
            bgColor: shareLinkCopied ? Style.positiveColor : Style.ncBlue
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
