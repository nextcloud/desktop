/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "account.h"

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

    static constexpr char proxyTypeC[] = "Proxy/type";
    static constexpr char proxyHostC[] = "Proxy/host";
    static constexpr char proxyPortC[] = "Proxy/port";
    static constexpr char proxyUserC[] = "Proxy/user";
    static constexpr char proxyPassC[] = "Proxy/pass";
    static constexpr char proxyNeedsAuthC[] = "Proxy/needsAuth";

public slots:
    void setupQtProxyFromConfig();
    void saveProxyConfigurationFromSettings(const QSettings &settings);
    void cleanupGlobalNetworkConfiguration();
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
