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
    if( qApp->applicationName().isEmpty() ) {
        qApp->setApplicationName( theme.appName() );
    }
    const QString dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation)+"/"+theme.configFileName();
    return dir;
}

bool MirallConfigFile::exists()
{
    QFile file( mirallConfigFile() );
    return file.exists();
}

QString MirallConfigFile::defaultConnection() const
{
    return QString::fromLocal8Bit("ownCloud");
}

bool MirallConfigFile::connectionExists( const QString& conn )
{
    QString con = conn;
    if( conn.isEmpty() ) con = defaultConnection();

    QSettings settings( mirallConfigFile(), QSettings::IniFormat);

    return settings.contains( QString("%1/url").arg( conn ) );
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

void MirallConfigFile::removeConnection( const QString& connection )
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    qDebug() << "    removing the config file for connection " << con;

    // Currently its just removing the entire config file
    QSettings settings( mirallConfigFile(), QSettings::IniFormat);
    settings.beginGroup( con );
    settings.remove("");  // removes all content from the group
    settings.sync();
}

/*
 * returns the configured owncloud url if its already configured, otherwise an empty
 * string.
 * The returned url always has a trailing hash.
 * If webdav is true, the webdav-server url is returned.
 */
QString MirallConfigFile::ownCloudUrl( const QString& connection, bool webdav ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings( mirallConfigFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QString url = settings.value( "url" ).toString();
    if( ! url.endsWith('/')) url.append('/');
    if( webdav ) url.append( "files/webdav.php/" );

    qDebug() << "Returning configured owncloud url: " << url;

  return url;
}

QString MirallConfigFile::ownCloudUser( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings( mirallConfigFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QString user = settings.value( "user" ).toString();
    qDebug() << "Returning configured owncloud user: " << user;

    return user;
}

QString MirallConfigFile::ownCloudPasswd( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings( mirallConfigFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QString pwd = settings.value( "password" ).toString();

    return pwd;
}

QByteArray MirallConfigFile::basicAuthHeader() const
{
    QString concatenated = ownCloudUser() + ":" + ownCloudPasswd();
    const QString b("Basic ");
    QByteArray data = b.toLocal8Bit() + concatenated.toLocal8Bit().toBase64();

    return data;
}

}
