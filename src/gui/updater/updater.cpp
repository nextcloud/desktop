/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include <QUrl>
#include <QUrlQuery>
#include <QProcess>

#include "updater/updater.h"
#include "updater/sparkleupdater.h"
#include "updater/ocupdater.h"

#ifdef WITH_APPIMAGEUPDATER
#include "updater/appimageupdater.h"
#endif

#include "common/utility.h"
#include "common/version.h"
#include "configfile.h"
#include "theme.h"

#include "config.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcUpdater, "gui.updater", QtInfoMsg)

Updater *Updater::_instance = nullptr;

Updater *Updater::instance()
{
    if (!_instance) {
        _instance = create();
    }
    return _instance;
}

QUrl Updater::updateUrl()
{
    QUrl updateBaseUrl(QString::fromLocal8Bit(qgetenv("OCC_UPDATE_URL")));
    if (updateBaseUrl.isEmpty()) {
        updateBaseUrl = QUrl(QStringLiteral(APPLICATION_UPDATE_URL));
    }
    if (!updateBaseUrl.isValid() || updateBaseUrl.host() == QLatin1String(".")) {
        return QUrl();
    }

    auto urlQuery = getQueryParams();

#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    urlQuery.addQueryItem(QLatin1String("sparkle"), QLatin1String("true"));
#endif

#if defined(Q_OS_WIN)
    urlQuery.addQueryItem(QLatin1String("msi"), QLatin1String("true"));
#endif

    updateBaseUrl.setQuery(urlQuery);

    return updateBaseUrl;
}

QUrlQuery Updater::getQueryParams()
{
    QUrlQuery query;
    Theme *theme = Theme::instance();
    QString platform = QStringLiteral("stranger");
    if (Utility::isLinux()) {
        platform = QStringLiteral("linux");
    } else if (Utility::isBSD()) {
        platform = QStringLiteral("bsd");
    } else if (Utility::isWindows()) {
        platform = QStringLiteral("win32");
    } else if (Utility::isMac()) {
        platform = QStringLiteral("macos");
    }

    query.addQueryItem(QStringLiteral("version"), Version::versionWithBuildNumber().toString());
    query.addQueryItem(QStringLiteral("platform"), platform);
    query.addQueryItem(QStringLiteral("oem"), theme->appName());
    query.addQueryItem(QStringLiteral("buildArch"), QSysInfo::buildCpuArchitecture());
    query.addQueryItem(QStringLiteral("currentArch"), QSysInfo::currentCpuArchitecture());

    // client updater server is now aware of packaging format
    // TODO: add packaging string for other supported platforms, too
#ifdef WITH_APPIMAGEUPDATER
    query.addQueryItem(QStringLiteral("packaging"), QStringLiteral("AppImage"));
#endif


    query.addQueryItem(QStringLiteral("versionsuffix"), OCC::Version::suffix());

    auto channel = ConfigFile().updateChannel();
    if (channel != QLatin1String("stable")) {
        query.addQueryItem(QStringLiteral("channel"), channel);
    }

    return query;
}

// To test, cmake with -DAPPLICATION_UPDATE_URL="http://127.0.0.1:8080/test.rss"
Updater *Updater::create()
{
    auto url = updateUrl();
    qCDebug(lcUpdater) << url;
    if (url.isEmpty()) {
        qCWarning(lcUpdater) << "Not a valid updater URL, will not do update check";
        return nullptr;
    }

#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    return new SparkleUpdater(url);
#elif defined(Q_OS_WIN32)
    // Also for MSI
    return new NSISUpdater(url);
#else
#ifdef WITH_APPIMAGEUPDATER
    if (Utility::runningInAppImage()) {
        return new AppImageUpdater(url);
    }
#endif

    // the best we can do is notify about updates
    return new PassiveUpdateNotifier(url);
#endif
}

} // namespace OCC
