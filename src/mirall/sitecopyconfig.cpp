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
                                      const QString& user, const QString& passwd,
                                      const QString& remoteFolder )
{
  parseSiteConfig( );
  qDebug() << "*** Writing Site Alias " << siteAlias;

  // now all configured sites are in the hash _Sites.
  foreach( QString site, _Sites.keys() ) {
    qDebug() << "Known site: " << site;
  }

  // check if there is already a oc site.
  QHash<QString, QString> ocDefs;

  if( _Sites.contains( siteAlias ) ) {
    ocDefs = _Sites[siteAlias];
  }
  QUrl ocUrl( url );
  QString host = ocUrl.host();
  QString path = ocUrl.path();
  qDebug() << "Split url, host: " << host << " and path: " << path;
  // FIXME: Check if user settings are overwritten
  ocDefs["server"]   = host;
  ocDefs["protocol"] = "webdav";
  ocDefs["local"]    = localPath;
  QString webdavBase = "files/webdav.php";
  if( !remoteFolder.isEmpty() ) {
    webdavBase += "/" + remoteFolder;
  }
  if( !path.endsWith( QChar('/')) ) {
    webdavBase.prepend( QChar('/') );
  }
  ocDefs["remote"]   = path + webdavBase;
  if( ! passwd.isEmpty() ) {
    ocDefs["password"] = passwd;
  }

  ocDefs["username"] = user;
  _Sites.insert( siteAlias, ocDefs );

  qDebug() << "** Now Writing!";

  QFile configFile( QDir::homePath() + "/.sitecopyrc" );
  if( !configFile.open( QIODevice::WriteOnly | QIODevice::Text )) {
    qDebug() << "Failed to open config file to write.";
  }
  QTextStream out(&configFile);

  foreach( QString site, _Sites.keys() ) {
    QHash<QString, QString> configs = _Sites[site];
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
  if( ! _CurrSiteName.isEmpty() ) {
      _Sites.insert(_CurrSiteName, _CurrSite);
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
    if( !_CurrSiteName.isEmpty() && !_CurrSite.isEmpty() ) {
      // save to the sites hash first
      _Sites.insert( _CurrSiteName, _CurrSite );
      _CurrSite.clear();
    }
    if( !val.isEmpty() ) {
        _CurrSiteName = val;
        // new site entry.
        if( _Sites.contains( val ) ) {
            _CurrSite = _Sites[val];
        }
    } else {
        qDebug() << "Found empty site name, can not parse, fix manually!";
        _CurrSiteName.clear();
    }
  } else {
    _CurrSite.insert( key, val );
  }
}

}
