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
#include "mirall/owncloudinfo.h"
#include "mirall/owncloudtheme.h"
#include "mirall/theme.h"
#include "mirall/creds/abstractcredentials.h"
#include "mirall/creds/credentialsfactory.h"
#include <QtCore>
#include <QtGui>

#define DEFAULT_REMOTE_POLL_INTERVAL 30000 // default remote poll time in milliseconds
#define DEFAULT_MAX_LOG_LINES 20000

namespace Mirall {

static const char urlC[] = "url";
static const char authTypeC[] = "authType";

static const char caCertsKeyC[] = "CaCertificates";
static const char remotePollIntervalC[] = "remotePollInterval";
static const char monoIconsC[] = "monoIcons";
static const char optionalDesktopNoficationsC[] = "optionalDesktopNotifications";
static const char skipUpdateCheckC[] = "skipUpdateCheck";
static const char geometryC[] = "geometry";

static const char proxyHostC[] = "Proxy/host";
static const char proxyTypeC[] = "Proxy/type";
static const char proxyPortC[] = "Proxy/port";
static const char proxyUserC[] = "Proxy/user";
static const char proxyPassC[] = "Proxy/pass";
static const char proxyNeedsAuthC[] = "Proxy/needsAuth";

static const char useUploadLimitC[]   = "BWLimit/useUploadLimit";
static const char useDownloadLimitC[] = "BWLimit/useDownloadLimit";
static const char uploadLimitC[]      = "BWLimit/uploadLimit";
static const char downloadLimitC[]    = "BWLimit/downloadLimit";

static const char seenVersionC[] = "Updater/seenVersion";
static const char maxLogLinesC[] = "Logging/maxLogLines";

QString MirallConfigFile::_oCVersion;
QString MirallConfigFile::_confDir = QString::null;
bool    MirallConfigFile::_askedUser = false;
QMap< QString, MirallConfigFile::SharedCreds > MirallConfigFile::credentialsPerConfig;

MirallConfigFile::MirallConfigFile( const QString& appendix )
    :_customHandle(appendix)
{
    QSettings::setDefaultFormat(QSettings::IniFormat);
    if (! credentialsPerConfig.contains(_customHandle)) {
        QString con( _customHandle );
        if( _customHandle.isEmpty() ) con = defaultConnection();

        const QString config = configFile();
        qDebug() << "Loading config: " << config;

        QSettings settings(config, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.beginGroup( con );

        QString type = settings.value( QLatin1String(authTypeC) ).toString();

        qDebug() << "Getting credentials of type " << type << " for " << _customHandle;

        credentialsPerConfig.insert(_customHandle, SharedCreds(CredentialsFactory::create (type)));
    }
}

void MirallConfigFile::setConfDir(const QString &value)
{
    if( value.isEmpty() ) return;

    QFileInfo fi(value);
    if( fi.exists() && fi.isDir() ) {
        qDebug() << "** Using custom config dir " << value;
        _confDir=value;
    }
}

bool MirallConfigFile::optionalDesktopNotifications() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    return settings.value(QLatin1String(optionalDesktopNoficationsC), true).toBool();
}

void MirallConfigFile::setOptionalDesktopNotifications(bool show)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QLatin1String(optionalDesktopNoficationsC), show);
    settings.sync();
}

QString MirallConfigFile::seenVersion() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    return settings.value(QLatin1String(seenVersionC)).toString();
}

void MirallConfigFile::setSeenVersion(const QString &version)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QLatin1String(seenVersionC), version);
    settings.sync();
}

void MirallConfigFile::saveGeometry(QWidget *w)
{
    Q_ASSERT(!w->objectName().isNull());
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(w->objectName());
    settings.setValue(QLatin1String(geometryC), w->saveGeometry());
    settings.sync();
}

void MirallConfigFile::restoreGeometry(QWidget *w)
{
    w->restoreGeometry(getValue(geometryC, w->objectName()).toByteArray());
}

QString MirallConfigFile::configPath() const
{
    if( _confDir.isEmpty() ) {
      _confDir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    }
    QString dir = _confDir;

    if( !dir.endsWith(QLatin1Char('/')) ) dir.append(QLatin1Char('/'));
    return dir;
}

QString MirallConfigFile::excludeFile(Scope scope) const
{
    // prefer sync-exclude.lst, but if it does not exist, check for
    // exclude.lst for compatibility reasons in the user writeable
    // directories.
    const QString exclFile("sync-exclude.lst");
    QFileInfo fi;

    if (scope != SystemScope) {
        fi.setFile( configPath(), exclFile );

        if( ! fi.isReadable() ) {
            fi.setFile( configPath(), QLatin1String("exclude.lst") );
        }
        if( ! fi.isReadable() ) {
            fi.setFile( configPath(), exclFile );
        }
    }

    if (scope != UserScope) {
        // Check alternative places...
        if( ! fi.isReadable() ) {
#ifdef Q_OS_WIN
            fi.setFile( QApplication::applicationDirPath(), exclFile );
#endif
#ifdef Q_OS_UNIX
            fi.setFile( QString("/etc/%1").arg(Theme::instance()->appName()), exclFile );
#endif
#ifdef Q_OS_MAC
            // exec path is inside the bundle
            fi.setFile( QApplication::applicationDirPath(),
                        QLatin1String("../Resources/") + exclFile );
#endif
        }
    }
    qDebug() << "  ==> returning exclude file path: " << fi.absoluteFilePath();
    return fi.absoluteFilePath();
    qDebug() << "EMPTY exclude file path!";
    return QString::null;
}

QString MirallConfigFile::configFile() const
{
    if( qApp->applicationName().isEmpty() ) {
        qApp->setApplicationName( Theme::instance()->appNameGUI() );
    }
    QString file = configPath() + Theme::instance()->configFileName();
    if( !_customHandle.isEmpty() ) {
        file.append( QLatin1Char('_'));
        file.append( _customHandle );
        qDebug() << __PRETTY_FUNCTION__ << "  OO Custom config file in use: " << file;
    }
    return file;
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

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.beginGroup(conn);
    return settings.contains( QLatin1String(urlC) );
}


void MirallConfigFile::writeOwncloudConfig( const QString& connection,
                                            const QString& url,
                                            AbstractCredentials* credentials)
{
    const QString file = configFile();
    qDebug() << "*** writing mirall config to " << file;

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.beginGroup( connection );
    settings.setValue( QLatin1String(urlC), url );
    settings.setValue(QLatin1String(authTypeC), credentials->authType());
    credentialsPerConfig.insert(_customHandle, SharedCreds(credentials));
    settings.sync();
    // check the perms, only read-write for the owner.
    QFile::setPermissions( file, QFile::ReadOwner|QFile::WriteOwner );

    // Store credentials temporar until the config is finalized.
    //ownCloudInfo::instance()->setCredentials( user, passwd, _customHandle );

}

void MirallConfigFile::storeData(const QString& group, const QString& key, const QVariant& value)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.beginGroup(con);
    settings.setValue(key, value);
    settings.sync();
}

QVariant MirallConfigFile::retrieveData(const QString& group, const QString& key) const
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.beginGroup(con);
    return settings.value(key);
}

void MirallConfigFile::removeData(const QString& group, const QString& key)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.beginGroup(con);
    settings.remove(key);
}

bool MirallConfigFile::dataExists(const QString& group, const QString& key) const
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.beginGroup(con);
    return settings.contains(key);
}

QByteArray MirallConfigFile::caCerts( )
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    return settings.value( QLatin1String(caCertsKeyC) ).toByteArray();
}

void MirallConfigFile::setCaCerts( const QByteArray & certs )
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.setIniCodec( "UTF-8" );
    settings.setValue( QLatin1String(caCertsKeyC), certs );
    settings.sync();
}


void MirallConfigFile::removeConnection( const QString& connection )
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    qDebug() << "    removing the config file for connection " << con;

    // Currently its just removing the entire config file
    // TODO: Eh? Shouldn't it try to load a file under configFile() and set it to INI?
    QSettings settings;
    settings.setIniCodec( "UTF-8" );
    settings.beginGroup( con );
    settings.remove(QString::null);  // removes all content from the group
    settings.sync();
}

/*
 * returns the configured owncloud url if its already configured, otherwise an empty
 * string.
 * The returned url always has a trailing hash.
 */
QString MirallConfigFile::ownCloudUrl( const QString& connection) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup( con );

    QString url = settings.value( QLatin1String(urlC) ).toString();
    if( ! url.isEmpty() ) {
        if( ! url.endsWith(QLatin1Char('/'))) url.append(QLatin1String("/"));
    }

    return url;
}

int MirallConfigFile::remotePollInterval( const QString& connection ) const
{
  QString con( connection );
  if( connection.isEmpty() ) con = defaultConnection();

  QSettings settings(configFile(), QSettings::IniFormat);
  settings.setIniCodec("UTF-8");
  settings.beginGroup( con );

  int remoteInterval = settings.value( QLatin1String(remotePollIntervalC), DEFAULT_REMOTE_POLL_INTERVAL ).toInt();
  if( remoteInterval < 5000) {
    qDebug() << "Remote Interval is less than 5 seconds, reverting to" << DEFAULT_REMOTE_POLL_INTERVAL;
    remoteInterval = DEFAULT_REMOTE_POLL_INTERVAL;
  }
  return remoteInterval;
}

void MirallConfigFile::setRemotePollInterval(int interval, const QString &connection )
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    if( interval < 5000 ) {
        qDebug() << "Remote Poll interval of " << interval << " is below fife seconds.";
        return;
    }
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup( con );
    settings.setValue(QLatin1String(remotePollIntervalC), interval );
    settings.sync();
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

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup( con );

    bool skipIt = settings.value( QLatin1String(skipUpdateCheckC), false ).toBool();

    return skipIt;
}

void MirallConfigFile::setOwnCloudSkipUpdateCheck( bool skip, const QString& connection )
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup( con );

    settings.setValue( QLatin1String(skipUpdateCheckC), QVariant(skip) );
    settings.sync();

}

int MirallConfigFile::maxLogLines() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    return settings.value( QLatin1String(maxLogLinesC), DEFAULT_MAX_LOG_LINES ).toInt();
}

void MirallConfigFile::setMaxLogLines( int lines )
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QLatin1String(maxLogLinesC), lines);
    settings.sync();
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

    credentialsPerConfig.insert(QString(), credentialsPerConfig[_customHandle]);
    credentialsPerConfig.remove(_customHandle);
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

    credentialsPerConfig[QString()]->persistForUrl(ownCloudUrl());
}

void MirallConfigFile::setProxyType(int proxyType,
                  const QString& host,
                  int port, bool needsAuth,
                  const QString& user,
                  const QString& pass)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.setValue(QLatin1String(proxyTypeC), proxyType);

    if (proxyType == QNetworkProxy::HttpProxy ||
        proxyType == QNetworkProxy::Socks5Proxy) {
        settings.setValue(QLatin1String(proxyHostC), host);
        settings.setValue(QLatin1String(proxyPortC), port);
        settings.setValue(QLatin1String(proxyNeedsAuthC), needsAuth);
        settings.setValue(QLatin1String(proxyUserC), user);
        settings.setValue(QLatin1String(proxyPassC), pass.toUtf8().toBase64());
    }
    settings.sync();
}

QVariant MirallConfigFile::getValue(const QString& param, const QString& group,
                                    const QVariant& defaultValue) const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    if (!group.isEmpty())
        settings.beginGroup(group);

    return settings.value(param, defaultValue);
}

void MirallConfigFile::setValue(const QString& key, const QVariant &value)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.setValue(key, value);
}

int MirallConfigFile::proxyType() const
{
    return getValue(QLatin1String(proxyTypeC)).toInt();
}

QString MirallConfigFile::proxyHostName() const
{
    return getValue(QLatin1String(proxyHostC)).toString();
}

int MirallConfigFile::proxyPort() const
{
    return getValue(QLatin1String(proxyPortC)).toInt();
}

bool MirallConfigFile::proxyNeedsAuth() const
{
    return getValue(QLatin1String(proxyNeedsAuthC)).toBool();
}

QString MirallConfigFile::proxyUser() const
{
    return getValue(QLatin1String(proxyUserC)).toString();
}

QString MirallConfigFile::proxyPassword() const
{
    QByteArray pass = getValue(proxyPassC).toByteArray();
    return QString::fromUtf8(QByteArray::fromBase64(pass));
}

int MirallConfigFile::useUploadLimit() const
{
    return getValue(useUploadLimitC, QString::null, -1).toInt();
}

bool MirallConfigFile::useDownloadLimit() const
{
    return getValue(useDownloadLimitC, QString::null, false).toBool();
}

void MirallConfigFile::setUseUploadLimit(int val)
{
    setValue(useUploadLimitC, val);
}

void MirallConfigFile::setUseDownloadLimit(bool enable)
{
    setValue(useDownloadLimitC, enable);
}

int MirallConfigFile::uploadLimit() const
{
    return getValue(uploadLimitC, QString::null, 10).toInt();
}

int MirallConfigFile::downloadLimit() const
{
    return getValue(downloadLimitC, QString::null, 80).toInt();
}

void MirallConfigFile::setUploadLimit(int kbytes)
{
    setValue(uploadLimitC, kbytes);
}

void MirallConfigFile::setDownloadLimit(int kbytes)
{
    setValue(downloadLimitC, kbytes);
}

bool MirallConfigFile::monoIcons() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    return settings.value(QLatin1String(monoIconsC), false).toBool();
}

void MirallConfigFile::setMonoIcons(bool useMonoIcons)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QLatin1String(monoIconsC), useMonoIcons);
}

AbstractCredentials* MirallConfigFile::getCredentials() const
{
    return credentialsPerConfig[_customHandle].data();
}

}
