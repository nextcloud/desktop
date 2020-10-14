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
#include "config.h"
#include "common/utility.h"
#include "version.h"
#include "configfile.h"
#include "common/vfs.h"

#include <QtCore>
#ifndef TOKEN_AUTH_ONLY
#include <QtGui>
#include <QStyle>
#include <QApplication>
#endif
#include <QSslSocket>

#include "owncloudtheme.h"

#ifdef THEME_INCLUDE
#define Mirall OCC // namespace hack to make old themes work
#define QUOTEME(M) #M
#define INCLUDE_FILE(M) QUOTEME(M)
#include INCLUDE_FILE(THEME_INCLUDE)
#undef Mirall
#endif

namespace OCC {

Theme *Theme::_instance = nullptr;

Theme *Theme::instance()
{
    if (!_instance) {
        _instance = new THEME_CLASS;
        // some themes may not call the base ctor
        _instance->_mono = false;
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

QString Theme::version() const
{
    return QStringLiteral(MIRALL_VERSION_STRING);
}

QString Theme::configFileName() const
{
    return QStringLiteral(APPLICATION_EXECUTABLE ".cfg");
}

#ifndef TOKEN_AUTH_ONLY

QIcon Theme::applicationIcon() const
{
    return themeIcon(QStringLiteral(APPLICATION_ICON_NAME "-icon"));
}

/*
 * helper to load a icon from either the icon theme the desktop provides or from
 * the apps Qt resources.
 */
QIcon Theme::themeIcon(const QString &name, bool sysTray, bool sysTrayMenuVisible) const
{
    QString flavor;
    if (sysTray) {
        flavor = systrayIconFlavor(_mono, sysTrayMenuVisible);
    } else {
        flavor = QStringLiteral("colored");
    }

    QString key = name + QLatin1Char(',') + flavor;
    QIcon &cached = _iconCache[key]; // Take reference, this will also "set" the cache entry
    if (cached.isNull()) {
        if (appName() == QLatin1String("ownCloud") && QIcon::hasThemeIcon(name)) {
            // use from theme
            return cached = QIcon::fromTheme(name);
        }

        const QList<int> sizes {16, 22, 32, 48, 64, 128, 256, 512, 1024};
        foreach (int size, sizes) {
            QString pixmapName = QStringLiteral(":/client/theme/%1/%2-%3.png").arg(flavor).arg(name).arg(size);
            if (QFile::exists(pixmapName)) {
                QPixmap px(pixmapName);
                // HACK, get rid of it by supporting FDO icon themes, this is really just emulating ubuntu-mono
                if (qgetenv("DESKTOP_SESSION") == "ubuntu") {
                    QBitmap mask = px.createMaskFromColor(Qt::white, Qt::MaskOutColor);
                    QPainter p(&px);
                    p.setPen(QColor("#dfdbd2"));
                    p.drawPixmap(px.rect(), mask, mask.rect());
                }
                cached.addPixmap(px);
            }
        }
        if (cached.isNull()) {
            QString pixmapName = QStringLiteral(":/client/resources/%1.svg").arg(name);
            if (QFile::exists(pixmapName)) {
                cached.addFile(pixmapName);
            }
        }
    }

#ifdef Q_OS_MAC
    // This defines the icon as a template and enables automatic macOS color handling
    // See https://bugreports.qt.io/browse/QTBUG-42109
    cached.setIsMask(_mono && sysTray && !sysTrayMenuVisible);
#endif

    return cached;
}
#endif

Theme::Theme()
    : QObject(nullptr)
    , _mono(false)
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
    return QStringLiteral("https://doc.owncloud.org/desktop/%1.%2/").arg(MIRALL_VERSION_MAJOR).arg(MIRALL_VERSION_MINOR);
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

QString Theme::forceConfigAuthType() const
{
    return QString();
}


QString Theme::defaultClientFolder() const
{
    return appName();
}

QString Theme::systrayIconFlavor(bool mono, bool sysTrayMenuVisible) const
{
    Q_UNUSED(sysTrayMenuVisible)
    QString flavor;
    if (mono) {
        flavor = Utility::hasDarkSystray() ? QStringLiteral("white") : QStringLiteral("black");

#ifdef Q_OS_MAC
        if (sysTrayMenuVisible) {
            flavor = QLatin1String("white");
        }
#endif
    } else {
        flavor = QStringLiteral("colored");
    }
    return flavor;
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
    QString themeDir = QStringLiteral(":/client/theme/%1/").arg(Theme::instance()->systrayIconFlavor(true));
    return QDir(themeDir).exists();
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

bool Theme::wizardHideFolderSizeLimitCheckbox() const
{
    return false;
}

QString Theme::gitSHA1(VersionFormat format) const
{
#ifdef GIT_SHA1
    const auto gitSha = QStringLiteral(GIT_SHA1);
    const auto gitShahSort = gitSha.left(6);
    if (!aboutShowCopyright()) {
        return gitShahSort;
    }
    const auto gitUrl = QStringLiteral("https://github.com/owncloud/client/commit/%1").arg(gitSha);
    switch (format) {
    case Theme::VersionFormat::Plain:
        return gitShahSort;
    case Theme::VersionFormat::Url:
        return gitUrl;
    case Theme::VersionFormat::RichText:
        return QStringLiteral("<a href=\"%1\">%3</a>").arg(gitUrl, gitShahSort);
    }
#endif
    return QString();
}

QString Theme::aboutVersions(Theme::VersionFormat format) const
{
    const QString br = format == Theme::VersionFormat::RichText ? QStringLiteral("<br>") : QStringLiteral("\n");
    const QString qtVersion = QString::fromUtf8(qVersion());
    const QString qtVersionString = (QLatin1String(QT_VERSION_STR) == qtVersion ? qtVersion : QCoreApplication::translate("ownCloudTheme::qtVer", "%1 (Built against Qt %1)").arg(qtVersion, QStringLiteral(QT_VERSION_STR)));
    QString _version = version();
    QString gitUrl;
#ifdef GIT_SHA1
    if (format != Theme::VersionFormat::Url) {
        _version = QCoreApplication::translate("ownCloudTheme::versionWithSha", "%1 %2").arg(version(), gitSHA1(format));
    } else {
        gitUrl = gitSHA1(format) + br;
    }
#endif
    return QCoreApplication::translate("ownCloudTheme::aboutVersions()",
        "%1 %2 %3 %4%8"
        "%9"
        "Libraries Qt %5, %6%8"
        "Using virtual files plugin: %7")
        .arg(appName(),
            _version,
            QStringLiteral(__DATE__),
            QStringLiteral(__TIME__),
            qtVersionString,
            QSslSocket::sslLibraryVersionString(),
            Vfs::modeToString(bestAvailableVfsMode()),
            br,
            gitUrl);
}


QString Theme::about() const
{
    QString vendor = QStringLiteral(APPLICATION_VENDOR);
    // Ideally, the vendor should be "ownCloud GmbH", but it cannot be changed without
    // changing the location of the settings and other registery keys.
    if (vendor == QLatin1String("ownCloud")) {
        vendor = QStringLiteral("ownCloud GmbH");
    }
    return tr("<p>Version %1. For more information visit <a href=\"%2\">https://%3</a></p>"
              "<p>For known issues and help, please visit: <a href=\"https://central.owncloud.org/c/desktop-client\">https://central.owncloud.org</a></p>"
              "<p><small>By Klaas Freitag, Daniel Molkentin, Olivier Goffart, Markus Götz, "
              " Jan-Christoph Borchardt, Thomas Müller, Dominik Schmidt, Michael Stingl, Hannah von Reth, and others.</small></p>"
              "<p>Copyright ownCloud GmbH</p>"
              "<p>Distributed by %4 and licensed under the GNU General Public License (GPL) Version 2.0.<br/>"
              "%5 and the %5 logo are registered trademarks of %4 in the "
              "United States, other countries, or both.</p>"
              "<p><small>%6</small></p>")
            .arg(Utility::escape(version()),
                 Utility::escape(QStringLiteral("https://" MIRALL_STRINGIFY(APPLICATION_DOMAIN))),
                 Utility::escape(QStringLiteral(MIRALL_STRINGIFY(APPLICATION_DOMAIN))),
                 Utility::escape(vendor),
                 Utility::escape(appNameGUI()),
                 aboutVersions(Theme::VersionFormat::RichText));
}

bool Theme::aboutShowCopyright() const
{
    return true;
}

#ifndef TOKEN_AUTH_ONLY
QVariant Theme::customMedia(CustomMediaType type)
{
    QVariant re;
    QString key;

    switch (type) {
    case oCSetupTop:
        key = QStringLiteral("oCSetupTop");
        break;
    case oCSetupSide:
        key = QStringLiteral("oCSetupSide");
        break;
    case oCSetupBottom:
        key = QStringLiteral("oCSetupBottom");
        break;
    case oCSetupResultTop:
        key = QStringLiteral("oCSetupResultTop");
        break;
    }

    QString imgPath = QStringLiteral(":/client/theme/colored/%1.png").arg(key);
    if (QFile::exists(imgPath)) {
        QPixmap pix(imgPath);
        if (pix.isNull()) {
            // pixmap loading hasn't succeeded. We take the text instead.
            re.setValue(key);
        } else {
            re.setValue(pix);
        }
    }
    return re;
}

QIcon Theme::syncStateIcon(SyncResult::Status status, bool sysTray, bool sysTrayMenuVisible) const
{
    // FIXME: Mind the size!
    QString statusIcon;

    switch (status) {
    case SyncResult::Undefined:
        // this can happen if no sync connections are configured.
        statusIcon = QStringLiteral("state-information");
        break;
    case SyncResult::NotYetStarted:
    case SyncResult::SyncRunning:
        statusIcon = QStringLiteral("state-sync");
        break;
    case SyncResult::SyncAbortRequested:
    case SyncResult::Paused:
        statusIcon = QStringLiteral("state-pause");
        break;
    case SyncResult::SyncPrepare:
    case SyncResult::Success:
        statusIcon = QStringLiteral("state-ok");
        break;
    case SyncResult::Problem:
        statusIcon = QStringLiteral("state-information");
        break;
    case SyncResult::Error:
    case SyncResult::SetupError:
    // FIXME: Use state-problem once we have an icon.
    default:
        statusIcon = QStringLiteral("state-error");
    }

    return themeIcon(statusIcon, sysTray, sysTrayMenuVisible);
}

QIcon Theme::folderDisabledIcon() const
{
    return themeIcon(QLatin1String("state-pause"));
}

QIcon Theme::folderOfflineIcon(bool sysTray, bool sysTrayMenuVisible) const
{
    return themeIcon(QLatin1String("state-offline"), sysTray, sysTrayMenuVisible);
}

QColor Theme::wizardHeaderTitleColor() const
{
    return qApp->palette().text().color();
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

QPair<QString, QString> Theme::oauthOverrideAuthUrl() const
{
    return {};
}

QString Theme::openIdConnectScopes() const
{
    return QStringLiteral("openid offline_access email profile");
}

QString Theme::versionSwitchOutput() const
{
    return aboutVersions(Theme::VersionFormat::Url);
}

bool Theme::showVirtualFilesOption() const
{
    return enableExperimentalFeatures();
}

bool Theme::enableExperimentalFeatures() const
{
    return ConfigFile().showExperimentalOptions();
}

} // end namespace client
