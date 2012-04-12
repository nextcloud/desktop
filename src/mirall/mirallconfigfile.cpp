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

#ifdef Q_WS_WIN
#include <sys/types.h>
#include <sys/stat.h>

#include <windef.h>
#include <winbase.h>
#endif

#include "mirall/mirallconfigfile.h"
#include "mirall/owncloudtheme.h"
#include "mirall/miralltheme.h"

namespace Mirall {

QString MirallConfigFile::_passwd = QString();
bool    MirallConfigFile::_askedUser = false;

MirallConfigFile::MirallConfigFile()
{
}

QString MirallConfigFile::configPath() const
{
    QString dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    if( !dir.endsWith('/') ) dir.append('/');
    return dir;
}

QString MirallConfigFile::excludeFile() const
{
    const QString exclFile("exclude.lst");
    QString dir = configPath();
    dir += exclFile;

    QFileInfo fi( dir );
    if( fi.isReadable() ) {
        return dir;
    }
    // Check alternative places...
#ifdef Q_WS_WIN
    /* For win32, try to copy the conf file from the directory from where the app was started. */
    char buf[MAX_PATH+1];
    int  len = 0;

    /* Get the path from where the application was started */
    len = GetModuleFileName(NULL, buf, MAX_PATH);
    QString exePath = QString::fromLocal8Bit(buf);
    exePath.remove("owncloud.exe");
    fi.setFile(exePath, exclFile );
#else
    fi.setFile( QString("/etc"), exclFile );
#endif
    if( fi.isReadable() ) {
        qDebug() << "  ==> returning exclude file path: " << fi.absoluteFilePath();
        return fi.absoluteFilePath();
    }
    qDebug() << "EMPTY exclude file path!";
    return QString();
}

QString MirallConfigFile::configFile() const
{
#ifdef OWNCLOUD_CLIENT
    ownCloudTheme theme;
#else
    mirallTheme theme;
#endif
    if( qApp->applicationName().isEmpty() ) {
        qApp->setApplicationName( theme.appName() );
    }
    const QString dir = configPath() + theme.configFileName();
    return dir;
}

bool MirallConfigFile::exists()
{
    QFile file( configFile() );
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

    QSettings settings( configFile(), QSettings::IniFormat);

    return settings.contains( QString("%1/url").arg( conn ) );
}


void MirallConfigFile::writeOwncloudConfig( const QString& connection,
                                            const QString& url,
                                            const QString& user,
                                            const QString& passwd,
                                            bool skipPwd )
{
    const QString file = configFile();
    qDebug() << "*** writing mirall config to " << file;
    QString pwd( passwd );

    QSettings settings( file, QSettings::IniFormat);
    QString cloudsUrl( url );

    if( !cloudsUrl.startsWith("http") )
        cloudsUrl.prepend( "http://" );

    settings.beginGroup( connection );
    settings.setValue("url", cloudsUrl );
    settings.setValue("user", user );
    if( skipPwd ) {
        pwd = QString();
    }
    settings.setValue("password", pwd );
    settings.setValue("nostoredpassword", QVariant(skipPwd) );
    settings.sync();

    // check the perms, only read-write for the owner.
    QFile::setPermissions( file, QFile::ReadOwner|QFile::WriteOwner );

}

void MirallConfigFile::removeConnection( const QString& connection )
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    qDebug() << "    removing the config file for connection " << con;

    // Currently its just removing the entire config file
    QSettings settings( configFile(), QSettings::IniFormat);
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

    QSettings settings( configFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QString url = settings.value( "url" ).toString();
    if( ! url.isEmpty() ) {
        if( ! url.endsWith('/')) url.append('/');
        if( webdav ) url.append( "files/webdav.php/" );
    }

    qDebug() << "Returning configured owncloud url: " << url;

  return url;
}

QString MirallConfigFile::ownCloudUser( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings( configFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QString user = settings.value( "user" ).toString();
    qDebug() << "Returning configured owncloud user: " << user;

    return user;
}

QString MirallConfigFile::ownCloudPasswd( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings( configFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    QString pwd;

    bool boolfalse( false );
    bool skipPwd = settings.value( "nostoredpassword", QVariant(boolfalse) ).toBool();
    if( skipPwd ) {
        if( ! _askedUser ) {
            bool ok;
            QString text = QInputDialog::getText(0, QObject::tr("ownCloud Password Required"),
                                                 QObject::tr("Please enter your ownCloud password:"), QLineEdit::Password,
                                                 QString(), &ok);
            if( ok && !text.isEmpty() ) { // empty password is not allowed on ownCloud
                _passwd = text;
                _askedUser = true;
            }
        }
        pwd = _passwd;
    } else {
        pwd = settings.value( "password" ).toString();
    }

    return pwd;
}

bool MirallConfigFile::ownCloudSkipUpdateCheck( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings( configFile(), QSettings::IniFormat );
    settings.beginGroup( con );

    bool skipIt = settings.value( "skipUpdateCheck", false ).toBool();

    return skipIt;
}



QByteArray MirallConfigFile::basicAuthHeader() const
{
    QString concatenated = ownCloudUser() + ":" + ownCloudPasswd();
    const QString b("Basic ");
    QByteArray data = b.toLocal8Bit() + concatenated.toLocal8Bit().toBase64();

    return data;
}

}
