/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "accountmigrator.h"
#include "configfile.h"
#include "folderman.h"
#include "theme.h"


#include <QSettings>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

namespace OCC {

// The purpose of this class is to migrate an existing account that
// was set up with an unbranded client to an branded one.
// The usecase is: Usually people try first with the community client,
// later they maybe switch to a branded client. When they install the
// branded client first, it should automatically pick the information
// from the already configured account.

AccountMigrator::AccountMigrator()
{

}

// the list of folder definitions which are files in the directory "folders"
// underneath the ownCloud configPath (with ownCloud as a last segment)
// need to be copied to the themed path and adjusted.

QStringList AccountMigrator::migrateFolderDefinitons()
{
    ConfigFile cfg;
    QStringList re;

    QString themePath = cfg.configPath();
    // create the original ownCloud config path out of the theme path
    // by removing the theme folder and append ownCloud.
    QString oCPath = themePath;
    if( oCPath.endsWith(QLatin1Char('/')) ) {
        oCPath.truncate( oCPath.length()-1 );
    }
    oCPath = oCPath.left( oCPath.lastIndexOf('/'));

    themePath += QLatin1String( "folders");
    oCPath += QLatin1String( "/ownCloud/folders" );

    qDebug() << "Migrator: theme-path: " << themePath;
    qDebug() << "Migrator: ownCloud path: " << oCPath;

    // get a dir listing of the ownCloud folder definitions and copy
    // them over to the theme dir
    QDir oCDir(oCPath);
    oCDir.setFilter( QDir::Files );
    QStringList files = oCDir.entryList();

    foreach( const QString& file, files ) {
        QString escapedAlias = FolderMan::instance()->escapeAlias(file);
        QString themeFile = themePath + QDir::separator() + file;
        QString oCFile = oCPath+QDir::separator()+file;
        if( QFile::copy( oCFile, themeFile ) ) {
            re.append(file);
            qDebug() << "Migrator: Folder definition migrated: " << file;

            // fix the connection entry of the folder definition
            QSettings settings(themeFile, QSettings::IniFormat);
            settings.beginGroup( escapedAlias );
            settings.setValue(QLatin1String("connection"),  Theme::instance()->appName());
            settings.sync();
        }
    }

    return re;
}

}
