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

#include <QSharedPointer>
#include <QString>
#include <QVariant>

class QWidget;

namespace Mirall {

class AbstractCredentials;

class MirallConfigFile
{
public:
    MirallConfigFile( const QString& appendix = QString() );

    enum Scope { UserScope, SystemScope };

    QString configPath() const;
    QString configFile() const;
    QString excludeFile(Scope scope) const;

    bool exists();

    bool connectionExists( const QString& = QString() );
    QString defaultConnection() const;

    void writeOwncloudConfig( const QString& connection,
                              const QString& url,
                              AbstractCredentials* credentials);

    AbstractCredentials* getCredentials() const;

    void removeConnection( const QString& connection = QString() );

    QString ownCloudUrl( const QString& connection = QString() ) const;

    // the certs do not depend on a connection.
    QByteArray caCerts();
    void setCaCerts( const QByteArray& );

    bool passwordStorageAllowed(const QString &connection = QString::null );

    QString ownCloudVersion() const;
    void setOwnCloudVersion( const QString& );

    // max count of lines in the log window
    int  maxLogLines() const;
    void setMaxLogLines(int);

    bool ownCloudSkipUpdateCheck( const QString& connection = QString() ) const;
    void setOwnCloudSkipUpdateCheck( bool, const QString& );

    /* Server poll interval in milliseconds */
    int remotePollInterval( const QString& connection = QString() ) const;
    /* Set poll interval. Value in microseconds has to be larger than 5000 */
    void setRemotePollInterval(int interval, const QString& connection = QString() );

    // Custom Config: accept the custom config to become the main one.
    void acceptCustomConfig();
    // Custom Config: remove the custom config file.
    void cleanupCustomConfig();

    bool monoIcons() const;
    void setMonoIcons(bool);

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
    int uploadLimit() const;
    int downloadLimit() const;
    void setUploadLimit(int kbytes);
    void setDownloadLimit(int kbytes);

    static void setConfDir(const QString &value);

    bool optionalDesktopNotifications() const;
    void setOptionalDesktopNotifications(bool show);

    QString seenVersion() const;
    void setSeenVersion(const QString &version);

    void saveGeometry(QWidget *w);
    void restoreGeometry(QWidget *w);

protected:
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
    static QMap< QString, SharedCreds > credentialsPerConfig;
    QString        _customHandle;
};

}
#endif // MIRALLCONFIGFILE_H
