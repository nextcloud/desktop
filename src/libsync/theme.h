/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef _THEME_H
#define _THEME_H

#include <QIcon>
#include <QObject>
#include <QPalette>
#include <QGuiApplication>
#include "syncresult.h"

class QString;
class QObject;
class QPixmap;
class QColor;
class QPaintDevice;

namespace OCC {

class SyncResult;

/**
 * @brief The Theme class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT Theme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool branded READ isBranded CONSTANT)
    Q_PROPERTY(QString appNameGUI READ appNameGUI CONSTANT)
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QUrl stateOnlineImageSource READ stateOnlineImageSource CONSTANT)
    Q_PROPERTY(QUrl stateOfflineImageSource READ stateOfflineImageSource CONSTANT)
    Q_PROPERTY(QUrl statusOnlineImageSource READ statusOnlineImageSource CONSTANT)
    Q_PROPERTY(QUrl statusDoNotDisturbImageSource READ statusDoNotDisturbImageSource CONSTANT)
    Q_PROPERTY(QUrl statusAwayImageSource READ statusAwayImageSource CONSTANT)
    Q_PROPERTY(QUrl statusInvisibleImageSource READ statusInvisibleImageSource CONSTANT)
#ifndef TOKEN_AUTH_ONLY
    Q_PROPERTY(QIcon folderDisabledIcon READ folderDisabledIcon CONSTANT)
    Q_PROPERTY(QIcon folderOfflineIcon READ folderOfflineIcon CONSTANT)
    Q_PROPERTY(QIcon applicationIcon READ applicationIcon CONSTANT)
#endif
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString helpUrl READ helpUrl CONSTANT)
    Q_PROPERTY(QString conflictHelpUrl READ conflictHelpUrl CONSTANT)
    Q_PROPERTY(QString overrideServerUrl READ overrideServerUrl WRITE setOverrideServerUrl NOTIFY overrideServerUrlChanged)
    Q_PROPERTY(bool forceOverrideServerUrl READ forceOverrideServerUrl WRITE setForceOverrideServerUrl NOTIFY forceOverrideServerUrlChanged)
    Q_PROPERTY(bool isVfsEnabled READ isVfsEnabled WRITE setVfsEnabled NOTIFY vfsEnabledChanged)
    Q_PROPERTY(bool startLoginFlowAutomatically READ startLoginFlowAutomatically WRITE setStartLoginFlowAutomatically NOTIFY startLoginFlowAutomaticallyChanged)
#ifndef TOKEN_AUTH_ONLY
    Q_PROPERTY(QColor wizardHeaderTitleColor READ wizardHeaderTitleColor CONSTANT)
    Q_PROPERTY(QColor wizardHeaderBackgroundColor READ wizardHeaderBackgroundColor CONSTANT)
#endif
    Q_PROPERTY(QString updateCheckUrl READ updateCheckUrl CONSTANT)

    Q_PROPERTY(QColor defaultColor READ defaultColor CONSTANT)

    Q_PROPERTY(QVariantMap systemPalette READ systemPalette NOTIFY systemPaletteChanged)
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY darkModeChanged)
public:
    enum CustomMediaType {
        oCSetupTop, // ownCloud connect page
        oCSetupSide,
        oCSetupBottom,
        oCSetupResultTop // ownCloud connect result page
    };

    /* returns a singleton instance. */
    static Theme *instance();

    ~Theme() override;

    /**
     * @brief isBranded indicates if the current application is branded
     *
     * By default, it is considered branded if the APPLICATION_NAME is
     * different from "Nextcloud".
     *
     * @return true if branded, false otherwise
     */
    [[nodiscard]] bool isBranded() const;

    /**
     * @brief appNameGUI - Human readable application name.
     *
     * Use and redefine this if the human readable name contains spaces,
     * special chars and such.
     *
     * By default, the name is derived from the APPLICATION_NAME
     * cmake variable.
     *
     * @return QString with human readable app name.
     */
    [[nodiscard]] QString appNameGUI() const;

    /**
     * @brief appName - Application name (short)
     *
     * Use and redefine this as an application name. Keep it straight as
     * it is used for config files etc. If you need a more sophisticated
     * name in the GUI, redefine appNameGUI.
     *
     * By default, the name is derived from the APPLICATION_SHORTNAME
     * cmake variable, and should be the same. This method is only
     * reimplementable for legacy reasons.
     *
     * Warning: Do not modify this value, as many things, e.g. settings
     * depend on it! You most likely want to modify \ref appNameGUI().
     *
     * @return QString with app name.
     */
    [[nodiscard]] QString appName() const;

    /**
     * @brief Returns full path to an online state icon
     * @return QUrl full path to an icon
     */
    [[nodiscard]] QUrl stateOnlineImageSource() const;

    /**
     * @brief Returns full path to an offline state icon
     * @return QUrl full path to an icon
     */
    [[nodiscard]] QUrl stateOfflineImageSource() const;
    
    /**
     * @brief Returns full path to an online user status icon
     * @return QUrl full path to an icon
     */
    [[nodiscard]] QUrl statusOnlineImageSource() const;
    
    /**
     * @brief Returns full path to an do not disturb user status icon
     * @return QUrl full path to an icon
     */
    [[nodiscard]] QUrl statusDoNotDisturbImageSource() const;
    
    /**
     * @brief Returns full path to an away user status icon
     * @return QUrl full path to an icon
     */
    [[nodiscard]] QUrl statusAwayImageSource() const;
    
    /**
     * @brief Returns full path to an invisible user status icon
     * @return QUrl full path to an icon
     */
    [[nodiscard]] QUrl statusInvisibleImageSource() const;

    [[nodiscard]] QUrl syncStatusOk() const;

    [[nodiscard]] QUrl syncStatusError() const;

    [[nodiscard]] QUrl syncStatusRunning() const;

    [[nodiscard]] QUrl syncStatusPause() const;

    [[nodiscard]] QUrl syncStatusWarning() const;

    [[nodiscard]] QUrl folderOffline() const;

    /**
     * @brief configFileName
     * @return the name of the config file.
     */
    [[nodiscard]] QString configFileName() const;

#ifndef TOKEN_AUTH_ONLY
    static QString hidpiFileName(const QString &fileName, QPaintDevice *dev = nullptr);

    static QString hidpiFileName(const QString &iconName, const QColor &backgroundColor, QPaintDevice *dev = nullptr);

    static bool isHidpi(QPaintDevice *dev = nullptr);

    /**
      * get an sync state icon
      */
    [[nodiscard]] QIcon syncStateIcon(SyncResult::Status, bool sysTray = false) const;

    [[nodiscard]] QIcon folderDisabledIcon() const;
    [[nodiscard]] QIcon folderOfflineIcon(bool sysTray = false) const;
    [[nodiscard]] QIcon applicationIcon() const;
#endif

    [[nodiscard]] QString statusHeaderText(SyncResult::Status) const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString versionSuffix() const;

    /**
     * Characteristics: bool if more than one sync folder is allowed
     */
    [[nodiscard]] bool singleSyncFolder() const;

    /**
     * When true, client works with multiple accounts.
     */
    [[nodiscard]] bool multiAccount() const;

    /**
    * URL to documentation.
    *
    * This is opened in the browser when the "Help" action is selected from the tray menu.
    *
    * If the function is overridden to return an empty string the action is removed from
    * the menu.
    *
    * Defaults to Nextclouds client documentation website.
    */
    [[nodiscard]] QString helpUrl() const;

    /**
     * The url to use for showing help on conflicts.
     *
     * If the function is overridden to return an empty string no help link will be shown.
     *
     * Defaults to helpUrl() + "conflicts.html", which is a page in ownCloud's client
     * documentation website. If helpUrl() is empty, this function will also return the
     * empty string.
     */
    [[nodiscard]] QString conflictHelpUrl() const;

    /**
     * Setting a value here will pre-define the server url.
     *
     * The respective UI controls will be disabled only if forceOverrideServerUrl() is true
     */
    [[nodiscard]] QString overrideServerUrl() const;

    /**
     * Enforce a pre-defined server url.
     *
     * When true, the respective UI controls will be disabled
     */
    [[nodiscard]] bool forceOverrideServerUrl() const;

    /**
     * Enforce use of virtual files whenever possible.
     */
    [[nodiscard]] bool isVfsEnabled() const;

    /**
     * Automatically start login flow
     *
     * When true, the browser will get opened automatically
     */
    [[nodiscard]] bool startLoginFlowAutomatically() const;
    
    /**
     * Enable OCSP stapling for SSL handshakes
     *
     * When true, peer will be requested for Online Certificate Status Protocol response
     */
    [[nodiscard]] bool enableStaplingOCSP() const;

    /**
     * Enforce SSL validity
     *
     * When true, trusting the untrusted certificate is not allowed
     */
    [[nodiscard]] bool forbidBadSSL() const;

    /**
     * Forbid use of proxy
     *
     * When true, the app always connects to the server directly
     */
    [[nodiscard]] bool doNotUseProxy() const;

    /**
     * This is only useful when previous version had a different overrideServerUrl
     * with a different auth type in that case You should then specify "http" or "shibboleth".
     * Normally this should be left empty.
     */
    [[nodiscard]] QString forceConfigAuthType() const;

    /**
     * The default folder name without path on the server at setup time.
     */
    [[nodiscard]] QString defaultServerFolder() const;

    /**
     * The default folder name without path on the client side at setup time.
     */
    [[nodiscard]] QString defaultClientFolder() const;

    /**
     * Override to encforce a particular locale, i.e. "de" or "pt_BR"
     */
    [[nodiscard]] QString enforcedLocale() const { return QString(); }

    /** colored, white or black */
    [[nodiscard]] QString systrayIconFlavor(bool mono) const;

#ifndef TOKEN_AUTH_ONLY
    /**
     * Override to use a string or a custom image name.
     * The default implementation will try to look up
     * :/client/theme/<type>.png
     */
    [[nodiscard]] QVariant customMedia(CustomMediaType type);

    /** @return color for the setup wizard */
    [[nodiscard]] QColor wizardHeaderTitleColor() const;

    /** @return color for the setup wizard. */
    [[nodiscard]] QColor wizardHeaderBackgroundColor() const;

    [[nodiscard]] QPixmap wizardApplicationLogo() const;

    /** @return logo for the setup wizard. */
    [[nodiscard]] QPixmap wizardHeaderLogo() const;

    /**
     * The default implementation creates a
     * background based on
     * \ref wizardHeaderTitleColor().
     *
     * @return banner for the setup wizard.
     */
    [[nodiscard]] QPixmap wizardHeaderBanner() const;
#endif

    /**
     * The SHA sum of the released git commit
     */
    [[nodiscard]] QString gitSHA1() const;

    /**
     * About info text
     */
    [[nodiscard]] QString aboutInfo() const;

    /**
     * About dialog contents
     */
    [[nodiscard]] QString about() const;

    /**
     * Legal notice dialog version detail contents
     */
    [[nodiscard]] QString aboutDetails() const;

    /**
     * Define if the systray icons should be using mono design
     */
    void setSystrayUseMonoIcons(bool mono);

    /**
     * Retrieve whether to use mono icons for systray
     */
    [[nodiscard]] bool systrayUseMonoIcons() const;

    /**
     * Check if mono icons are available
     */
    [[nodiscard]] bool monoIconsAvailable() const;

    /**
     * @brief Where to check for new Updates.
     */
    [[nodiscard]] QString updateCheckUrl() const;

    /**
     * When true, the setup wizard will show the selective sync dialog by default and default
     * to nothing selected
     */
    [[nodiscard]] bool wizardSelectiveSyncDefaultNothing() const;

    /**
     * Default option for the newBigFolderSizeLimit.
     * Size in MB of the maximum size of folder before we ask the confirmation.
     * Set -1 to never ask confirmation.  0 to ask confirmation for every folder.
     **/
    [[nodiscard]] qint64 newBigFolderSizeLimit() const;

    /**
     * Hide the checkbox that says "Ask for confirmation before synchronizing folders larger than X MB"
     * in the account wizard
     */
    [[nodiscard]] bool wizardHideFolderSizeLimitCheckbox() const;
    /**
     * Hide the checkbox that says "Ask for confirmation before synchronizing external storages"
     * in the account wizard
     */
    [[nodiscard]] bool wizardHideExternalStorageConfirmationCheckbox() const;

    /**
     * @brief Sharing options
     *
     * Allow link sharing and or user/group sharing
     */
    [[nodiscard]] bool linkSharing() const;
    [[nodiscard]] bool userGroupSharing() const;

    /**
     * If this returns true, the user cannot configure the proxy in the network settings.
     * The proxy settings will be disabled in the configuration dialog.
     * Default returns false.
     */
    [[nodiscard]] bool forceSystemNetworkProxy() const;

    /**
     * @brief How to handle the userID
     *
     * @value UserIDUserName Wizard asks for username as ID
     * @value UserIDEmail Wizard asks for an email as ID
     * @value UserIDCustom Specify string in \ref customUserID
     */
    enum UserIDType { UserIDUserName = 0,
        UserIDEmail,
        UserIDCustom };

    /** @brief What to display as the userID (e.g. in the wizards)
     *
     *  @return UserIDType::UserIDUserName, unless reimplemented
     */
    [[nodiscard]] UserIDType userIDType() const;

    /**
     * @brief Allows to customize the type of user ID (e.g. user name, email)
     *
     * @note This string cannot be translated, but is still useful for
     *       referencing brand name IDs (e.g. "ACME ID", when using ACME.)
     *
     * @return An empty string, unless reimplemented
     */
    [[nodiscard]] QString customUserID() const;

    /**
     * @brief Demo string to be displayed when no text has been
     *        entered for the user id (e.g. mylogin@company.com)
     *
     * @return An empty string, unless reimplemented
     */
    [[nodiscard]] QString userIDHint() const;

    /**
     * @brief Postfix that will be enforced in a URL. e.g.
     *        ".myhosting.com".
     *
     * @return An empty string, unless reimplemented
     */
    [[nodiscard]] QString wizardUrlPostfix() const;

    /**
     * @brief String that will be shown as long as no text has been entered by the user.
     *
     * @return An empty string, unless reimplemented
     */
    [[nodiscard]] virtual QString wizardUrlHint() const;

    /**
     * @brief the server folder that should be queried for the quota information
     *
     * This can be configured to show the quota information for a different
     * folder than the root. This is the folder on which the client will do
     * PROPFIND calls to get "quota-available-bytes" and "quota-used-bytes"
     *
     * Defaults: "/"
     */
    [[nodiscard]] QString quotaBaseFolder() const;

    /**
     * @brief What should be output for the --version command line switch.
     *
     * By default, it's a combination of appName(), version(), the GIT SHA1 and some
     * important dependency versions.
     */
    [[nodiscard]] QString versionSwitchOutput() const;
	
	/**
    * @brief Request suitable QIcon resource depending on the background colour of the parent widget.
    *
    * This should be replaced (TODO) by a real theming implementation for the client UI 
    * (actually 2019/09/13 only systray theming).
    */
    [[nodiscard]] QIcon uiThemeIcon(const QString &iconName, bool uiHasDarkBg) const;

    Q_INVOKABLE static double getColorDarkness(const QColor &color);

    /**
     * @brief Perform a calculation to check if a colour is dark or light and accounts for different sensitivity of the human eye.
     *
     * @return True if the specified colour is dark.
     *
     * 2019/12/08: Moved here from SettingsDialog.
     */
    Q_INVOKABLE static bool isDarkColor(const QColor &color);
    
    /**
     * @brief Return the colour to be used for HTML links (e.g. used in QLabel), based on the current app palette or given colour (Dark-/Light-Mode switching).
     *
     * @return Background-aware colour for HTML links, based on the current app palette or given colour.
     *
     * 2019/12/08: Implemented for the Dark Mode on macOS, because the app palette can not account for that (Qt 5.12.5).
     */
    static QColor getBackgroundAwareLinkColor(const QColor &backgroundColor);
    
    /**
     * @brief Return the colour to be used for HTML links (e.g. used in QLabel), based on the current app palette (Dark-/Light-Mode switching).
     *
     * @return Background-aware colour for HTML links, based on the current app palette.
     *
     * 2019/12/08: Implemented for the Dark Mode on macOS, because the app palette can not account for that (Qt 5.12.5).
     */
    static QColor getBackgroundAwareLinkColor();

    /**
     * @brief Appends a CSS-style colour value to all HTML link tags in a given string, based on the current app palette or given colour (Dark-/Light-Mode switching).
     *
     * 2019/12/08: Implemented for the Dark Mode on macOS, because the app palette can not account for that (Qt 5.12.5).
     *
     * This way we also avoid having certain strings re-translated on Transifex.
     */
    static void replaceLinkColorStringBackgroundAware(QString &linkString, const QColor &backgroundColor);

    /**
     * @brief Appends a CSS-style colour value to all HTML link tags in a given string, based on the current app palette (Dark-/Light-Mode switching).
     *
     * 2019/12/08: Implemented for the Dark Mode on macOS, because the app palette can not account for that (Qt 5.12.5).
     *
     * This way we also avoid having certain strings re-translated on Transifex.
     */
    static void replaceLinkColorStringBackgroundAware(QString &linkString);

    /**
     * @brief Appends a CSS-style colour value to all HTML link tags in a given string, as specified by newColor.
     *
     * 2019/12/19: Implemented for the Dark Mode on macOS, because the app palette can not account for that (Qt 5.12.5).
     *
     * This way we also avoid having certain strings re-translated on Transifex.
     */
    static void replaceLinkColorString(QString &linkString, const QColor &newColor);

    /**
     * @brief Creates a colour-aware icon based on the specified palette's base colour.
     *
     * @return QIcon, colour-aware (inverted on dark backgrounds).
     *
     * 2019/12/09: Moved here from SettingsDialog.
     */
    static QIcon createColorAwareIcon(const QString &name, const QPalette &palette);

    /**
     * @brief Creates a colour-aware icon based on the app palette's base colour (Dark-/Light-Mode switching).
     *
     * @return QIcon, colour-aware (inverted on dark backgrounds).
     *
     * 2019/12/09: Moved here from SettingsDialog.
     */
    static QIcon createColorAwareIcon(const QString &name);

    /**
     * @brief Creates a colour-aware pixmap based on the specified palette's base colour.
     *
     * @return QPixmap, colour-aware (inverted on dark backgrounds).
     *
     * 2019/12/09: Adapted from createColorAwareIcon.
     */
    static QPixmap createColorAwarePixmap(const QString &name, const QPalette &palette);

    /**
     * @brief Creates a colour-aware pixmap based on the app palette's base colour (Dark-/Light-Mode switching).
     *
     * @return QPixmap, colour-aware (inverted on dark backgrounds).
     *
     * 2019/12/09: Adapted from createColorAwareIcon.
     */
    static QPixmap createColorAwarePixmap(const QString &name);

    /**
     * @brief Whether to show the option to create folders using "virtual files".
     *
     * By default, the options are not shown unless experimental options are
     * manually enabled in the configuration file.
     */
    [[nodiscard]] bool showVirtualFilesOption() const;

    [[nodiscard]] bool enforceVirtualFilesSyncFolder() const;

    static QColor defaultColor();

    static constexpr const char *themePrefix = ":/client/theme/";

    QVariantMap systemPalette();
    bool darkMode();

public slots:
    void setOverrideServerUrl(const QString &overrideServerUrl);
    void setForceOverrideServerUrl(bool forceOverride);
    void setVfsEnabled(bool enabled);
    void setStartLoginFlowAutomatically(bool startLoginFlowAuto);

protected:
#ifndef TOKEN_AUTH_ONLY
    QIcon themeIcon(const QString &name, bool sysTray = false) const;
#endif
    /**
     * @brief Generates image path in the resources
     * @param name Name of the image file
     * @param size Size in the power of two (16, 32, 64, etc.)
     * @param sysTray Whether the image requested is for Systray or not
     * @return QString image path in the resources
     **/
    QString themeImagePath(const QString &name, int size = -1, bool sysTray = false) const;
    Theme();

signals:
    void systrayUseMonoIconsChanged(bool);
    void systemPaletteChanged(const QPalette &palette);
    void darkModeChanged();
    void overrideServerUrlChanged();
    void forceOverrideServerUrlChanged();
    void vfsEnabledChanged();
    void startLoginFlowAutomaticallyChanged();

private:
    Theme(Theme const &);
    Theme &operator=(Theme const &);

    void connectToPaletteSignal();
#if defined(Q_OS_WIN)
    QPalette reserveDarkPalette; // Windows 11 button and window dark colours
#endif

    static Theme *_instance;
    bool _mono = false;
    bool _paletteSignalsConnected = false;

    QString _overrideServerUrl;
    bool _forceOverrideServerUrl = false;
    bool _isVfsEnabled = false;
    bool _startLoginFlowAutomatically = false;

#ifndef TOKEN_AUTH_ONLY
    mutable QHash<QString, QIcon> _iconCache;
#endif
};
}
#endif // _THEME_H
