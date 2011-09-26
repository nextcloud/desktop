/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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

#include "sitecopyconfig.h"

namespace Mirall {

SitecopyConfig::SitecopyConfig()
{

}

void SitecopyConfig::writeSiteConfig( const QString& localPath,
                                      const QString& siteAlias, const QString& url,
                                      const QString& user, const QString& passwd )
{
  parseSiteConfig( );
  // now all configured sites are in the hash mSites.
  foreach( QString site, mSites.keys() ) {
    qDebug() << "Known site: " << site;
  }

  // check if there is already a cloudia site.
  QHash<QString, QString> cloudiaDefs;

  if( mSites.contains( siteAlias ) ) {
    cloudiaDefs = mSites[siteAlias];
  }
  QUrl ocUrl( url );
  QString host = ocUrl.host();
  QString path = ocUrl.path();
  qDebug() << "Split url, host: " << host << " and path: " << path;
  // FIXME: Check if user settings are overwritten
  cloudiaDefs["server"]   = host;
  cloudiaDefs["protocol"] = "webdav";
  cloudiaDefs["local"]    = localPath;
  cloudiaDefs["remote"]   =  path + "/files/webdav.php";
  cloudiaDefs["password"] = passwd;

  // QString user( getenv("USER"));
  cloudiaDefs["username"] = user;
  mSites.insert( siteAlias, cloudiaDefs );

  qDebug() << "** Now Writing!";

  QFile configFile( QDir::homePath() + "/.sitecopyrc" );
  if( !configFile.open( QIODevice::WriteOnly | QIODevice::Text )) {
    qDebug() << "Failed to open config file to write.";
  }
  QTextStream out(&configFile);

  foreach( QString site, mSites.keys() ) {
    QHash<QString, QString> configs = mSites[site];
    qDebug() << "Writing site: <" << site << ">";
    out << "site " << site << '\n';
    foreach( QString configKey, configs.keys() ) {
      out << "  " << configKey << " " << configs[configKey] << '\n';
      qDebug() << "  Setting: " << configKey << ": " << configs[configKey];
    }
     out << '\n';
   }
   configFile.close();
   configFile.setPermissions( QFile::ReadOwner | QFile::WriteOwner );

   // check if the .sitecopy dir is there, if not, create
   if( !QFile::exists( QDir::homePath() + "/.sitecopy" ) ) {
     QDir home( QDir::homePath() );
     if( home.mkdir( ".sitecopy" ) ) {
       QFile::setPermissions( QDir::homePath() + "/.sitecopy",
                              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeUser );
     }
   }
 }

bool SitecopyConfig::parseSiteConfig( )
{
  QFile configFile( QDir::homePath() + "/.sitecopyrc" );
  if( ! configFile.exists() ) {
    qDebug() << "No site config file. Create one!";
    return false;
  }
  if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text))
    return false;

  while (!configFile.atEnd()) {
    QByteArray line = configFile.readLine();
    processConfigLine( line.simplified() );
  }
  if( ! mCurrSiteName.isEmpty() ) {
      mSites.insert(mCurrSiteName, mCurrSite);
  } else {
      qDebug() << "ERR: No current Site name found";
      return false;
  }
  qDebug() << "The end of parsing.";
  return true;
}

void SitecopyConfig::processConfigLine( const QString& line )
{
  if( line.isEmpty() ) return;
  QStringList li = line.split( QRegExp( "\\s+") );
  if( li.size() < 2 ) {
      qDebug() << "Unable to parse line, return: " << line;
      return;
  }
  const QString key = li[0];
  const QString val = li[1];
  qDebug() << "Key: " << key << ", Value " << val;

  if( key == QString::fromLatin1("site") && !val.isEmpty() ) {
    qDebug() << "Found site " << val;
    if( !mCurrSiteName.isEmpty() && !mCurrSite.isEmpty() ) {
      // save to the sites hash first
      mSites.insert( mCurrSiteName, mCurrSite );
      mCurrSite.clear();
    }
    if( !val.isEmpty() ) {
        mCurrSiteName = val;
        // new site entry.
        if( mSites.contains( val ) ) {
            mCurrSite = mSites[val];
        }
    } else {
        qDebug() << "Found empty site name, can not parse, fix manually!";
        mCurrSiteName.clear();
    }
  } else {
    mCurrSite.insert( key, val );
  }
}

}
