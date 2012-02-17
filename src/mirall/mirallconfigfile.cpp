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
#include <QtCore>
#include <QtGui>

#include "mirall/mirallconfigfile.h"
#include "mirall/owncloudtheme.h"
#include "mirall/miralltheme.h"

namespace Mirall {

MirallConfigFile::MirallConfigFile()
{
}

QString MirallConfigFile::mirallConfigFile() const
{
#ifdef OWNCLOUD_CLIENT
    ownCloudTheme theme;
#else
    mirallTheme theme;
#endif
    const QString dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation)+"/"+theme.configFileName();
    return dir;
}

bool MirallConfigFile::exists()
{
    QFile file( mirallConfigFile() );
    return file.exists();
}

void MirallConfigFile::writeOwncloudConfig( const QString& connection,
                                         const QString& url,
                                         const QString& user,
                                         const QString& passwd )
{
    qDebug() << "*** writing mirall config to " << mirallConfigFile();
    QSettings settings( mirallConfigFile(), QSettings::IniFormat);
    QString cloudsUrl( url );

    if( !cloudsUrl.startsWith("http") )
        cloudsUrl.prepend( "http://" );

    settings.beginGroup( connection );
    settings.setValue("url", cloudsUrl );
    settings.setValue("user", user );
    settings.setValue("password", passwd );

    settings.sync();
}

/*
 * returns the configured owncloud url if its already configured, otherwise an empty
 * string.
 */
QString MirallConfigFile::ownCloudUrl( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = QString::fromLocal8Bit("ownCloud");

    QSettings settings( mirallConfigFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QString url = settings.value( "url" ).toString();

    qDebug() << "Returning configured owncloud url: " << url;

  return url;
}

QUrl MirallConfigFile::fullOwnCloudUrl( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = QString::fromLocal8Bit("ownCloud");

    QSettings settings( mirallConfigFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QUrl url( ownCloudUrl( con ) );
    url.setUserName( ownCloudUser( con ) );
    url.setPassword( ownCloudPasswd( con ) );

    return url;
}

QString MirallConfigFile::ownCloudUser( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = QString::fromLocal8Bit("ownCloud");

    QSettings settings( mirallConfigFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QString user = settings.value( "user" ).toString();
    qDebug() << "Returning configured owncloud user: " << user;

    return user;
}

QString MirallConfigFile::ownCloudPasswd( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = QString::fromLocal8Bit("ownCloud");

    QSettings settings( mirallConfigFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QString pwd = settings.value( "password" ).toString();

    return pwd;
}

}
