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

#ifndef CLIENTPROXY_H
#define CLIENTPROXY_H

#include <QObject>
#include <QNetworkProxy>
#include <QRunnable>
#include <QUrl>

#include <csync.h>
#include "common/utility.h"
#include "owncloudlib.h"

namespace OCC {

class ConfigFile;

/**
 * @brief The ClientProxy class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT ClientProxy : public QObject
{
    Q_OBJECT
public:
    explicit ClientProxy(QObject *parent = nullptr);

    static bool isUsingSystemDefault();
    static void lookupSystemProxyAsync(const QUrl &url, QObject *dst, const char *slot);

    static QString printQNetworkProxy(const QNetworkProxy &proxy);
    static const char *proxyTypeToCStr(QNetworkProxy::ProxyType type);

public slots:
    void setupQtProxyFromConfig();
};

class OWNCLOUDSYNC_EXPORT SystemProxyRunnable : public QObject, public QRunnable
{
    Q_OBJECT
public:
    SystemProxyRunnable(const QUrl &url);
    void run() override;
signals:
    void systemProxyLookedUp(const QNetworkProxy &url);

private:
    QUrl _url;
};

}

#endif // CLIENTPROXY_H
