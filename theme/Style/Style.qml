// SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later
pragma Singleton

import QtQuick

import com.nextcloud.desktopclient

QtObject {
    readonly property int pixelSize: fontMetrics.font.pixelSize
    readonly property bool darkMode: Theme.darkMode

    // Colors
    readonly property color ncBlue: Theme.wizardHeaderBackgroundColor
    readonly property color ncHeaderTextColor: Theme.wizardHeaderTitleColor
    readonly property color accentColor: UserModel.currentUser ? UserModel.currentUser.accentColor : ncBlue

    readonly property color currentUserHeaderColor: UserModel.currentUser ? UserModel.currentUser.headerColor : ncBlue
    readonly property color currentUserHeaderTextColor: UserModel.currentUser ? UserModel.currentUser.headerTextColor : ncHeaderTextColor
    readonly property color adjustedCurrentUserHeaderColor: Theme.darkMode ? Qt.lighter(currentUserHeaderColor, 2)
                                                                           : Qt.darker(currentUserHeaderColor, 1.5)

    // Account wizard colors
    readonly property color wizardWindowBackground: darkMode ? "#202124" : "#ffffff"
    readonly property color wizardPrimaryText: darkMode ? "#f1f3f4" : "#111111"
    readonly property color wizardSecondaryText: darkMode ? Qt.rgba(1, 1, 1, 0.68) : Qt.rgba(0, 0, 0, 0.62)
    readonly property color wizardPlaceholderText: darkMode ? Qt.rgba(1, 1, 1, 0.50) : Qt.rgba(0, 0, 0, 0.50)
    readonly property color wizardDisabledText: "#8a949c"
    readonly property color wizardFieldBackground: darkMode ? "#292a2d" : "#ffffff"
    readonly property color wizardFieldBorder: darkMode ? Qt.rgba(1, 1, 1, 0.24) : Qt.rgba(0, 0, 0, 0.24)
    readonly property color wizardRowBackground: darkMode ? "#292d30" : "#f5f8fa"
    readonly property color wizardRowBorder: darkMode ? "#3c454c" : "#e1e8ee"
    readonly property color wizardRowDisabledBackground: darkMode ? "#252729" : "#fafafa"
    readonly property color wizardRowDisabledBorder: darkMode ? "#343a3f" : "#edf1f4"
    readonly property color wizardSelectedBackground: darkMode ? "#263b4a" : "#eef5fb"
    readonly property color wizardSelectedBorder: darkMode ? "#365b73" : "#d8e7f1"
    readonly property color wizardRadioAccent: darkMode ? "#4da3d4" : "#0076b5"
    readonly property color wizardRadioDisabled: darkMode ? "#65727a" : "#b7c0c7"
    readonly property color wizardSecondaryButtonBackground: darkMode ? "#303b43" : "#e7eef4"
    readonly property color wizardSecondaryButtonPressed: darkMode ? "#394852" : "#dce8f0"
    readonly property color wizardSecondaryButtonBorder: darkMode ? "#4a5963" : "#d5e0e7"
    readonly property color wizardDisabledButtonBackground: darkMode ? "#292f33" : "#eef3f7"
    readonly property color wizardDisabledButtonBorder: darkMode ? "#39434a" : "#dde7ee"
    readonly property color wizardPrimaryButtonBackground: "#2b659a"
    readonly property color wizardPrimaryButtonPressed: "#245783"
    readonly property color wizardWarningBorder: darkMode ? "#d99832" : "#b36b00"
    readonly property color wizardWarningText: darkMode ? "#f2b84b" : "#8a5200"
    readonly property color wizardErrorBorder: darkMode ? "#e06c75" : "#d84b4b"
    readonly property color wizardErrorText: darkMode ? "#ff7b86" : "#b00020"
    readonly property color wizardAvatarPlaceholder: darkMode ? "#3b464d" : "#dfe8ee"
    readonly property color wizardSelectedText: "#ffffff"

    // ErrorBox colors
    readonly property color errorBoxBackgroundColor: Qt.rgba(0.89, 0.18, 0.18, 1)
    readonly property int errorBoxStripeWidth: 4

    // InfoBox colors
    readonly property color infoBoxBackgroundColor: Qt.rgba(0, 0.51, 0.79, 0.1)
    readonly property int infoBoxBorderWidth: 1
    readonly property color infoBoxBorderColor: Qt.rgba(0, 0.51, 0.79, 1)

    // Fonts
    // We are using pixel size because this is cross platform comparable, point size isn't
    readonly property int topLinePixelSize: pixelSize
    readonly property int subLinePixelSize: topLinePixelSize - 2
    readonly property int defaultFontPtSize: fontMetrics.font.pointSize
    readonly property int subheaderFontPtSize: defaultFontPtSize + 2
    readonly property int headerFontPtSize: defaultFontPtSize + 4
    readonly property int titleFontPtSize: defaultFontPtSize + 8

    // Dimensions and sizes
    property int trayWindowWidth: variableSize(400)
    property int trayWindowHeight: variableSize(510)
    // text input and main windows radius
    property int trayWindowRadius: 10
    // dropdown menus radius
    property int halfTrayWindowRadius: 5
    property int trayWindowBorderWidth: variableSize(1)
    property int trayWindowHeaderHeight: variableSize(50)
    property int trayHorizontalMargin: 10
    property int trayModalWidth: 380
    property int trayModalHeight: 490
    property int trayAccountPopupWidth: variableSize(300)
    property int trayAccountActionsMenuWidth: variableSize(340)
    property int trayAccountPopupRowHeight: variableSize(44)
    property int trayAccountPopupTopPadding: 4
    property int trayAccountPopupActionHeight: variableSize(26)
    property int trayAccountPopupPreviewActionHeight: variableSize(52)
    property int trayAccountPopupDetailedPreviewActionHeight: variableSize(58)
    property int trayAccountPopupCompactSeparatorHeight: variableSize(5)
    property int trayAccountPopupActionVerticalPadding: 8
    property int trayAccountPopupAvatarSize: variableSize(30)
    property int trayAccountPopupHoverMargin: 5
    property int trayAccountPopupAccountHoverVerticalMargin: 4
    property int trayAccountPopupHoverRadius: 5
    property int trayAccountPopupRowPadding: 12
    property int trayAccountPopupRowSpacing: 10
    property real trayAccountPopupRowHoverOpacity: 0.07
    property int trayAccountPopupHoverAnimationDuration: 80
    property int trayAccountPopupPrimaryFontSize: topLinePixelSize
    property int trayAccountPopupSecondaryFontSize: subLinePixelSize
    property int trayAccountPopupChevronFontSize: 18
    property int trayAccountPopupSyncIconSize: 16
    property int filesActionsWidth: 380
    property int filesActionsHeight: 350
    property int trayListItemIconSize: accountAvatarSize
    property int trayDrawerMargin: trayWindowHeaderHeight
    property real thumbnailImageSizeReduction: 0.2  // We reserve some space within the thumbnail "item", here about 20%.
                                                    // This is because we need to also add the added/modified icon and we
                                                    // want them to fit within the general icon size. We also need to know
                                                    // this amount to properly center the sync status icon to the thumbnail
                                                    // images, which will work so long as the thumbnails are left aligned

    property int standardSpacing: trayHorizontalMargin
    property int smallSpacing: 5
    property int extraSmallSpacing: 2
    property int extraExtraSmallSpacing: 1
    readonly property int fileProviderSettingsPadding: 12

    property int iconButtonWidth: 36
    property int standardPrimaryButtonHeight: 40
    readonly property int smallIconSize: 16
    readonly property int extraSmallIconSize: 8

    readonly property int wizardWindowMargin: 24
    readonly property int wizardWindowTopMargin: standardSpacing
    readonly property int wizardFooterButtonHeight: iconButtonWidth
    readonly property int wizardFooterSpacing: trayAccountPopupActionVerticalPadding
    readonly property int wizardSectionSpacing: trayAccountPopupRowPadding
    readonly property int wizardDialogMaximumWidth: 420
    readonly property int wizardDialogSpacing: wizardSectionSpacing + extraSmallSpacing
    readonly property int wizardDialogRadius: wizardSectionSpacing
    readonly property int wizardBodyFontPixelSize: pixelSize + extraSmallSpacing
    readonly property int wizardHeaderSpacing: trayAccountPopupActionVerticalPadding
    readonly property int wizardHeaderRowSpacing: trayAccountPopupRowSpacing
    readonly property int wizardHeaderLabelSpacing: extraExtraSmallSpacing
    readonly property int wizardHeaderAvatarSize: trayAccountPopupAvatarSize
    readonly property int wizardHeaderTitleFontPixelSize: pixelSize + trayAccountPopupActionVerticalPadding
    readonly property int wizardHeaderAccountNameFontPixelSize: topLinePixelSize
    readonly property int wizardHeaderAccountServerFontPixelSize: subLinePixelSize
    readonly property int wizardStandaloneWindowMinimumWidth: 520
    readonly property int wizardStandaloneWindowMinimumHeight: 420
    readonly property int activitiesWindowWidth: 680
    readonly property int activitiesWindowHeight: 700
    readonly property int assistantWindowWidth: 640
    readonly property int assistantWindowHeight: 620

    property int minActivityHeight: variableSize(32)

    property int minimumScrollBarWidth: 12
    property real minimumScrollBarThumbSize: 0
    property int currentAccountButtonWidth: 220
    property int currentAccountButtonRadius: 2
    property int currentAccountLabelWidth: 128

    property int normalBorderWidth: 1
    property int thickBorderWidth: 2
    property int veryRoundedButtonRadius: 100
    property int mediumRoundedButtonRadius: 8
    property int slightlyRoundedButtonRadius: 5
    property double hoverOpacity: 0.7

    property url stateOnlineImageSource: Theme.stateOnlineImageSource
    property url stateOfflineImageSource: Theme.stateOfflineImageSource

    property int accountAvatarSize: (trayWindowHeaderHeight - 16)
    property int accountAvatarStateIndicatorSize: 16
    property int folderStateIndicatorSize: 16
    property int accountLabelWidth: 128

    property int accountDropDownCaretSize: 10
    property int accountDropDownCaretMargin: 8

    property int trayFoldersMenuButtonStateIndicatorBottomOffset: 5
    property double trayFoldersMenuButtonDropDownCaretIconSizeFraction: 0.3
    property double trayFoldersMenuButtonMainIconSizeFraction: 1.0 - trayFoldersMenuButtonDropDownCaretIconSizeFraction

    property int activityListButtonWidth: 42
    property int activityListButtonHeight: 32
    property int activityListButtonIconSize: 18
    property int headerButtonIconSize: 48
    property int minimumActivityItemHeight: 24

    property int accountIconsMenuMargin: 7

    property int activityLabelBaseWidth: 240

    property int talkReplyTextFieldPreferredHeight: 34
    property int talkReplyTextFieldPreferredWidth: 250

    property int activityItemActionPrimaryButtonMinWidth: 100
    property int activityItemActionSecondaryButtonMinWidth: 80

    property int callNotificationPrimaryButtonMinWidth: 100
    property int callNotificationPrimaryButtonMinHeight: 40

    property int roundButtonBackgroundVerticalMargins: 10
    property int roundedButtonBackgroundVerticalMargins: 5

    property int userStatusEmojiSize: 8
    property int userStatusSpacing: trayHorizontalMargin
    property int userStatusAnchorsMargin: 2
    property int userLineSpacing: smallSpacing
    property int accountServerAnchorsMargin: 10
    property int accountLabelsSpacing: 4
    property int accountsServerMargin: 6
    property int accountLabelsAnchorsMargin: 5
    property int accountLabelsLayoutMargin: 12
    property int accountLabelsLayoutTopMargin: 10

    // Visual behaviour
    property bool hoverEffectsEnabled: true

    // unified search constants
    readonly property int unifiedSearchItemHeight: trayWindowHeaderHeight
    readonly property int unifiedSearchResultTextLeftMargin: 18
    readonly property int unifiedSearchResultTextRightMargin: 16
    readonly property int unifiedSearchResultIconWidth: trayListItemIconSize * (1 - thumbnailImageSizeReduction)
    readonly property int unifiedSearchResultSmallIconWidth: trayListItemIconSize * (1 - thumbnailImageSizeReduction * 2)
    readonly property int unifiedSearchResultIconLeftMargin: 12
    readonly property int unifiedSearchResultTitleFontSize: topLinePixelSize
    readonly property int unifiedSearchResultSublineFontSize: subLinePixelSize
    readonly property int unifiedSearchResultSectionItemLeftPadding: 16
    readonly property int unifiedSearchResultSectionItemVerticalPadding: 8
    readonly property int unifiedSearchResultNothingFoundHorizontalMargin: 10
    readonly property int unifiedSearchInputContainerHeight: 40
    readonly property int unifiedSearchPlaceholderViewTitleFontPixelSize: pixelSize * 1.25
    readonly property int unifiedSearchPlaceholderViewSublineFontPixelSize: subLinePixelSize * 1.25

    readonly property int radioButtonCustomMarginLeftInner: 4
    readonly property int radioButtonCustomMarginLeftOuter: 5
    readonly property int radioButtonCustomRadius: 9
    readonly property int radioButtonIndicatorSize: 16

    readonly property var fontMetrics: FontMetrics {}

    readonly property int bigFontPixelSizeResolveConflictsDialog: 20
    readonly property int fontPixelSizeResolveConflictsDialog: 15
    readonly property int minimumWidthResolveConflictsDialog: 600
    readonly property int minimumHeightResolveConflictsDialog: 300

    readonly property double smallIconScaleFactor: 0.6

    readonly property double trayFolderListButtonWidthScaleFactor: 1.75
    readonly property int trayFolderStatusIndicatorSizeOffset: 2
    readonly property double trayFolderStatusIndicatorRadiusFactor: 0.5
    readonly property double trayFolderStatusIndicatorMouseHoverOpacityFactor: 0.2

    readonly property double trayWindowMenuWidthFactor: 0.35

    readonly property int trayWindowMenuOffsetX: -2
    readonly property int trayWindowMenuOffsetY: 2

    readonly property int trayWindowMenuEntriesMargin: 6

    // animation durations
    readonly property int shortAnimationDuration: 200
    readonly property int veryLongAnimationDuration: 3000

    // sync status
    property int progressBarPreferredHeight: 9

    property int progressBarWidth: 100
    property int progressBarBackgroundHeight: 8
    property int progressBarContentHeight: 8
    property int progressBarRadius: 4
    property int progressBarContentBorderWidth: 1
    property int progressBarBackgroundBorderWidth: 1

    property int newActivitiesButtonWidth: 150
    property int newActivitiesButtonHeight: 40

    property real newActivitiesBgNormalOpacity: 0.8
    property real newActivitiesBgHoverOpacity: 1.0

    property int newActivityButtonDisappearTimeout: 5000
    property int newActivityButtonDisappearFadeTimeout: 250

    property int activityListScrollToTopTimerInterval: 50

    property int activityListScrollToTopVelocity: 10000

    function variableSize(size) {
        return size * (1 + Math.min(pixelSize / 100, 1));
    }

    // some platforms (e.g. Windows 11) have a transparency set on palette colours, this function removes that
    function colorWithoutTransparency(color) {
        return Qt.rgba(color.r, color.g, color.b, 1)
    }
}
