/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "clientproxy.h"
#include "configfile.h"

#include <QLoggingCategory>
#include <QUrl>
#include <QThreadPool>
#include <QSettings>

namespace OCC {

Q_LOGGING_CATEGORY(lcClientProxy, "nextcloud.sync.clientproxy", QtInfoMsg)

ClientProxy::ClientProxy(QObject *parent)
    : QObject(parent)
{
}

static QNetworkProxy proxyFromConfig(const ConfigFile &cfg)
{
    QNetworkProxy proxy;

    if (cfg.proxyHostName().isEmpty())
        return QNetworkProxy();

    proxy.setHostName(cfg.proxyHostName());
    proxy.setPort(cfg.proxyPort());
    if (cfg.proxyNeedsAuth()) {
        proxy.setUser(cfg.proxyUser());
        proxy.setPassword(cfg.proxyPassword());
    }
    return proxy;
}

bool ClientProxy::isUsingSystemDefault()
{
    OCC::ConfigFile cfg;

    // if there is no config file, default to system proxy.
    if (cfg.exists()) {
        return cfg.proxyType() == QNetworkProxy::DefaultProxy;
    }

    return true;
}

const char *ClientProxy::proxyTypeToCStr(QNetworkProxy::ProxyType type)
{
    switch (type) {
    case QNetworkProxy::NoProxy:
        return "NoProxy";
    case QNetworkProxy::DefaultProxy:
        return "DefaultProxy";
    case QNetworkProxy::Socks5Proxy:
        return "Socks5Proxy";
    case QNetworkProxy::HttpProxy:
        return "HttpProxy";
    case QNetworkProxy::HttpCachingProxy:
        return "HttpCachingProxy";
    case QNetworkProxy::FtpCachingProxy:
        return "FtpCachingProxy";
    default:
        return "NoProxy";
    }
}

QString ClientProxy::printQNetworkProxy(const QNetworkProxy &proxy)
{
    return QStringLiteral("%1://%2:%3").arg(proxyTypeToCStr(proxy.type())).arg(proxy.hostName()).arg(proxy.port());
}

void ClientProxy::setupQtProxyFromConfig()
{
    OCC::ConfigFile cfg;
    int proxyType = QNetworkProxy::DefaultProxy;
    QNetworkProxy proxy;

    // if there is no config file, default to system proxy.
    if (cfg.exists()) {
        proxyType = cfg.proxyType();
        proxy = proxyFromConfig(cfg);
    }

    switch (proxyType) {
        case QNetworkProxy::NoProxy:
            qCInfo(lcClientProxy) << "Set proxy configuration to use NO proxy";
            QNetworkProxyFactory::setUseSystemConfiguration(false);
            QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
            break;
        case QNetworkProxy::DefaultProxy:
            qCInfo(lcClientProxy) << "Set proxy configuration to use the preferred system proxy for http tcp connections";
            {
                QNetworkProxyQuery query;
                query.setProtocolTag("http");
                query.setQueryType(QNetworkProxyQuery::TcpSocket);
                auto proxies = QNetworkProxyFactory::proxyForQuery(query);
                proxy = proxies.first();
            }
            QNetworkProxyFactory::setUseSystemConfiguration(false);
            QNetworkProxy::setApplicationProxy(proxy);
            break;
        case QNetworkProxy::Socks5Proxy:
            proxy.setType(QNetworkProxy::Socks5Proxy);
            qCInfo(lcClientProxy) << "Set proxy configuration to SOCKS5" << printQNetworkProxy(proxy);
            QNetworkProxyFactory::setUseSystemConfiguration(false);
            QNetworkProxy::setApplicationProxy(proxy);
            break;
        case QNetworkProxy::HttpProxy:
            proxy.setType(QNetworkProxy::HttpProxy);
            qCInfo(lcClientProxy) << "Set proxy configuration to HTTP" << printQNetworkProxy(proxy);
            QNetworkProxyFactory::setUseSystemConfiguration(false);
            QNetworkProxy::setApplicationProxy(proxy);
            break;
        default:
            break;
    }
}

void ClientProxy::saveProxyConfigurationFromSettings(const QSettings &settings)
{
    if (settings.value(QLatin1String(proxyTypeC)).isNull()) {
        qCDebug(lcClientProxy) << "No Proxy settings found.";
        return;
    }

    OCC::ConfigFile configFile;
    const auto proxyType = settings.value(QLatin1String(proxyTypeC)).toInt();
    configFile.setProxyType(proxyType,
                            settings.value(QLatin1String(proxyHostC)).toString(),
                            settings.value(QLatin1String(proxyPortC)).toInt(),
                            settings.value(QLatin1String(proxyNeedsAuthC)).toBool(),
                            settings.value(QLatin1String(proxyUserC)).toString(),
                            settings.value(QLatin1String(proxyPassC)).toString());
}

void ClientProxy::cleanupGlobalNetworkConfiguration()
{
    OCC::ConfigFile configFile;
    QSettings settings(configFile.configFile(), QSettings::IniFormat);
    settings.remove(proxyTypeC);
    settings.remove(proxyHostC);
    settings.remove(proxyPortC);
    settings.remove(proxyUserC);
    settings.remove(proxyPassC);
    settings.remove(proxyNeedsAuthC);
    settings.sync();
}

void ClientProxy::lookupSystemProxyAsync(const QUrl &url, QObject *dst, const char *slot)
{
    auto *runnable = new SystemProxyRunnable(url);
    QObject::connect(runnable, SIGNAL(systemProxyLookedUp(QNetworkProxy)), dst, slot);
    QThreadPool::globalInstance()->start(runnable); // takes ownership and deletes
}

SystemProxyRunnable::SystemProxyRunnable(const QUrl &url)
    : QObject()
    , QRunnable()
    , _url(url)
{
}

void SystemProxyRunnable::run()
{
    qRegisterMetaType<QNetworkProxy>("QNetworkProxy");
    QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(_url));

    if (proxies.isEmpty()) {
        emit systemProxyLookedUp(QNetworkProxy(QNetworkProxy::NoProxy));
    } else {
        emit systemProxyLookedUp(proxies.first());
        // FIXME Would we really ever return more?
    }
}
}
