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

#ifndef MIRALLCONFIGFILE_H
#define MIRALLCONFIGFILE_H

#include "owncloudlib.h"
#include <QSharedPointer>
#include <QString>
#include <QVariant>

class QWidget;
class QHeaderView;

namespace OCC {

class AbstractCredentials;

class OWNCLOUDSYNC_EXPORT ConfigFile
{
public:
    ConfigFile();

    enum Scope { UserScope, SystemScope };

    QString configPath() const;
    QString configPathWithAppName() const;
    QString configFile() const;
    QString excludeFile(Scope scope) const;
    static QString excludeFileFromSystem(); // doesn't access config dir

    bool exists();

    QString defaultConnection() const;

    // the certs do not depend on a connection.
    QByteArray caCerts();
    void setCaCerts( const QByteArray& );

    bool passwordStorageAllowed(const QString &connection = QString::null );

    QString ownCloudVersion() const;
    void setOwnCloudVersion( const QString& );

    // max count of lines in the log window
    int  maxLogLines() const;
    void setMaxLogLines(int);

    /* Server poll interval in milliseconds */
    int remotePollInterval( const QString& connection = QString() ) const;
    /* Set poll interval. Value in microseconds has to be larger than 5000 */
    void setRemotePollInterval(int interval, const QString& connection = QString() );

    /* Force sync interval, in milliseconds */
    quint64 forceSyncInterval(const QString &connection = QString()) const;

    bool monoIcons() const;
    void setMonoIcons(bool);

    bool crashReporter() const;
    void setCrashReporter(bool enabled);

    // proxy settings
    void setProxyType(int proxyType,
                      const QString& host = QString(),
                      int port = 0, bool needsAuth = false,
                      const QString& user = QString(),
                      const QString& pass = QString());

    int proxyType() const;
    QString proxyHostName() const;
    int proxyPort() const;
    bool proxyNeedsAuth() const;
    QString proxyUser() const;
    QString proxyPassword() const;
    
    /** 0: no limit, 1: manual, >0: automatic */
    int useUploadLimit() const;
    bool useDownloadLimit() const;
    void setUseUploadLimit(int);
    void setUseDownloadLimit(bool);
    /** in kbyte/s */
    int uploadLimit() const;
    int downloadLimit() const;
    void setUploadLimit(int kbytes);
    void setDownloadLimit(int kbytes);

    static void setConfDir(const QString &value);

    bool optionalDesktopNotifications() const;
    void setOptionalDesktopNotifications(bool show);

    int timeout() const;

    void saveGeometry(QWidget *w);
    void restoreGeometry(QWidget *w);

    // installer
    bool skipUpdateCheck( const QString& connection = QString() ) const;
    void setSkipUpdateCheck( bool, const QString& );

    QString lastVersion() const;
    void setLastVersion(const QString &version);

    void saveGeometryHeader(QHeaderView *header);
    void restoreGeometryHeader(QHeaderView *header);

protected:
    QVariant getPolicySetting(const QString& policy, const QVariant& defaultValue = QVariant()) const;
    void storeData(const QString& group, const QString& key, const QVariant& value);
    QVariant retrieveData(const QString& group, const QString& key) const;
    void removeData(const QString& group, const QString& key);
    bool dataExists(const QString& group, const QString& key) const;

private:
    QVariant getValue(const QString& param, const QString& group = QString::null,
                      const QVariant& defaultValue = QVariant()) const;
    void setValue(const QString& key, const QVariant &value);

private:
    typedef QSharedPointer< AbstractCredentials > SharedCreds;

    static bool    _askedUser;
    static QString _oCVersion;
    static QString _confDir;
};

}
#endif // MIRALLCONFIGFILE_H
