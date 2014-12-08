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
#include "version.h"
#include "config.h"
#include "utility.h"

#include <QtCore>
#ifndef TOKEN_AUTH_ONLY
#include <QtGui>
#endif

#include "owncloudtheme.h"

#ifdef THEME_INCLUDE
#  define QUOTEME(M)       #M
#  define INCLUDE_FILE(M)  QUOTEME(M)
#  include INCLUDE_FILE(THEME_INCLUDE)
#endif

namespace OCC {

Theme* Theme::_instance = 0;

Theme* Theme::instance() {
    if (!_instance) {
        _instance = new THEME_CLASS;
        // some themes may not call the base ctor
        _instance->_mono = false;
    }
    return _instance;
}

QString Theme::statusHeaderText( SyncResult::Status status ) const
{
    QString resultStr;

    switch( status ) {
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
        resultStr = QCoreApplication::translate("theme", "Setup Error" );
        break;
    case SyncResult::SyncPrepare:
        resultStr = QCoreApplication::translate("theme", "Preparing to sync" );
        break;
    case SyncResult::SyncAbortRequested:
        resultStr = QCoreApplication::translate("theme", "Aborting..." );
        break;
    case SyncResult::Paused:
        resultStr = QCoreApplication::translate("theme", "Sync is paused");
        break;
    }
    return resultStr;
}

QString Theme::appNameGUI() const
{
    return QLatin1String(APPLICATION_NAME);
}

QString Theme::appName() const
{
    return QLatin1String(APPLICATION_SHORTNAME);
}

QString Theme::version() const
{
    return QString::fromLocal8Bit( MIRALL_STRINGIFY( MIRALL_VERSION ));
}

#ifndef TOKEN_AUTH_ONLY

QIcon Theme::trayFolderIcon( const QString& backend ) const
{
    Q_UNUSED(backend)
    return applicationIcon();
}

/*
 * helper to load a icon from either the icon theme the desktop provides or from
 * the apps Qt resources.
 */
QIcon Theme::themeIcon( const QString& name, bool sysTray ) const
{
    QString flavor;
    if (sysTray) {
        flavor = systrayIconFlavor(_mono);
    } else {
        flavor = QLatin1String("colored");
    }

    QIcon icon;
    if( QIcon::hasThemeIcon( name )) {
        // use from theme
        icon = QIcon::fromTheme( name );
    } else {
        QList<int> sizes;
        sizes <<16 << 22 << 32 << 48 << 64 << 128;
        foreach (int size, sizes) {
            QString pixmapName = QString::fromLatin1(":/client/theme/%1/%2-%3.png").arg(flavor).arg(name).arg(size);
            if (QFile::exists(pixmapName)) {
                QPixmap px(pixmapName);
                // HACK, get rid of it by supporting FDO icon themes, this is really just emulating ubuntu-mono
                if (qgetenv("DESKTOP_SESSION") == "ubuntu") {
                    QBitmap mask = px.createMaskFromColor(Qt::white, Qt::MaskOutColor);
                    QPainter p(&px);
                    p.setPen(QColor("#dfdbd2"));
                    p.drawPixmap(px.rect(), mask, mask.rect());
                }
                icon.addPixmap(px);
            }
        }
        if (icon.isNull()) {
            foreach (int size, sizes) {
                QString pixmapName = QString::fromLatin1(":/client/resources/%1-%2.png").arg(name).arg(size);
                if (QFile::exists(pixmapName)) {
                    icon.addFile(pixmapName);
                }
            }
        }
    }
    return icon;
}

#endif

Theme::Theme() :
    QObject(0)
    ,_mono(false)
{

}

// if this option return true, the client only supports one folder to sync.
// The Add-Button is removed accoringly.
bool Theme::singleSyncFolder() const {
    return false;
}

QString Theme::defaultServerFolder() const
{
    return QLatin1String("/");
}

QString Theme::overrideServerUrl() const
{
    return QString::null;
}

QString Theme::defaultClientFolder() const
{
    return appName();
}

QString Theme::systrayIconFlavor(bool mono) const
{
    QString flavor;
    if (mono) {
        flavor = Utility::hasDarkSystray() ? QLatin1String("white") : QLatin1String("black");
    } else {
        flavor = QLatin1String("colored");
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

QString Theme::updateCheckUrl() const
{
    return QLatin1String("https://updates.owncloud.com/client/");
}

QString Theme::gitSHA1() const
{
    QString devString;
#ifdef GIT_SHA1
    const QString githubPrefix(QLatin1String(
                                   "https://github.com/owncloud/mirall/commit/"));
    const QString gitSha1(QLatin1String(GIT_SHA1));
    devString = QCoreApplication::translate("ownCloudTheme::about()",
                   "<p><small>Built from Git revision <a href=\"%1\">%2</a>"
                   " on %3, %4 using Qt %5.</small></p>")
            .arg(githubPrefix+gitSha1).arg(gitSha1.left(6))
            .arg(__DATE__).arg(__TIME__)
            .arg(QT_VERSION_STR);
#endif
    return devString;
}

QString Theme::about() const
{
    return tr("<p>Version %1 For more information please visit <a href='%2'>%3</a>.</p>"
              "<p>Copyright ownCloud, Inc.</p>"
              "<p>Distributed by %4 and licensed under the GNU General Public License (GPL) Version 2.0.<br/>"
              "%5 and the %5 logo are registered trademarks of %4 in the "
              "United States, other countries, or both.</p>")
            .arg(MIRALL_VERSION_STRING).arg("http://" MIRALL_STRINGIFY(APPLICATION_DOMAIN))
            .arg(MIRALL_STRINGIFY(APPLICATION_DOMAIN)).arg(APPLICATION_VENDOR).arg(APPLICATION_NAME)
            +gitSHA1();
}

#ifndef TOKEN_AUTH_ONLY
QVariant Theme::customMedia( CustomMediaType type )
{
    QVariant re;
    QString key;

    switch ( type )
    {
    case oCSetupTop:
        key = QLatin1String("oCSetupTop");
        break;
    case oCSetupSide:
        key = QLatin1String("oCSetupSide");
        break;
    case oCSetupBottom:
        key = QLatin1String("oCSetupBottom");
        break;
    case oCSetupResultTop:
        key = QLatin1String("oCSetupResultTop");
        break;
    }

    QString imgPath = QString::fromLatin1(":/client/theme/colored/%1.png").arg(key);
    if ( QFile::exists( imgPath ) ) {
        QPixmap pix( imgPath );
        if( pix.isNull() ) {
            // pixmap loading hasn't succeeded. We take the text instead.
            re.setValue( key );
        } else {
            re.setValue( pix );
        }
    }
    return re;
}

QIcon Theme::syncStateIcon( SyncResult::Status status, bool sysTray ) const
{
    // FIXME: Mind the size!
    QString statusIcon;

    switch( status ) {
    case SyncResult::Undefined:
        // this can happen if no sync connections are configured.
        statusIcon = QLatin1String("state-information");
        break;
    case SyncResult::NotYetStarted:
    case SyncResult::SyncRunning:
        statusIcon = QLatin1String("state-sync");
        break;
    case SyncResult::SyncAbortRequested:
    case SyncResult::Paused:
        statusIcon = QLatin1String("state-pause");
        break;
    case SyncResult::SyncPrepare:
    case SyncResult::Success:
        statusIcon = QLatin1String("state-ok");
        break;
    case SyncResult::Problem:
        statusIcon = QLatin1String("state-information");
        break;
    case SyncResult::Error:
    case SyncResult::SetupError:
        // FIXME: Use state-problem once we have an icon.
    default:
        statusIcon = QLatin1String("state-error");
    }

    return themeIcon( statusIcon, sysTray );
}

QIcon Theme::folderDisabledIcon( ) const
{
    return themeIcon( QLatin1String("state-pause") );
}

QIcon Theme::folderOfflineIcon(bool systray) const
{
    return themeIcon( QLatin1String("state-offline"), systray );
}

QColor Theme::wizardHeaderTitleColor() const
{
    return qApp->palette().text().color();
}

QColor Theme::wizardHeaderBackgroundColor() const
{
    return QColor();
}

QPixmap Theme::wizardHeaderLogo() const
{
    return applicationIcon().pixmap(64);
}

QPixmap Theme::wizardHeaderBanner() const
{
    QColor c = wizardHeaderBackgroundColor();
    if (!c.isValid())
        return QPixmap();

    QPixmap pix(QSize(600, 78));
    pix.fill(wizardHeaderBackgroundColor());
    return pix;
}
#endif

} // end namespace mirall

