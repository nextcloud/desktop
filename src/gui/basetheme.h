#ifndef _BASETHEME_H
#define _BASETHEME_H

#include <QColor>
#include <QFont>
#include <QString>
#include "theme.h"
#include "common/utility.h"

namespace OCC {

class BaseTheme : public QObject{
    Q_OBJECT
    Q_PROPERTY(QString dialogBackgroundColor READ dialogBackgroundColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString checkboxCheckmarkColor READ checkboxCheckmarkColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString trayFontColor READ trayFontColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString trayBorderColor READ trayBorderColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString trayInputFieldBorderColor READ trayInputFieldBorderColor CONSTANT)
    Q_PROPERTY(QString trayBackgroundColor READ trayBackgroundColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString iconDarkColor READ iconDarkColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString headerBannerColor READ headerBannerColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString buttonIconColor READ buttonIconColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString buttonHoveredColor READ buttonHoveredColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString buttonPressedColor READ buttonPressedColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString toolButtonHoveredColor READ toolButtonHoveredColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString toolButtonPressedColor READ toolButtonPressedColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString pillButtonPrimaryColor READ pillButtonPrimaryColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString pillButtonSecondaryColor READ pillButtonSecondaryColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString pillButtonBorderColor READ pillButtonBorderColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString clipboardBackgroundColor READ clipboardBackgroundColor CONSTANT)
    Q_PROPERTY(QString trayErrorBorderColor READ trayErrorBorderColor NOTIFY themeColorsChanged)
    Q_PROPERTY(QString trayErrorTextColor READ trayErrorTextColor NOTIFY themeColorsChanged)
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

    BaseTheme() {
        connect(Theme::instance(), &Theme::darkModeChanged, this, &BaseTheme::themeColorsChanged);
    }

    virtual ~BaseTheme() = default;

    virtual QString themePrefix(QString context = "qml") const {
        if (context == "qml") {
            return QString("qrc:///client/theme/");
        }
        return QString(Theme::themePrefix);
    }

    virtual QString additionalThemePrefix() const { return QStringLiteral(""); }

    // Recolors a themed ses-*.svg icon at load time via Qt's svgimage-custom-color image
    // provider (see SvgImageProvider), ignoring whatever fill color is baked into the SVG.
    // Use this instead of a static icon getter wherever the icon is consumed by QML as an
    // "image://" source, so it stays legible when darkMode() flips.
    Q_INVOKABLE virtual QString coloredIcon(const QString &fileName, const QString &color) const {
        return QStringLiteral("image://svgimage-custom-color/") + _sesFolder + additionalThemePrefix() + fileName + QStringLiteral("/") + color;
    }

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

    virtual QString syncSuccessIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-syncstate-success.svg");
    }

    virtual QString syncErrorIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-syncstate-error.svg");
    }

    virtual QString syncPausedIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-syncstate-paused.svg");
    }

    virtual QString syncWarningIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-syncstate-warning.svg");
    }

    virtual QString syncSyncingIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-syncstate-syncing.svg");
    }

    virtual QString syncOfflineIcon(QString context = "qml") const {
        return themePrefix(context) + _sesFolder + additionalThemePrefix() + QStringLiteral("ses-state-offline.svg");
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
        return Utility::isWindows() ? QStringLiteral("Segoe UI") : QStringLiteral("Open Sans");
    }

    virtual QString contextMenuFont() const {
        return settingsFont();
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
        return themedColor("#000000", "#D6E4F5");
    }

    virtual QString folderWizardSubtitleColor() const {
        return themedColor("#104996", "#5FA8E0");
    }

    virtual QString folderWizardPathColor() const {
        return themedColor("#97A3B4", "#A8B4C6");
    }

    virtual QString loginWizardFontGrey() const {
        return themedColor("#616161", "#D6D6D6");
    }

    virtual QString loginWizardFontLightGrey() const {
        return themedColor("#BDBDBD", "#8A8A8A");
    }

    virtual QString trayFontColor() const {
        return themedColor("#001B41", "#D6E4F5");
    }

    virtual QString trayBorderColor() const {
        return themedColor("#D7D7D7", "#3A3D42");
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
        return themedColor("#02306A", "#5FA8E0");
    }

    virtual QString quotaProgressColor() const {
        return themedColor("#308cc6", "#5FA8E0");
    }

    virtual QString syncProgressColor() const {
        return themedColor("#359ada", "#4FB6F0");
    }

    virtual QString buttonPrimaryColor() const {
        return themedColor("#0F6CBD", "#3D6BB0");
    }

    virtual QString buttonSecondaryColor() const {
        // Dark value matches the tray's pillButtonSecondaryColor() (SecondaryPillButton.qml).
        return themedColor("#FFFFFF", "#2A2E35");
    }

    virtual QString buttonSecondaryBorderColor() const {
        // Dark value matches the tray's pillButtonBorderColor() (SecondaryPillButton.qml).
        return themedColor("#D1D1D1", "#3D6BB0");
    }

    virtual QString buttonDisabledColor() const {
        return themedColor("#F0F0F0", "#33363C");
    }

    virtual QString buttonPrimaryHoverColor() const {
        return themedColor("#115EA3", "#4B7CC4");
    }

    virtual QString buttonSecondaryHoverColor() const {
        return themedColor("#F5F5F5", "#343841");
    }

    virtual QString buttonPrimaryPressedColor() const {
        return themedColor("#0C3B5E", "#2A4D82");
    }

    virtual QString buttonSecondaryPressedColor() const {
        return themedColor("#E0E0E0", "#3D424D");
    }

    virtual QString buttonPrimaryFocusedBorderColor() const {
        return themedColor("#000000", "#FFFFFF");
    }

    virtual QString buttonSecondaryFocusedBorderColor() const {
        return themedColor("#000000", "#FFFFFF");
    }

    virtual QString buttonDisabledFontColor() const {
        return themedColor("#BDBDBD", "#5A5D63");
    }

    virtual QString pillButtonPrimaryColor() const {
        return themedColor("#0B2A63", "#3D6BB0");
    }

    virtual QString pillButtonSecondaryColor() const {
        return themedColor("#FFFFFF", "#2A2E35");
    }

    virtual QString pillButtonBorderColor() const {
        return themedColor("#0B2A63", "#3D6BB0");
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
        return themedColor("#FFFFFF", "#1E2126");
    }

    // Fixed white in both modes: the checked box's fill is always a saturated accent color
    // (buttonPrimaryColor()), not the page background, so the checkmark needs to contrast
    // against that accent color rather than flip with light/dark mode.
    virtual QString checkboxCheckmarkColor() const {
        return themedColor("#FFFFFF", "#FFFFFF");
    }

    virtual QString trayBackgroundColor() const {
        return themedColor("#FFFFFF", "#1E2126");
    }

    virtual QString menuBorderColor() const {
        return themedColor("#2E4360", "#5B7699");
    }

    virtual QString menuTextColor() const {
        return themedColor("#001B41", "#D6E4F5");
    }

    virtual QString menuPressedTextColor() const {
        return themedColor("#001B41", "#D6E4F5");
    }

    virtual QString iconDarkColor() const {
        return themedColor("#001B41", "#D6E4F5");
    }

    virtual QString menuSelectedItemColor() const {
        return themedColor("#F4F7FA", "#333844");
    }

    // Background of the brand logo banner at the top of the tray dropdown (HeaderLogo.qml).
    // Kept separate from menuSelectedItemColor so a brand can give its logo banner its own
    // look without also affecting selected-menu-item styling elsewhere.
    virtual QString headerBannerColor() const {
        return themedColor("#F4F7FA", "#333844");
    }

    virtual QString menuPressedItemColor() const {
        return themedColor("#F4F7FA", "#333844");
    }

    virtual QString menuBorderRadius() const {
        return "16px";
    }

    virtual QString buttonIconColor() const {
        return themedColor("#1474C4", "#5FA8E0");
    }

    virtual QString buttonIconHoverColor() const {
        return themedColor("#FFFFFF", "#0B2A63");
    }

    virtual QString buttonPressedColor() const {
        return themedColor("#0B2A63", "#123B85");
    }

    virtual QString buttonHoveredColor() const {
        return themedColor("#1474C4", "#5FA8E0");
    }

    virtual QString toolButtonHoveredColor() const {
        return themedColor("#DBEDF8", "#233240");
    }

    virtual QString toolButtonPressedColor() const {
        return themedColor("#95CAEB", "#2C4A63");
    }

    virtual QString errorBorderColor() const {
        return themedColor("#EEACB2", "#B25C63");
    }

    virtual QString trayErrorBorderColor() const {
        return themedColor("#F50C00", "#FF6B61");
    }

    virtual QString trayErrorTextColor() const {
        return themedColor("#C80A00", "#FF8A80");
    }

    virtual QString warningBorderColor() const {
        return themedColor("#F4BFAB", "#C98F5E");
    }

    virtual QString successBorderColor() const {
        return themedColor("#9FD89F", "#5FA86A");
    }

    virtual QString infoBorderColor() const {
        return themedColor("#11C7E6", "#4DD9F0");
    }

    virtual QString treeViewHoverColor() const {
        return themedColor("#e5f3ff", "#242A33");
    }

signals:
    void themeColorsChanged();

protected:
    // TODO: first-pass dark variants, derived by hand and not yet design-reviewed.
    // Needs a proper visual QA/design pass before being considered final.
    static QString themedColor(const QString &light, const QString &dark) {
        return Theme::instance()->darkMode() ? dark : light;
    }

public:
    // Tints an already theme-aware border color into a translucent fill, instead of
    // inventing a separate opaque pastel per dark/light variant.
    static QColor tintedFillFromBorder(const QColor &border, float alpha = 0.2f) {
        auto fill = border;
        fill.setAlphaF(alpha);
        return fill;
    }

    private:
    inline static const QString _sesFolder = QStringLiteral("ses/");
};
}
#endif // _BASETHEME_H