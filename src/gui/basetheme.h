#ifndef _BASETHEME_H
#define _BASETHEME_H

#include <QFont>
#include <QString>
#include "theme.h"

namespace OCC {

class BaseTheme : public QObject{
    Q_OBJECT
    Q_PROPERTY(QString dialogBackgroundColor READ dialogBackgroundColor CONSTANT)
    Q_PROPERTY(QString trayFontColor READ trayFontColor CONSTANT)
    Q_PROPERTY(QString trayBorderColor READ trayBorderColor CONSTANT)
    Q_PROPERTY(QString trayInputFieldBorderColor READ trayInputFieldBorderColor CONSTANT)
    Q_PROPERTY(QString trayBackgroundColor READ trayBackgroundColor CONSTANT)
    Q_PROPERTY(QString iconDarkColor READ iconDarkColor CONSTANT)
    Q_PROPERTY(QString buttonIconColor READ buttonIconColor CONSTANT)
    Q_PROPERTY(QString buttonHoveredColor READ buttonHoveredColor CONSTANT)
    Q_PROPERTY(QString buttonPressedColor READ buttonPressedColor CONSTANT)
    Q_PROPERTY(QString toolButtonHoveredColor READ toolButtonHoveredColor CONSTANT)
    Q_PROPERTY(QString toolButtonPressedColor READ toolButtonPressedColor CONSTANT)
    Q_PROPERTY(QString pillButtonPrimaryColor READ pillButtonPrimaryColor CONSTANT)
    Q_PROPERTY(QString pillButtonSecondaryColor READ pillButtonSecondaryColor CONSTANT)
    Q_PROPERTY(QString pillButtonBorderColor READ pillButtonBorderColor CONSTANT)
    Q_PROPERTY(QString clipboardBackgroundColor READ clipboardBackgroundColor CONSTANT)
    Q_PROPERTY(QString trayErrorBorderColor READ trayErrorBorderColor CONSTANT)
    Q_PROPERTY(QString trayErrorTextColor READ trayErrorTextColor CONSTANT)
    Q_PROPERTY(QString sesHeaderLogoIcon READ sesHeaderLogoIcon CONSTANT)
    Q_PROPERTY(QString websiteIcon READ websiteIcon CONSTANT)
    Q_PROPERTY(QString folderIcon READ folderIcon CONSTANT)
    Q_PROPERTY(QString moreIcon READ moreIcon CONSTANT)
    Q_PROPERTY(QString moreHoverIcon READ moreHoverIcon CONSTANT)
    Q_PROPERTY(QString avatarIcon READ avatarIcon CONSTANT)
    Q_PROPERTY(QString plusIcon READ plusIcon CONSTANT)
    Q_PROPERTY(QString lightPlusIcon READ lightPlusIcon CONSTANT)
    Q_PROPERTY(QString quitIcon READ quitIcon CONSTANT)
    Q_PROPERTY(QString resumeIcon READ resumeIcon CONSTANT)
    Q_PROPERTY(QString pauseIcon READ pauseIcon CONSTANT)
    Q_PROPERTY(QString settingsIcon READ settingsIcon CONSTANT)
    Q_PROPERTY(QString logoutIcon READ logoutIcon CONSTANT)
    Q_PROPERTY(QString deleteIcon READ deleteIcon CONSTANT)
    Q_PROPERTY(QString clipboardIcon READ clipboardIcon CONSTANT)
    Q_PROPERTY(QString lightClipboardIcon READ lightClipboardIcon CONSTANT)
    Q_PROPERTY(QString chevronIcon READ chevronIcon CONSTANT)
    Q_PROPERTY(QString syncSuccessIcon READ syncSuccessIcon CONSTANT)
    Q_PROPERTY(QString syncErrorIcon READ syncErrorIcon CONSTANT)
    Q_PROPERTY(QString syncOfflineIcon READ syncOfflineIcon CONSTANT)
    Q_PROPERTY(QString snackbarErrorIcon READ snackbarErrorIcon CONSTANT)
    Q_PROPERTY(QString activityIcon READ activityIcon CONSTANT)

public:

    virtual ~BaseTheme() = default;

    virtual QString themePrefix(QString context = "qml") const {
        if (context == "qml") {
            return QString("qrc:///client/theme/");
        }
        return QString(Theme::themePrefix);
    }

    virtual QString additionalThemePrefix() const { return QStringLiteral(""); }

    virtual QString avatarIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-settingsAvatar.svg");
    }

    virtual QString roundAvatarIcon() const {
        return QString(Theme::themePrefix) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-settingsAvatarRound.svg");
    }

    virtual QString folderIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-folderIcon.svg");
    }

    virtual QString syncArrows() const {
        return QString(Theme::themePrefix) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-syncArrows.svg");
    }

    virtual QString questionCircleIcon() const {
        return QString(Theme::themePrefix) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-questionMark.svg");
    }

    virtual QString liveBackupPlusIcon() const {
        return QString(Theme::themePrefix) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-addlivebackup.svg");
    }

    virtual QString websiteIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-website.svg");
    }

    virtual QString moreIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-more.svg");
    }

    virtual QString moreHoverIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-more-hover.svg");
    }

    virtual QString plusIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-darkPlus.svg");
    }

    virtual QString lightPlusIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-lightPlus.svg");
    }

    virtual QString quitIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-accountQuit.svg");
    }

    virtual QString resumeIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-accountResume.svg");
    }

    virtual QString pauseIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-accountPause.svg");
    }

    virtual QString settingsIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-settings.svg");
    }

    virtual QString logoutIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-accountLogout.svg");
    }
    
    virtual QString sesHeaderLogoIcon() const = 0;

    virtual QString deleteIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-accountDelete.svg");
    }

    virtual QString activityDeleteIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-activityDelete.svg");
    }

    virtual QString refreshIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-refresh.svg");
    }

    virtual QString infoIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-info.svg");
    }

    virtual QString clipboardIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-clipboard.svg");
    }

    virtual QString lightClipboardIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-lightClipboard.svg");
    }

    virtual QString chevronIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-chevron.svg");
    }

    virtual QString syncSuccessIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-syncstate-success.svg");
    }

    virtual QString syncErrorIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-syncstate-error.svg");
    }

    virtual QString syncOfflineIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-state-offline.svg");
    }

    virtual QString snackbarErrorIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-snackbar-error.svg");
    }

    virtual QString activityIcon() const {
        return themePrefix() + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-activity.svg");
    }

    virtual int treeViewIconSize() const {
        return 32;
    }

    //Control Configuration: Sizes
    virtual QString toolbarActionBorderRadius() const {
        return "8px";
    }

    virtual QString toolbarSideMargin() const {
        return "10px";
    }

    virtual int toolbarIconSize() const {
        return 24;
    }

    virtual QString buttonRadius() const {
        return "4px";
    }

    virtual int buttonRadiusInt() const {
        return 4;
    }

    virtual QString buttonPadding() const {
        return "10px";
    }

    virtual QString smallMargin() const {
        return "8";
    }

    virtual int minimalSettingsDialogWidth() const {
        return 740;
    }

    virtual int wizardFixedWidth() const {
        return 576;
    }

    virtual int wizardFixedHeight() const {
        return 704;
    }

    virtual int LoginPageSpacer() const {
        return 45;
    }

    //Font Configuration
    virtual QString settingsFont() const {
        return "Segoe UI";
    }

    virtual QString contextMenuFont() const {
        //TODO
        return ":/client/fonts/OpenSans-Regular.ttf";
    }

    virtual QString settingsSmallTextSize() const {
        return "14px";
    }

    virtual int settingsTextPixel() const {
        return 16;
    }

    virtual QString settingsTextSize() const {
        return QString::number(settingsTextPixel()) + "px";
    }

    virtual int settingsTitlePixel() const {
        return 20;
    }

    virtual QString settingsTitleSize() const {
        return QString::number(settingsTitlePixel()) + "px";
    }

    virtual int settingsBigTitlePixel() const {
        return 24;
    }

    virtual QString settingsBigTitleSize() const {
        return QString::number(settingsBigTitlePixel()) + "px";
    }

    virtual QString onboardingTitle() const {
        return "28px";
    }

    virtual QString settingsTextWeight() const {
        return "400";
    }

    virtual QString settingsTitleWeight400() const {
        return "400";
    }

    virtual QString settingsTitleWeight500() const {
        return "500";
    }

    virtual QString settingsTitleWeight600() const {
        return "600";
    }

    virtual QFont::Weight settingsTitleWeightDemiBold() const {
        return QFont::DemiBold;
    }

    virtual QFont::Weight settingsTitleWeightNormal() const {
        return QFont::Normal;
    }

    virtual QFont settingsFontDefault() const {
        QFont defaultFont(settingsFont());
        defaultFont.setPixelSize(settingsTextPixel());
        defaultFont.setWeight(settingsTitleWeightNormal());
        return defaultFont;
    }

    virtual QString titleColor() const {
        return "#000000";
    }

    virtual QString folderWizardSubtitleColor() const {
        return "#104996";
    }

    virtual QString folderWizardPathColor() const {
        return "#97A3B4";
    }

    virtual QString loginWizardFontGrey() const {
        return "#616161";
    }

    virtual QString loginWizardFontLightGrey() const {
        return "#BDBDBD";
    }

    virtual QString trayFontColor() const {
        return "#001B41";
    }

    virtual QString trayBorderColor() const {
        return "#D7D7D7";
    }

    virtual QString trayInputFieldBorderColor() const {
        return "#718095";
    }

    virtual QString fontConfigurationCss(QString font, QString size, QString weight, QString color) const {
        return QString("font-family: %1; font-size: %2; font-weight: %3; color: %4; ").arg(
            font,
            size,
            weight,
            color);
    }

    //Colors
    virtual QString settingsLinkColor() const {
        return "#02306A";
    }

    virtual QString quotaProgressColor() const {
        return "#308cc6";
    }

    virtual QString syncProgressColor() const {
        return "#359ada";
    }

    virtual QString buttonPrimaryColor() const {
        return "#0F6CBD";
    }

    virtual QString buttonSecondaryColor() const {
        return "#FFFFFF";
    }

    virtual QString buttonSecondaryBorderColor() const {
        return "#D1D1D1";
    }

    virtual QString buttonDisabledColor() const {
        return "#F0F0F0";
    }

    virtual QString buttonPrimaryHoverColor() const {
        return "#115EA3";
    }

    virtual QString buttonSecondaryHoverColor() const {
        return "#F5F5F5";
    }

    virtual QString buttonPrimaryPressedColor() const {
        return "#0C3B5E";
    }

    virtual QString buttonSecondaryPressedColor() const {
        return "#E0E0E0";
    }

    virtual QString buttonPrimaryFocusedBorderColor() const {
        return "#000000";
    }

    virtual QString buttonSecondaryFocusedBorderColor() const {
        return "#000000";
    }

    virtual QString buttonDisabledFontColor() const {
        return "#BDBDBD";
    }

    virtual QString pillButtonPrimaryColor() const {
        return "#0B2A63";
    }

    virtual QString pillButtonSecondaryColor() const {
        return "#FFFFFF";
    }

    virtual QString pillButtonBorderColor() const {
        return "#0B2A63";
    }

    virtual QString clipboardBackgroundColor() const {
        return "#FFFFFF";
    }

    virtual QString white() const {
        return "#FFFFFF";
    }

    virtual QString black() const {
        return "#000000";
    }

    virtual QString dialogBackgroundColor() const {
        return "#FAFAFA";
    }

    virtual QString trayBackgroundColor() const {
        return "#FFFFFF";
    }

    virtual QString menuBorderColor() const {
        return "#2E4360";
    }

    virtual QString menuTextColor() const {
        return "#001B41";
    }

    virtual QString menuPressedTextColor() const {
        return "#001B41";
    }

    virtual QString iconDarkColor() const {
        return "#001B41";
    }

    virtual QString menuSelectedItemColor() const {
        return "#F4F7FA";
    }

    virtual QString menuPressedItemColor() const {
        return "#F4F7FA";
    }

    virtual QString menuBorderRadius() const {
        return "16px";
    }

    virtual QString buttonIconColor() const {
        return "#1474C4";
    }

    virtual QString buttonIconHoverColor() const {
        return "#FFFFFF";
    }

    virtual QString buttonPressedColor() const {
        return "#0B2A63";
    }

    virtual QString buttonHoveredColor() const {
        return "#1474C4";
    }

    virtual QString toolButtonHoveredColor() const {
        return "#DBEDF8";
    }

    virtual QString toolButtonPressedColor() const {
        return "#95CAEB";
    }

    virtual QString errorColor() const {
        return "#FDF3F4";
    }

    virtual QString errorBorderColor() const {
        return "#EEACB2";
    }

    virtual QString trayErrorBorderColor() const {
        return "#F50C00";
    }

    virtual QString trayErrorTextColor() const {
        return "#C80A00";
    }

    virtual QString warningBorderColor() const {
        return "#F4BFAB";
    }

    virtual QString warningColor() const {
        return "#FDF6F3";
    }

    virtual QString successBorderColor() const {
        return "#9FD89F";
    }

    virtual QString successColor() const {
        return "#F1FAF1";
    }

    virtual QString infoBorderColor() const {
        return "#11C7E6";
    }

    virtual QString infoColor() const {
        return "#E6F9FC";
    }

    private:
    inline static const QString _sesFolder = QStringLiteral("ses/");
};
}
#endif // _BASETHEME_H