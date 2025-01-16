pragma Singleton

import QtQuick

import com.ionos.hidrivenext.desktopclient

QtObject {
    readonly property int pixelSize: fontMetrics.font.pixelSize
    readonly property bool darkMode: Theme.darkMode

    // Colors
    readonly property color ncBlue:      Theme.wizardHeaderBackgroundColor
    readonly property color ncHeaderTextColor: sesTrayFontColor
    readonly property color ncTextColor: sesTrayFontColor
    readonly property color ncTextBrightColor: "white"
    readonly property color ncSecondaryTextColor: sesTrayFontColor//"#808080"
    readonly property color lightHover: Theme.darkMode ? Qt.lighter(backgroundColor, 2) : Qt.darker(backgroundColor, 1.05)
    readonly property color darkerHover: Theme.darkMode ? Qt.lighter(backgroundColor, 2.35) : Qt.darker(backgroundColor, 1.25)
    readonly property color menuBorder: Theme.darkMode ? Qt.lighter(backgroundColor, 2.5) : Qt.darker(backgroundColor, 1.5)
    readonly property color backgroundColor: "#FFFFFF"
    readonly property color buttonBackgroundColor: Theme.systemPalette.button
    readonly property color positiveColor: Qt.rgba(0.38, 0.74, 0.38, 1)
    readonly property color accentColor: UserModel.currentUser ? UserModel.currentUser.accentColor : ncBlue

    readonly property color currentUserHeaderColor: UserModel.currentUser ? UserModel.currentUser.headerColor : ncBlue
    readonly property color currentUserHeaderTextColor: sesTrayFontColor
    readonly property color adjustedCurrentUserHeaderColor: Theme.darkMode ? Qt.lighter(currentUserHeaderColor, 2)
                                                                           : Qt.darker(currentUserHeaderColor, 1.5)

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

    property int iconButtonWidth: 36
    property int standardPrimaryButtonHeight: 40
    readonly property int smallIconSize: 16
    readonly property int extraSmallIconSize: 8

    property int minActivityHeight: variableSize(32)

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

    property int addAccountButtonHeight: 50

    property int headerButtonIconSize: sesIconSize
    property int dismissButtonSize: 26
    property int activityListButtonWidth: 42
    property int activityListButtonHeight: 32
    property int activityListButtonIconSize: 18
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
    property int accountLabelsAnchorsMargin: 7
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

    // SES
    readonly property string sesWebsiteIcon: "qrc:///client/theme/external.svg"
    readonly property string sesFilesIcon: "qrc:///client/theme/files.svg"
    readonly property string sesIonosLogoIcon: "qrc:///client/theme/ses/ses-IONOS-Logo.svg"

    readonly property string sesAvatar: "qrc:///client/theme/account.svg"

    readonly property string sesAccountQuit: "qrc:///client/theme/black/close.svg"
    readonly property string sesAccountPause: "qrc:///client/theme/colored/state-pause.svg"
    readonly property string sesDarkPlus: "qrc:///client/theme/black/add.svg"
    readonly property string sesLightPlus: "qrc:///client/theme/white/add.svg"
    readonly property string sesAccountSettings: "qrc:///client/theme/black/settings.svg"
    readonly property string sesAccountResume: "qrc:///client/theme/black/state-sync.svg"
    readonly property string sesLogout: "qrc:///client/theme/black/close.svg"
    readonly property string sesDelete: "qrc:///client/theme/delete.svg"
    readonly property string sesClipboard: "qrc:///client/theme/copy.svg"
    readonly property string sesErrorIcon: "qrc:///client/theme/colored/state-error.svg"
    readonly property string sesErrorBoxIcon: "qrc:///client/theme/colored/state-error.svg"
    readonly property string sesGreenCheckmark: "qrc:///client/theme/colored/state-ok.svg"
    readonly property string sesChevron: "qrc:///client/theme/black/caret-down.svg"

    readonly property color sesIconDarkColor: "#001B41"
    readonly property color sesIconColor: "#1474C4"

    readonly property color sesBorderColor: "#D7D7D7"
    readonly property color sesWhite: "#FFFFFF"
    readonly property color sesGray: "#465A75"
    readonly property color sesTrayInputField: "#718095"
    readonly property color sesHover: "#F2F5F8"
    readonly property color sesActionHover: "#1474C4"
    readonly property color sesActionPressed: "#0B2A63"
    readonly property color sesSelectedColor: "#F4F7FA"
    readonly property color sesButtonPressed: "#95CAEB"
    readonly property color sesAccountMenuHover: "#DBEDF8"
    readonly property color sesDarkGreen: "#096B35"
    readonly property color sesDarkBlue: "#001B41"
    readonly property color sesTrayFontColor: "#001B41"
    readonly property color sesErrorBoxBorder: "#F50C00"
    readonly property color sesErrorBoxText: "#C80A00"
    readonly property color sesMenuBorder: "#2E4360"
    readonly property color sesSearchFieldContent: "#97A3B4"

    property int sesAccountMenuHeight: variableSize(40)
    property int sesHeaderLogoHeigth: variableSize(40)
    property int sesHeaderLogoTopMargin: variableSize(12)
    property int sesHeaderLogoLeftMargin: variableSize(24)
    property int sesCornerRadius: 8
    property int sesHeaderTopMargin: variableSize(10)
    property int sesSmallMargin: 8
    property int sesAccountMenuItemPadding: 12
    property int sesMediumMargin: 16

    readonly property string sesOpenSansRegular: "qrc:///client/fonts/OpenSans-Regular.ttf"
    property int sesFontPointSize: 9
    property int sesFontPixelSizeTitle: 20
    property int sesFontPixelSize: 16
    property int sesFontErrortextPixelSize: 14
    property int sesFontHintPixelSize: 12
    property int sesFontBoldWeight: 400
    property int sesFontNormalWeight: sesFontBoldWeight

    property int sesIconSize: 24
    property int sesActivityItemDistanceToFrame: 24
    property int sesActivityItemWidthModifier: 26
    property int sesFileDetailsIconSize: 58
    property int sesFileDetailsHeaderModifier: 100
    property int sesSearchFieldHeight: 40

    //Tray Account Menu Values
    property int sesAccountButtonWidth: 256
    property int sesAccountButtonHeight: 68
    property int sesAccountButtonRightMargin: 33
    property int sesAccountButtonLeftMargin: 19
    property int sesHeaderButtonWidth: 84
    property int sesHeaderButtonHeight: 68
    property int sesAccountMenuWidth: sesAccountButtonWidth - 8
    property int sesAccountLabelWidth: 157
    property int sesTrayHeaderMargin: 11
    property int sesTrayWindowWidth: 464
}
