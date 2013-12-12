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
#include "mirall/theme.h"
#include "mirall/utility.h"

#include "creds/abstractcredentials.h"
#include "creds/credentialsfactory.h"

#include <QWidget>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QDebug>
#include <QNetworkProxy>
#include <QHeaderView>

#define DEFAULT_REMOTE_POLL_INTERVAL 30000 // default remote poll time in milliseconds
#define DEFAULT_MAX_LOG_LINES 20000

namespace Mirall {

static const char caCertsKeyC[] = "CaCertificates";
static const char remotePollIntervalC[] = "remotePollInterval";
static const char forceSyncIntervalC[] = "forceSyncInterval";
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

MirallConfigFile::MirallConfigFile()
{
    QSettings::setDefaultFormat(QSettings::IniFormat);

    const QString config = configFile();


    QSettings settings(config, QSettings::IniFormat);
    settings.beginGroup( defaultConnection() );

    qDebug() << "Loading config: " << config << " (URL is " << settings.value("url").toString() << ")";
}

void MirallConfigFile::setConfDir(const QString &value)
{
    QString dirPath = value;
    if( dirPath.isEmpty() ) return;

    QFileInfo fi(dirPath);
    if ( !fi.exists() && !fi.isAbsolute() ) {
        QDir::current().mkdir(dirPath);
        QDir dir = QDir::current();
        dir.cd(dirPath);
        fi.setFile(dir.path());
    }
    if( fi.exists() && fi.isDir() ) {
        dirPath = fi.absoluteFilePath();
        qDebug() << "** Using custom config dir " << dirPath;
        _confDir=dirPath;
    }
}

bool MirallConfigFile::optionalDesktopNotifications() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(optionalDesktopNoficationsC), true).toBool();
}

void MirallConfigFile::setOptionalDesktopNotifications(bool show)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(optionalDesktopNoficationsC), show);
    settings.sync();
}

QString MirallConfigFile::seenVersion() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(seenVersionC)).toString();
}

void MirallConfigFile::setSeenVersion(const QString &version)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(seenVersionC), version);
    settings.sync();
}

void MirallConfigFile::saveGeometry(QWidget *w)
{
    Q_ASSERT(!w->objectName().isNull());
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(w->objectName());
    settings.setValue(QLatin1String(geometryC), w->saveGeometry());
    settings.sync();
}

void MirallConfigFile::restoreGeometry(QWidget *w)
{
    w->restoreGeometry(getValue(geometryC, w->objectName()).toByteArray());
}

void MirallConfigFile::saveGeometryHeader(QHeaderView *header)
{
    if(!header) return;
    Q_ASSERT(!header->objectName().isNull());

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(header->objectName());
    settings.setValue(QLatin1String(geometryC), header->saveState());
    settings.sync();
}

void MirallConfigFile::restoreGeometryHeader(QHeaderView *header)
{
    if(!header) return;
    Q_ASSERT(!header->objectName().isNull());

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(header->objectName());
    header->restoreState(getValue(geometryC, header->objectName()).toByteArray());
}


QString MirallConfigFile::configPath() const
{
    if( _confDir.isEmpty() ) {
        _confDir = Utility::dataLocation();
    }
    QString dir = _confDir;

    if( !dir.endsWith(QLatin1Char('/')) ) dir.append(QLatin1Char('/'));
    return dir;
}

QString MirallConfigFile::configPathWithAppName() const
{
    //HACK
    return QFileInfo( configFile() ).dir().absolutePath().append("/");
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
            fi.setFile( QCoreApplication::applicationDirPath(), exclFile );
#endif
#ifdef Q_OS_UNIX
            fi.setFile( QString( SYSCONFDIR "/%1").arg(Theme::instance()->appName()), exclFile );
#endif
#ifdef Q_OS_MAC
            // exec path is inside the bundle
            fi.setFile( QCoreApplication::applicationDirPath(),
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
    return configPath() + Theme::instance()->configFileName();
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

void MirallConfigFile::storeData(const QString& group, const QString& key, const QVariant& value)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    settings.setValue(key, value);
    settings.sync();
}

QVariant MirallConfigFile::retrieveData(const QString& group, const QString& key) const
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    return settings.value(key);
}

void MirallConfigFile::removeData(const QString& group, const QString& key)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    settings.remove(key);
}

bool MirallConfigFile::dataExists(const QString& group, const QString& key) const
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    return settings.contains(key);
}

QByteArray MirallConfigFile::caCerts( )
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value( QLatin1String(caCertsKeyC) ).toByteArray();
}

void MirallConfigFile::setCaCerts( const QByteArray & certs )
{
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.setValue( QLatin1String(caCertsKeyC), certs );
    settings.sync();
}

int MirallConfigFile::remotePollInterval( const QString& connection ) const
{
  QString con( connection );
  if( connection.isEmpty() ) con = defaultConnection();

  QSettings settings(configFile(), QSettings::IniFormat);
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
    settings.beginGroup( con );
    settings.setValue(QLatin1String(remotePollIntervalC), interval );
    settings.sync();
}

quint64 MirallConfigFile::forceSyncInterval(const QString& connection) const
{
    uint pollInterval = remotePollInterval(connection);

    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup( con );

    quint64 interval = settings.value( QLatin1String(forceSyncIntervalC), 10 * pollInterval ).toULongLong();
    if( interval < pollInterval) {
        qDebug() << "Force sync interval is less than the remote poll inteval, reverting to" << pollInterval;
        interval = pollInterval;
    }
    return interval;
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
    settings.beginGroup( con );

    bool skipIt = settings.value( QLatin1String(skipUpdateCheckC), false ).toBool();

    return skipIt;
}

void MirallConfigFile::setOwnCloudSkipUpdateCheck( bool skip, const QString& connection )
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup( con );

    settings.setValue( QLatin1String(skipUpdateCheckC), QVariant(skip) );
    settings.sync();

}

int MirallConfigFile::maxLogLines() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value( QLatin1String(maxLogLinesC), DEFAULT_MAX_LOG_LINES ).toInt();
}

void MirallConfigFile::setMaxLogLines( int lines )
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(maxLogLinesC), lines);
    settings.sync();
}

void MirallConfigFile::setProxyType(int proxyType,
                  const QString& host,
                  int port, bool needsAuth,
                  const QString& user,
                  const QString& pass)
{
    QSettings settings(configFile(), QSettings::IniFormat);

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
    if (!group.isEmpty())
        settings.beginGroup(group);

    return settings.value(param, defaultValue);
}

void MirallConfigFile::setValue(const QString& key, const QVariant &value)
{
    QSettings settings(configFile(), QSettings::IniFormat);

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
    return getValue(useUploadLimitC, QString::null, 0).toInt();
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
    return settings.value(QLatin1String(monoIconsC), false).toBool();
}

void MirallConfigFile::setMonoIcons(bool useMonoIcons)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(monoIconsC), useMonoIcons);
}

}
