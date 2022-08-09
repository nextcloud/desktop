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

#include "theme.h"
#include "common/depreaction.h"
#include "common/utility.h"
#include "common/version.h"
#include "common/vfs.h"
#include "config.h"
#include "configfile.h"

#include <QtCore>
#ifndef TOKEN_AUTH_ONLY
#include <QtGui>
#include <QStyle>
#include <QApplication>
#endif
#include <QSslSocket>

#include "owncloudtheme.h"

#ifdef THEME_INCLUDE
#include THEME_INCLUDE
#endif

namespace {
QString vanillaThemePath()
{
    return QStringLiteral(":/client/ownCloud/theme");
}

QString brandThemePath()
{
    return QStringLiteral(":/client/" APPLICATION_SHORTNAME "/theme");
}

QString darkTheme()
{
    return QStringLiteral("dark");
}

QString coloredTheme()
{
    return QStringLiteral("colored");
}

QString whiteTheme()
{
    return QStringLiteral("white");
}

QString blackTheme()
{
    return QStringLiteral("black");
}

constexpr bool strings_equal(char const *a, char const *b)
{
    return *a == *b && (*a == '\0' || strings_equal(a + 1, b + 1));
}

constexpr bool isVanilla()
{
    // TODO: c++17 stringview
    return strings_equal(APPLICATION_SHORTNAME, "ownCloud");
}
}
namespace OCC {

Theme *Theme::_instance = nullptr;

Theme *Theme::instance()
{
    if (!_instance) {
        _instance = new THEME_CLASS;
    }
    return _instance;
}

Theme::~Theme()
{
}

QString Theme::statusHeaderText(SyncResult::Status status) const
{
    QString resultStr;

    switch (status) {
    case SyncResult::Undefined:
        resultStr = QCoreApplication::translate("theme", "Status undefined");
        break;
    case SyncResult::NotYetStarted:
        resultStr = QCoreApplication::translate("theme", "Waiting to start sync");
        break;
    case SyncResult::SyncRunning:
        resultStr = QCoreApplication::translate("theme", "Sync is running");
        break;
    case SyncResult::Success:
        resultStr = QCoreApplication::translate("theme", "Sync Success");
        break;
    case SyncResult::Problem:
        resultStr = QCoreApplication::translate("theme", "Sync Success, some files were ignored.");
        break;
    case SyncResult::Error:
        resultStr = QCoreApplication::translate("theme", "Sync Error");
        break;
    case SyncResult::SetupError:
        resultStr = QCoreApplication::translate("theme", "Setup Error");
        break;
    case SyncResult::SyncPrepare:
        resultStr = QCoreApplication::translate("theme", "Preparing to sync");
        break;
    case SyncResult::SyncAbortRequested:
        resultStr = QCoreApplication::translate("theme", "Aborting...");
        break;
    case SyncResult::Paused:
        resultStr = QCoreApplication::translate("theme", "Sync is paused");
        break;
    }
    return resultStr;
}

QString Theme::appNameGUI() const
{
    return QStringLiteral(APPLICATION_NAME);
}

QString Theme::appName() const
{
    return QStringLiteral(APPLICATION_SHORTNAME);
}

QString Theme::configFileName() const
{
    return QStringLiteral(APPLICATION_EXECUTABLE ".cfg");
}

#ifndef TOKEN_AUTH_ONLY

QIcon Theme::applicationIcon() const
{
    return themeUniversalIcon(QStringLiteral(APPLICATION_ICON_NAME "-icon"));
}

QIcon Theme::aboutIcon() const
{
    return applicationIcon();
}

bool Theme::isUsingDarkTheme() const
{
    //TODO: replace by a command line switch
    static bool forceDark = qEnvironmentVariableIntValue("OWNCLOUD_FORCE_DARK_MODE") != 0;
    return forceDark || QPalette().base().color().lightnessF() <= 0.5;
}

bool Theme::allowDarkTheme() const
{
    return _hasBrandedColored == _hasBrandedDark;
}


QIcon Theme::themeUniversalIcon(const QString &name, Theme::IconType iconType) const
{
    return loadIcon(QStringLiteral("universal"), name, iconType);
}

QIcon Theme::themeTrayIcon(const QString &name, bool sysTrayMenuVisible, IconType iconType) const
{
    auto icon = loadIcon(systrayIconFlavor(_mono, sysTrayMenuVisible), name, iconType);
#ifdef Q_OS_MAC
    // This defines the icon as a template and enables automatic macOS color handling
    // See https://bugreports.qt.io/browse/QTBUG-42109
    icon.setIsMask(_mono && !sysTrayMenuVisible);
#endif
    return icon;
}

QIcon Theme::themeIcon(const QString &name, Theme::IconType iconType) const
{
    return loadIcon((isUsingDarkTheme() && allowDarkTheme()) ? darkTheme() : coloredTheme(), name, iconType);
}
/*
 * helper to load a icon from either the icon theme the desktop provides or from
 * the apps Qt resources.
 */
QIcon Theme::loadIcon(const QString &flavor, const QString &name, IconType iconType) const
{
    // prevent recusion
    const bool useCoreIcon = (iconType == IconType::VanillaIcon) || isVanilla();
    const QString path = useCoreIcon ? vanillaThemePath() : brandThemePath();
    const QString key = name + QLatin1Char(',') + flavor;
    QIcon &cached = _iconCache[key]; // Take reference, this will also "set" the cache entry
    if (cached.isNull()) {
        if (isVanilla() && QIcon::hasThemeIcon(name)) {
            // use from theme
            return cached = QIcon::fromTheme(name);
        }
        const QString svg = QStringLiteral("%1/%2/%3.svg").arg(path, flavor, name);
        if (QFile::exists(svg)) {
            return cached = QIcon(svg);
        }

        const QString png = QStringLiteral("%1/%2/%3.png").arg(path, flavor, name);
        if (QFile::exists(png)) {
            return cached = QIcon(png);
        }

        const QList<int> sizes {16, 22, 32, 48, 64, 128, 256, 512, 1024};
        QString previousIcon;
        for (int size : sizes) {
            QString pixmapName = QStringLiteral("%1/%2/%3-%4.png").arg(path, flavor, name, QString::number(size));
            if (QFile::exists(pixmapName)) {
                previousIcon = pixmapName;
                cached.addFile(pixmapName, { size, size });
            } else if (size >= 128) {
                if (!previousIcon.isEmpty()) {
                    qWarning() << "Upscaling:" << previousIcon << "to" << size;
                    cached.addPixmap(QPixmap(previousIcon).scaled({ size, size }, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            }
        }
    }
    if (cached.isNull()) {
        if (!useCoreIcon && iconType == IconType::BrandedIconWithFallbackToVanillaIcon) {
            return loadIcon(flavor, name, IconType::VanillaIcon);
        }
        qWarning() << "Failed to locate the icon" << name;
    }
    return cached;
}

bool Theme::hasTheme(IconType type, const QString &theme) const
{
    const auto key = qMakePair(type != IconType::VanillaIcon, theme);
    auto it = _themeCache.constFind(key);
    if (it == _themeCache.cend()) {
        return _themeCache[key] = QFileInfo(QStringLiteral("%1/%2/").arg(type == IconType::VanillaIcon ? vanillaThemePath() : brandThemePath(), theme)).isDir();
    }
    return it.value();
}

QString Theme::systrayIconFlavor(bool mono, bool sysTrayMenuVisible) const
{
    Q_UNUSED(sysTrayMenuVisible)
    QString flavor;
    if (mono) {
        flavor = Utility::hasDarkSystray() ? whiteTheme() : blackTheme();

#ifdef Q_OS_MAC
        if (sysTrayMenuVisible) {
            flavor = QLatin1String("white");
        }
#endif
    } else {
        // we have a dark sys tray and the theme has support for that
        flavor = (Utility::hasDarkSystray() && allowDarkTheme()) ? darkTheme() : coloredTheme();
    }
    return flavor;
}

#endif

Theme::Theme()
    : QObject(nullptr)
{
}

// If this option returns true, the client only supports one folder to sync.
// The Add-Button is removed accordingly.
bool Theme::singleSyncFolder() const
{
    return false;
}

bool Theme::multiAccount() const
{
    return true;
}

QString Theme::defaultServerFolder() const
{
    return QStringLiteral("/");
}

QString Theme::helpUrl() const
{
    return QStringLiteral("https://doc.owncloud.org/desktop/%1.%2/").arg(OCC::Version::version().majorVersion()).arg(OCC::Version::version().microVersion());
}

QString Theme::conflictHelpUrl() const
{
    auto baseUrl = helpUrl();
    if (baseUrl.isEmpty())
        return QString();
    if (!baseUrl.endsWith(QLatin1Char('/')))
        baseUrl.append(QLatin1Char('/'));
    return baseUrl + QStringLiteral("conflicts.html");
}

QString Theme::overrideServerUrl() const
{
    return QString();
}

QString Theme::overrideServerUrlV2() const
{
    static const auto serverOverride = qEnvironmentVariable("OWNCLOUD_OVERRIDE_SERVER_URL");
    if (serverOverride.isEmpty()) {
        OC_DISABLE_DEPRECATED_WARNING
        return overrideServerUrl();
        OC_ENABLE_DEPRECATED_WARNING
    }
    return serverOverride;
}

QString Theme::forceConfigAuthType() const
{
    return QString();
}


QString Theme::defaultClientFolder() const
{
    return appName();
}

void Theme::setSystrayUseMonoIcons(bool mono)
{
    _mono = mono;
    emit systrayUseMonoIconsChanged(mono);
}

bool Theme::systrayUseMonoIcons() const
{
    return _mono;
}

bool Theme::monoIconsAvailable() const
{
    // mono icons are only supported in vanilla and if a customer provides them
    // no fallback to vanilla
    return hasTheme(IconType::BrandedIcon, systrayIconFlavor(true));
}

QString Theme::updateCheckUrl() const
{
    return QStringLiteral("https://updates.owncloud.com/client/");
}

qint64 Theme::newBigFolderSizeLimit() const
{
    // Default to 500MB
    return 500;
}

bool Theme::wizardHideExternalStorageConfirmationCheckbox() const
{
    return false;
}

bool Theme::wizardSkipAdvancedPage() const
{
    return false;
}

bool Theme::wizardHideFolderSizeLimitCheckbox() const
{
    return false;
}

QString Theme::gitSHA1(VersionFormat format) const
{
    const QString gitShahSort = Version::gitSha().left(6);
    if (!aboutShowCopyright()) {
        return gitShahSort;
    }
    const auto gitUrl = QStringLiteral("https://github.com/owncloud/client/commit/%1").arg(Version::gitSha());
    switch (format) {
    case Theme::VersionFormat::OneLiner:
        Q_FALLTHROUGH();
    case Theme::VersionFormat::Plain:
        return gitShahSort;
    case Theme::VersionFormat::Url:
        return gitUrl;
    case Theme::VersionFormat::RichText:
        return QStringLiteral("<a href=\"%1\">%3</a>").arg(gitUrl, gitShahSort);
    }
    return QString();
}

QString Theme::aboutVersions(Theme::VersionFormat format) const
{
    const QString br = [&format] {
        switch (format) {
        case Theme::VersionFormat::RichText:
            return QStringLiteral("<br>");
        case Theme::VersionFormat::Url:
            Q_FALLTHROUGH();
        case Theme::VersionFormat::Plain:
            return QStringLiteral("\n");
        case Theme::VersionFormat::OneLiner:
            return QStringLiteral(" ");
        }
        Q_UNREACHABLE();
    }();
    const QString qtVersion = QString::fromUtf8(qVersion());
    const QString qtVersionString = (QLatin1String(QT_VERSION_STR) == qtVersion ? qtVersion : QCoreApplication::translate("ownCloudTheme::qtVer", "%1 (Built against Qt %1)").arg(qtVersion, QStringLiteral(QT_VERSION_STR)));
    QString _version = Version::displayString();
    QString gitUrl;
    if (!Version::gitSha().isEmpty()) {
        if (format != Theme::VersionFormat::Url) {
            _version = QCoreApplication::translate("ownCloudTheme::versionWithSha", "%1 %2").arg(_version, gitSHA1(format));
        } else {
            gitUrl = gitSHA1(format) + br;
        }
    }
    return QCoreApplication::translate("ownCloudTheme::aboutVersions()",
        "%1 %2 %3%8"
        "%9"
        "Libraries Qt %4, %5%8"
        "Using virtual files plugin: %6%8"
        "%7")
        .arg(appName(),
            _version,
            QStringLiteral(__DATE__ " " __TIME__),
            qtVersionString,
            QSslSocket::sslLibraryVersionString(),
            Vfs::modeToString(bestAvailableVfsMode()),
            QSysInfo::productType() % QLatin1Char('-') % QSysInfo::kernelVersion(),
            br,
            gitUrl);
}


QString Theme::about() const
{
    // Ideally, the vendor should be "ownCloud GmbH", but it cannot be changed without
    // changing the location of the settings and other registery keys.
    const QString vendor = isVanilla() ? QStringLiteral("ownCloud GmbH") : QStringLiteral(APPLICATION_VENDOR);
    return tr("<p>Version %1. For more information visit <a href=\"%2\">https://%3</a></p>"
              "<p>For known issues and help, please visit: <a href=\"https://central.owncloud.org/c/desktop-client\">https://central.owncloud.org</a></p>"
              "<p><small>By Klaas Freitag, Daniel Molkentin, Olivier Goffart, Markus Götz, "
              " Jan-Christoph Borchardt, Thomas Müller,<br>"
              "Dominik Schmidt, Michael Stingl, Hannah von Reth, Fabian Müller and others.</small></p>"
              "<p>Copyright ownCloud GmbH</p>"
              "<p>Distributed by %4 and licensed under the GNU General Public License (GPL) Version 2.0.<br/>"
              "%5 and the %5 logo are registered trademarks of %4 in the "
              "United States, other countries, or both.</p>"
              "<p><small>%6</small></p>")
        .arg(Utility::escape(Version::displayString()),
            Utility::escape(QStringLiteral("https://" APPLICATION_DOMAIN)),
            Utility::escape(QStringLiteral(APPLICATION_DOMAIN)),
            Utility::escape(vendor),
            Utility::escape(appNameGUI()),
            aboutVersions(Theme::VersionFormat::RichText));
}

bool Theme::aboutShowCopyright() const
{
    return true;
}

#ifndef TOKEN_AUTH_ONLY
QIcon Theme::syncStateIcon(SyncResult::Status status, bool sysTray, bool sysTrayMenuVisible) const
{
    return syncStateIcon(SyncResult { status }, sysTray, sysTrayMenuVisible);
}
QIcon Theme::syncStateIcon(const SyncResult &result, bool sysTray, bool sysTrayMenuVisible) const
{
    QString statusIcon;

    switch (result.status()) {
    case SyncResult::NotYetStarted:
        Q_FALLTHROUGH();
    case SyncResult::SyncRunning:
        statusIcon = QStringLiteral("state-sync");
        break;
    case SyncResult::SyncAbortRequested:
        Q_FALLTHROUGH();
    case SyncResult::Paused:
        statusIcon = QStringLiteral("state-pause");
        break;
    case SyncResult::SyncPrepare:
        Q_FALLTHROUGH();
    case SyncResult::Success:
        if (result.hasUnresolvedConflicts()) {
            return syncStateIcon(SyncResult { SyncResult::Problem }, sysTray, sysTrayMenuVisible);
        }
        statusIcon = QStringLiteral("state-ok");
        break;
    case SyncResult::Problem:
        Q_FALLTHROUGH();
    case SyncResult::Undefined:
        // this can happen if no sync connections are configured.
        statusIcon = QStringLiteral("state-information");
        break;
    case SyncResult::Error:
        Q_FALLTHROUGH();
    case SyncResult::SetupError:
    // FIXME: Use state-problem once we have an icon.
        statusIcon = QStringLiteral("state-error");
    }
    if (sysTray) {
        return themeTrayIcon(statusIcon, sysTrayMenuVisible);
    } else {
        return themeIcon(statusIcon);
    }
}

QIcon Theme::folderDisabledIcon() const
{
    return themeIcon(QLatin1String("state-pause"));
}

QIcon Theme::folderOfflineIcon(bool sysTray, bool sysTrayMenuVisible) const
{
    const auto statusIcon = QLatin1String("state-offline");
    if (sysTray) {
        return themeTrayIcon(statusIcon, sysTrayMenuVisible);
    } else {
        return themeIcon(statusIcon);
    }
}

QColor Theme::wizardHeaderTitleColor() const
{
    return qApp->palette().text().color();
}

QColor Theme::wizardHeaderSubTitleColor() const
{
    return wizardHeaderTitleColor();
}

QColor Theme::wizardHeaderBackgroundColor() const
{
    return QColor();
}

QIcon Theme::wizardHeaderLogo() const
{
    return applicationIcon();
}

QPixmap Theme::wizardHeaderBanner(const QSize &size) const
{
    const QColor c = wizardHeaderBackgroundColor();
    if (!c.isValid())
        return QPixmap();
    QPixmap pix(size);
    pix.fill(c);
    return pix;
}
#endif

QString Theme::webDavPath() const
{
    return QStringLiteral("remote.php/webdav/");
}

bool Theme::linkSharing() const
{
    return true;
}

bool Theme::userGroupSharing() const
{
    return true;
}

bool Theme::forceSystemNetworkProxy() const
{
    return false;
}

Theme::UserIDType Theme::userIDType() const
{
    return UserIDType::UserIDUserName;
}

QString Theme::customUserID() const
{
    return QString();
}

QString Theme::userIDHint() const
{
    return QString();
}


QString Theme::wizardUrlPostfix() const
{
    return QString();
}

QString Theme::wizardUrlHint() const
{
    return QString();
}

QString Theme::quotaBaseFolder() const
{
    return QStringLiteral("/");
}

QString Theme::oauthClientId() const
{
    return QStringLiteral("xdXOt13JKxym1B1QcEncf2XDkLAexMBFwiT9j6EfhhHFJhs2KM9jbjTmf8JBXE69");
}

QString Theme::oauthClientSecret() const
{
    return QStringLiteral("UBntmLjC2yYCeHwsyj73Uwo9TAaecAetRwMw0xYcvNL9yRdLSUi0hUAHfvCHFeFh");
}

QString Theme::oauthLocalhost() const
{
    return QStringLiteral("http://localhost");
}

QPair<QString, QString> Theme::oauthOverrideAuthUrl() const
{
    return {};
}

QString Theme::openIdConnectScopes() const
{
    return QStringLiteral("openid offline_access email profile");
}

QString Theme::openIdConnectPrompt() const
{
    return QStringLiteral("select_account consent");
}

QString Theme::versionSwitchOutput() const
{
    return aboutVersions(Theme::VersionFormat::Url);
}

bool Theme::showVirtualFilesOption() const
{
    return enableExperimentalFeatures();
}

bool Theme::forceVirtualFilesOption() const
{
    return false;
}

bool Theme::enableExperimentalFeatures() const
{
    return ConfigFile().showExperimentalOptions();
}

bool Theme::connectionValidatorClearCookies() const
{
    return false;
}

bool Theme::enableSocketApiIconSupport() const
{
    return true;
}

bool Theme::warnOnMultipleDb() const
{
    return isVanilla();
}

bool Theme::allowDuplicatedFolderSyncPair() const
{
    return true;
}

bool Theme::wizardEnableWebfinger() const
{
    return false;
}

template <>
OWNCLOUDSYNC_EXPORT QString Utility::enumToDisplayName(Theme::UserIDType userIdType)
{
    switch (userIdType) {
    case Theme::UserIDUserName:
        return QCoreApplication::translate("Type of user ID", "Username");
    case Theme::UserIDEmail:
        return QCoreApplication::translate("Type of user ID", "E-mail address");
    case Theme::UserIDCustom:
        return Theme::instance()->customUserID();
    default:
        Q_UNREACHABLE();
    }
}

} // end namespace client
