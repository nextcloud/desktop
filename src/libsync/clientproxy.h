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

#ifndef CLIENTPROXY_H
#define CLIENTPROXY_H

#include <QObject>
#include <QNetworkProxy>

#include <csync.h>
#include "utility.h"

namespace OCC {

class ConfigFile;

class OWNCLOUDSYNC_EXPORT ClientProxy : public QObject
{
    Q_OBJECT
public:
    explicit ClientProxy(QObject *parent = 0);

signals:

public slots:
    void setCSyncProxy( const QUrl& url, CSYNC *csync_ctx );
    void setupQtProxyFromConfig();

private:
    QNetworkProxy proxyFromConfig(const ConfigFile& cfg);
    const char* proxyTypeToCStr(QNetworkProxy::ProxyType type);
};

}

#endif // CLIENTPROXY_H
