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

#include "config.h"

#include "mirall/mirallconfigfile.h"
#include "mirall/owncloudtheme.h"
#include "mirall/miralltheme.h"
#include "mirall/credentialstore.h"

#include <QtCore>
#include <QtGui>

#define DEFAULT_REMOTE_POLL_INTERVAL 30000 // default remote poll time in milliseconds
#define DEFAULT_LOCAL_POLL_INTERVAL  10000 // default local poll time in milliseconds
#define DEFAULT_POLL_TIMER_EXEED     10

#define CA_CERTS_KEY QLatin1String("CaCertificates")

namespace Mirall {

QString MirallConfigFile::_oCVersion;
bool    MirallConfigFile::_askedUser = false;

MirallConfigFile::MirallConfigFile( const QString& appendix )
    :_customHandle(appendix)
{
}

QString MirallConfigFile::configPath() const
{
    QString dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    if( !dir.endsWith(QLatin1Char('/')) ) dir.append(QLatin1Char('/'));
    return dir;
}

QString MirallConfigFile::excludeFile() const
{
    // prefer sync-exclude.lst, but if it does not exist, check for
    // exclude.lst for compatibility reasonsin the user writeable
    // directories.
    const QString exclFile("sync-exclude.lst");

    QFileInfo fi;
    fi.setFile( configPath(), exclFile );

    if( ! fi.isReadable() ) {
        fi.setFile( configPath(), QLatin1String("exclude.lst") );
    }

    // Check alternative places...
    if( ! fi.isReadable() ) {
#ifdef Q_OS_WIN32
        fi.setFile( QApplication::applicationDirPath(), exclFile );
#endif
#ifdef Q_OS_LINUX
        fi.setFile( QString("/etc/%1").arg(Theme::instance()->appName()), exclFile );
#endif
#ifdef Q_OS_MAC
        // exec path is inside the bundle
        fi.setFile( QApplication::applicationDirPath(),
                    QLatin1String("../Resources/") + exclFile );
#endif
    }

    if( fi.isReadable() ) {
        qDebug() << "  ==> returning exclude file path: " << fi.absoluteFilePath();
        return fi.absoluteFilePath();
    }
    qDebug() << "EMPTY exclude file path!";
    return QString::null;
}

QString MirallConfigFile::configFile() const
{
    if( qApp->applicationName().isEmpty() ) {
        qApp->setApplicationName( Theme::instance()->appName() );
    }
    QString dir = configPath() + Theme::instance()->configFileName();
    if( !_customHandle.isEmpty() ) {
        dir.append( QLatin1Char('_'));
        dir.append( _customHandle );
        qDebug() << "  OO Custom config file in use: " << dir;
    }
    return dir;
}

bool MirallConfigFile::exists()
{
    QFile file( configFile() );
    return file.exists();
}

QString MirallConfigFile::defaultConnection() const
{
    return Theme::instance()->appName();
}

bool MirallConfigFile::connectionExists( const QString& conn )
{
    QString con = conn;
    if( conn.isEmpty() ) con = defaultConnection();

    QSettings settings( configFile(), QSettings::IniFormat);
    settings.setIniCodec( "UTF-8" );

    return settings.contains( QString::fromLatin1("%1/url").arg( conn ) );
}


void MirallConfigFile::writeOwncloudConfig( const QString& connection,
                                            const QString& url,
                                            const QString& user,
                                            const QString& passwd,
                                            bool https, bool skipPwd )
{
    const QString file = configFile();
    qDebug() << "*** writing mirall config to " << file << " Skippwd: " << skipPwd;
    QString pwd( passwd );

    QSettings settings( file, QSettings::IniFormat);
    settings.setIniCodec( "UTF-8" );
    QString cloudsUrl( url );

    if( !cloudsUrl.startsWith( QLatin1String("http")) ) {
        if (https)
            cloudsUrl.prepend(QLatin1String("https://"));
        else
            cloudsUrl.prepend(QLatin1String("http://"));
    }

    settings.beginGroup( connection );
    settings.setValue( QLatin1String("url"), cloudsUrl );
    settings.setValue( QLatin1String("user"), user );
    if( skipPwd ) {
        pwd.clear();
    }

#ifdef WITH_QTKEYCHAIN
    // Password is stored to QtKeyChain now by default in CredentialStore
    // The CredentialStore calls clearPasswordFromConfig after the creds
    // were successfully wiritten to delete the passwd entry from config.
    qDebug() << "Going to delete the password from settings file.";
#else
    // store password into settings file.
    QByteArray pwdba = pwd.toUtf8();
    settings.setValue( QLatin1String("passwd"), QVariant(pwdba.toBase64()) );
#endif
    settings.setValue( QLatin1String("nostoredpassword"), QVariant(skipPwd) );
    settings.sync();
    // check the perms, only read-write for the owner.
    QFile::setPermissions( file, QFile::ReadOwner|QFile::WriteOwner );

    // Store credentials temporar until the config is finalized.
    CredentialStore::instance()->setCredentials( cloudsUrl, user, passwd );

}

// This method is called after the password was successfully stored into the
// QKeyChain in CredentialStore.
void MirallConfigFile::clearPasswordFromConfig( const QString& connection )
{
    const QString file = configFile();
    QString con( defaultConnection() );
    if( !connection.isEmpty() )
        con = connection;

    QSettings settings( file, QSettings::IniFormat);
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup( con ); // FIXME: Connection!
    settings.remove(QLatin1String("passwd"));
    settings.remove(QLatin1String("password"));
    settings.sync();
}

// set the url, called from redirect handling.
void MirallConfigFile::setOwnCloudUrl( const QString& connection, const QString & url )
{
    const QString file = configFile();

    QSettings settings( file, QSettings::IniFormat);
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup( connection );
    settings.setValue( QLatin1String("url"), url );

    settings.sync();
}

QByteArray MirallConfigFile::caCerts( )
{
    QSettings settings( configFile(), QSettings::IniFormat );
    settings.setIniCodec( "UTF-8" );

    QByteArray certs = settings.value( CA_CERTS_KEY ).toByteArray();

    return certs;
}

void MirallConfigFile::setCaCerts( const QByteArray & certs )
{
    const QString file = configFile();

    QSettings settings( file, QSettings::IniFormat);
    settings.setIniCodec( "UTF-8" );
    settings.setValue( CA_CERTS_KEY, certs );

    settings.sync();
}


void MirallConfigFile::removeConnection( const QString& connection )
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    qDebug() << "    removing the config file for connection " << con;

    // Currently its just removing the entire config file
    QSettings settings( configFile(), QSettings::IniFormat);
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup( con );
    settings.remove(QString::null);  // removes all content from the group
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
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup( con );

    // For the WebDAV connect it is required to know which version the server is running
    // because the url changed :-/
    if( webdav && _oCVersion.isEmpty() ) {
        qDebug() << "######## Config does not yet know the ownCloud server version #########";
        qDebug() << "###################### THIS SHOULD NOT HAPPEN! ########################";
    }

    QString url = settings.value( QLatin1String("url") ).toString();
    if( ! url.isEmpty() ) {
        if( ! url.endsWith(QLatin1Char('/'))) url.append(QLatin1String("/"));
        if( webdav ) url.append( QLatin1String("remote.php/webdav/") );
    }

    qDebug() << "Returning configured owncloud url: " << url;

  return url;
}

QString MirallConfigFile::ownCloudUser( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings( configFile(), QSettings::IniFormat );
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup( con );

    QString user = settings.value( QLatin1String("user") ).toString();
    // qDebug() << "Returning configured owncloud user: " << user;

    return user;
}

int MirallConfigFile::remotePollInterval( const QString& connection ) const
{
  QString con( connection );
  if( connection.isEmpty() ) con = defaultConnection();

  QSettings settings( configFile(), QSettings::IniFormat );
  settings.setIniCodec( "UTF-8" );
  settings.beginGroup( con );

  int remoteInterval = settings.value( QLatin1String("remotePollInterval"), DEFAULT_REMOTE_POLL_INTERVAL ).toInt();
  int localInterval  = settings.value(QLatin1String("localPollInterval"), DEFAULT_LOCAL_POLL_INTERVAL ).toInt();
  if( remoteInterval < 2*localInterval ) {
    qDebug() << "WARN: remote poll Interval should at least be twice as local poll interval!";
  }
  if( remoteInterval < 5000 || remoteInterval < localInterval ) {
    qDebug() << "Remote Interval is smaller than local Interval";
    remoteInterval = DEFAULT_REMOTE_POLL_INTERVAL;
  }
  return remoteInterval;
}

int MirallConfigFile::localPollInterval( const QString& connection ) const
{
  QString con( connection );
  if( connection.isEmpty() ) con = defaultConnection();

  QSettings settings( configFile(), QSettings::IniFormat );
  settings.setIniCodec( "UTF-8" );
  settings.beginGroup( con );

  int remoteInterval = settings.value( QLatin1String("remotePollInterval"), DEFAULT_REMOTE_POLL_INTERVAL ).toInt();
  int localInterval  = settings.value(QLatin1String("localPollInterval"), DEFAULT_LOCAL_POLL_INTERVAL ).toInt();
  if( remoteInterval < 2*localInterval ) {
    qDebug() << "WARN: remote poll Interval should at least be twice as local poll interval!";
  }
  if( localInterval < 2500 || remoteInterval < localInterval ) {
    qDebug() << "Remote Interval is smaller than local Interval";
    localInterval = DEFAULT_LOCAL_POLL_INTERVAL;
  }
  return localInterval;
}

int MirallConfigFile::pollTimerExceedFactor( const QString& connection ) const
{
  QString con( connection );
  if( connection.isEmpty() ) con = defaultConnection();

  QSettings settings( configFile(), QSettings::IniFormat );
  settings.setIniCodec( "UTF-8" );
  settings.beginGroup( con );

  int pte = settings.value( QLatin1String("pollTimerExeedFactor"), DEFAULT_POLL_TIMER_EXEED).toInt();

  if( pte < 1 ) pte = DEFAULT_POLL_TIMER_EXEED;

  return pte;
}

MirallConfigFile::CredentialType MirallConfigFile::credentialType( const QString& connection) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    CredentialType ct = Settings;
#ifdef WITH_QTKEYCHAIN
    // In case QtKeyChain is there, use it mandatory.
    ct = KeyChain;
#endif

    QSettings settings( configFile(), QSettings::IniFormat );
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup( con );

    bool skipPwd = settings.value( QLatin1String("nostoredpassword"), false ).toBool();
    if( skipPwd ) {
        ct = User;
    }

    return ct;
}

QString MirallConfigFile::ownCloudPasswd( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings( configFile(), QSettings::IniFormat );
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup( con );

    QString pwd;

    QByteArray pwdba = settings.value(QLatin1String("passwd")).toByteArray();
    if( pwdba.isEmpty() ) {
        // check the password entry, cleartext from before
        // read it and convert to base64, delete the cleartext entry.
        QString p = settings.value(QLatin1String("password")).toString();

        if( ! p.isEmpty() ) {
            // its there, save base64-encoded and delete.

            pwdba = p.toUtf8();
            settings.setValue( QLatin1String("passwd"), QVariant(pwdba.toBase64()) );
            settings.remove( QLatin1String("password") );
            settings.sync();
        }
    }
    pwd = QString::fromUtf8( QByteArray::fromBase64(pwdba) );

    return pwd;
}

QString MirallConfigFile::ownCloudVersion() const
{
    return _oCVersion;
}

void MirallConfigFile::setOwnCloudVersion( const QString& ver)
{
    qDebug() << "** Setting ownCloud Server version to " << ver;
    _oCVersion = ver;
}


bool MirallConfigFile::ownCloudSkipUpdateCheck( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings( configFile(), QSettings::IniFormat );
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup( con );

    bool skipIt = settings.value( QLatin1String("skipUpdateCheck"), false ).toBool();

    return skipIt;
}

int MirallConfigFile::maxLogLines() const
{
    QSettings settings( configFile(), QSettings::IniFormat );
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup(QLatin1String("Logging"));
    int logLines = settings.value( QLatin1String("maxLogLines"), 20000 ).toInt();
    return logLines;
}

// remove a custom config file.
void MirallConfigFile::cleanupCustomConfig()
{
    if( _customHandle.isEmpty() ) {
        qDebug() << "SKipping to erase the main configuration.";
        return;
    }
    QString file = configFile();
    if( QFile::exists( file ) ) {
        QFile::remove( file );
    }
}

// accept a config identified by the customHandle as general config.
void MirallConfigFile::acceptCustomConfig()
{
    if( _customHandle.isEmpty() ) {
        qDebug() << "WRN: Custom Handle is empty. Can not accept.";
        return;
    }

    QString srcConfig = configFile(); // this considers the custom handle

    _customHandle.clear();
    QString targetConfig = configFile();
    QString targetBak = targetConfig + QLatin1String(".bak");

    bool bakOk = false;
    // remove an evtl existing old config backup.
    if( QFile::exists( targetBak ) ) {
        QFile::remove( targetBak );
    }
    // create a backup of the current config.
    bakOk = QFile::rename( targetConfig, targetBak );

    // move the custom config to the master place.
    if( ! QFile::rename( srcConfig, targetConfig ) ) {
        // if the move from custom to master failed, put old backup back.
        if( bakOk ) {
            QFile::rename( targetBak, targetConfig );
        }
    }
    QFile::remove( targetBak );

    // inform the credential store about the password change.
    CredentialStore::instance()->saveCredentials( );
}

QVariant MirallConfigFile::customMedia( customMediaType type )
{
    QVariant re;
    QString key;

    if( type == oCSetupTop ) {
        key = QLatin1String("oCSetupTop");
    } else if( type == oCSetupSide ) {
        key = QLatin1String("oCSetupSide");
    } else if( type == oCSetupBottom) {
        key = QLatin1String("oCSetupBottom");
    } else if( type == oCSetupFixUrl ) {
        key = QLatin1String("oCSetupFixUrl");
    } else if( type == oCSetupResultTop ) {
        key = QLatin1String("oCSetupResultTop");
    } else {
        qDebug() << "Wrong media type.";
    }

    if( !key.isEmpty() ) {
        const QString customFile("custom.ini");
        QFileInfo fi;

#ifdef Q_OS_WIN32
        fi.setFile( QApplication::applicationDirPath(), customFile );
#endif
#ifdef Q_OS_MAC
        // exec path is inside the bundle
        fi.setFile( QApplication::applicationDirPath(),
                    QLatin1String("../Resources/") + customFile );
#endif
#ifdef Q_OS_LINUX
        fi.setFile( QString("/etc/%1").arg(Theme::instance()->appName()), customFile );
#endif
        QSettings settings( fi.absoluteFilePath(), QSettings::IniFormat );

        QString cfg = settings.fileName();
        qDebug() << "Trying to read config ini file at " << cfg;

        settings.setIniCodec( "UTF-8" );
        settings.beginGroup(QLatin1String("GUICustomize"));
        QString val = settings.value( key ).toString();

        // if file is relative, prepend the application dir path.
        QFileInfo checkFi(val);
        if( !val.isEmpty() && checkFi.isRelative() ) {
            checkFi.setFile( QApplication::applicationDirPath(), val );
            val = checkFi.absoluteFilePath();
        }

        if( !val.isEmpty() ) {
            QPixmap pix( val );
            if( pix.isNull() ) {
                // pixmap loading hasn't succeeded. We take the text instead.
                re.setValue( val );
            } else {
                re.setValue( pix );
            }
        }
    }
    return re;
}

void MirallConfigFile::setProxyType(int proxyType,
                  const QString& host,
                  int port,
                  const QString& user,
                  const QString& pass)
{
    QSettings settings( configFile(), QSettings::IniFormat );
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup(QLatin1String("proxy"));

    settings.setValue(QLatin1String("type"), proxyType);
    settings.setValue(QLatin1String("host"), host);
    settings.setValue(QLatin1String("port"), port);
    settings.setValue(QLatin1String("user"), user);
    settings.setValue(QLatin1String("pass"), pass);

    settings.sync();
}

QVariant MirallConfigFile::getValue(const QString& param, const QString& group) const
{
    QSettings settings( configFile(), QSettings::IniFormat );
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup(group);

    return settings.value(param);
}

int MirallConfigFile::proxyType() const
{
    return getValue(QLatin1String("type"), QLatin1String("proxy")).toInt();
}

QString MirallConfigFile::proxyHostName() const
{
    return getValue(QLatin1String("host"), QLatin1String("proxy")).toString();
}

int MirallConfigFile::proxyPort() const
{
    return getValue(QLatin1String("port"), QLatin1String("proxy")).toInt();
}

QString MirallConfigFile::proxyUser() const
{
    return getValue(QLatin1String("user"), QLatin1String("proxy")).toString();
}

QString MirallConfigFile::proxyPassword() const
{
    QByteArray pass = getValue(QLatin1String("pass"), QLatin1String("proxy")).toByteArray();
    return QString::fromUtf8(QByteArray::fromBase64(pass));
}

}
