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

#include "owncloudtheme.h"

#include <QString>
#include <QDebug>
#include <QPixmap>
#include <QIcon>
#include <QStyle>
#include <QApplication>

#include "mirall/version.h"
#include "config.h"

namespace Mirall {

ownCloudTheme::ownCloudTheme()
{
    // qDebug() << " ** running ownCloud theme!";
}

QString ownCloudTheme::appName() const
{
    /* If this is changed, existing configs are not found any more
     * because the value is used by QDesktopServices to find the config
     * file. Be aware.
     */
    return QLatin1String("ownCloud");
}

QString ownCloudTheme::configFileName() const
{
    return QLatin1String("owncloud.cfg");
}

QString ownCloudTheme::about() const
{
    QString devString;
#ifdef GIT_SHA1
    const QString githubPrefix(QLatin1String(
                                   "    https://github.com/owncloud/mirall/commit/"));
    const QString gitSha1(QLatin1String(GIT_SHA1));
    devString = QCoreApplication::translate("ownCloudTheme::about()",
                   "<p><small>Built from Git revision <a href=\"%1\">%2</a>"
                   " on %3, %4<br>using OCsync %5 and Qt %6.</small><p>")
            .arg(githubPrefix+gitSha1).arg(gitSha1.left(6))
            .arg(__DATE__).arg(__TIME__)
            .arg(MIRALL_STRINGIFY(LIBCSYNC_VERSION))
            .arg(QT_VERSION_STR);
#endif
    return  QCoreApplication::translate("ownCloudTheme::about()",
               "<p><b>%1 Client Version %2</b></p>"
               "<p><b>Authors</b>"
               "<br><a href=\"mailto:freitag@owncloud.com\">"
               "Klaas Freitag</a>, ownCloud, Inc."
               "<br><a href=\"mailto:danimo@owncloud.com\">"
               "Daniel Molkentin</a>, ownCloud, Inc."
               "<br><br>Based on Mirall by Duncan Mac-Vicar P.</p>"
               "<p>For more information visit <a href=\"%3\">%4</a>.</p>"
               "%7"
               )
            .arg(appName())
            .arg(MIRALL_STRINGIFY(MIRALL_VERSION))
            .arg("http://" MIRALL_STRINGIFY(APPLICATION_DOMAIN))
            .arg(MIRALL_STRINGIFY(APPLICATION_DOMAIN))
            .arg(devString);
}

QPixmap ownCloudTheme::splashScreen() const
{
    return QPixmap(QLatin1String(":/mirall/resources/owncloud_splash.png"));
}

QIcon ownCloudTheme::folderIcon( const QString& backend ) const
{
  QString name;

  if( backend == QLatin1String("owncloud")) {
      name = QLatin1String( "owncloud-framed" );
  }
  if( backend == QLatin1String("unison" )) {
      name  = QLatin1String( "folder-sync" );
  }
  if( backend == QLatin1String("csync" )) {
      name   = QLatin1String( "folder-remote" );
  }
  if( backend.isEmpty() || backend == QLatin1String("none") ) {
      name = QLatin1String("folder-grey");
  }

  qDebug() << "==> load folder icon " << name;
  return themeIcon( name );
}

QIcon ownCloudTheme::trayFolderIcon( const QString& ) const
{
    QPixmap fallback = qApp->style()->standardPixmap(QStyle::SP_FileDialogNewFolder);
    return QIcon::fromTheme("folder", fallback);
}

QIcon ownCloudTheme::syncStateIcon( SyncResult::Status status, bool sysTray ) const
{
    // FIXME: Mind the size!
    QString statusIcon;

    switch( status ) {
    case SyncResult::Undefined:
    case SyncResult::NotYetStarted:
    case SyncResult::Unavailable:
        statusIcon = QLatin1String("state-offline");
        break;
    case SyncResult::SyncRunning:
        statusIcon = QLatin1String("state-sync");
        break;
    case SyncResult::SyncPrepare:
    case SyncResult::Success:
        statusIcon = QLatin1String("state-ok");
        break;
    case SyncResult::Error:
    case SyncResult::SetupError:
    default:
        statusIcon = QLatin1String("state-error");
    }

    return themeIcon( statusIcon, sysTray );
}

QIcon ownCloudTheme::folderDisabledIcon( ) const
{
    // Fixme: Do we really want the dialog-canel from theme here?
    return themeIcon( QLatin1String("state-pause") );
}

QIcon ownCloudTheme::applicationIcon( ) const
{
    return themeIcon( QLatin1String("owncloud") );
}

}

