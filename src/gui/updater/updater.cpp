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
#include <QProcess>

#include "updater/updater.h"
#include "updater/sparkleupdater.h"
#include "updater/ocupdater.h"

#include "theme.h"
#include "common/utility.h"
#include "version.h"

#include "config.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcUpdater, "gui.updater", QtInfoMsg)

Updater *Updater::_instance = 0;

Updater *Updater::instance()
{
    if (!_instance) {
        _instance = create();
    }
    return _instance;
}

QUrl Updater::addQueryParams(const QUrl &url)
{
    QUrlQuery query;
    Theme *theme = Theme::instance();
    QString platform = QLatin1String("stranger");
    if (Utility::isLinux()) {
        platform = QLatin1String("linux");
    } else if (Utility::isBSD()) {
        platform = QLatin1String("bsd");
    } else if (Utility::isWindows()) {
        platform = QLatin1String("win32");
    } else if (Utility::isMac()) {
        platform = QLatin1String("macos");
    }

    QString sysInfo = getSystemInfo();
    if (!sysInfo.isEmpty()) {
        query.addQueryItem(QLatin1String("client"), sysInfo);
    }
    query.addQueryItem(QLatin1String("version"), clientVersion());
    query.addQueryItem(QLatin1String("platform"), platform);
    query.addQueryItem(QLatin1String("oem"), theme->appName());

    QString suffix = QString::fromLatin1(MIRALL_STRINGIFY(MIRALL_VERSION_SUFFIX));
    query.addQueryItem(QLatin1String("versionsuffix"), suffix);
    if (suffix.startsWith("daily")
            || suffix.startsWith("nightly")
            || suffix.startsWith("alpha")
            || suffix.startsWith("rc")
            || suffix.startsWith("beta")) {
        query.addQueryItem(QLatin1String("channel"), "beta");
        // FIXME: Provide a checkbox in UI to enable regular versions to switch
        // to beta channel
    }

    QUrl paramUrl = url;
    paramUrl.setQuery(query);
    return paramUrl;
}


QString Updater::getSystemInfo()
{
#ifdef Q_OS_LINUX
    QProcess process;
    process.start(QLatin1String("lsb_release -a"));
    process.waitForFinished();
    QByteArray output = process.readAllStandardOutput();
    qCDebug(lcUpdater) << "Sys Info size: " << output.length();
    if (output.length() > 1024)
        output.clear(); // don't send too much.

    return QString::fromLocal8Bit(output.toBase64());
#else
    return QString();
#endif
}

// To test, cmake with -DAPPLICATION_UPDATE_URL="http://127.0.0.1:8080/test.rss"
Updater *Updater::create()
{
    QUrl updateBaseUrl(QString::fromLocal8Bit(qgetenv("OCC_UPDATE_URL")));
    if (updateBaseUrl.isEmpty()) {
        updateBaseUrl = QUrl(QLatin1String(APPLICATION_UPDATE_URL));
    }
    if (!updateBaseUrl.isValid() || updateBaseUrl.host() == ".") {
        qCWarning(lcUpdater) << "Not a valid updater URL, will not do update check";
        return 0;
    }
    updateBaseUrl = addQueryParams(updateBaseUrl);
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    updateBaseUrl.addQueryItem(QLatin1String("sparkle"), QLatin1String("true"));
    return new SparkleUpdater(updateBaseUrl.toString());
#elif defined(Q_OS_WIN32)
    // the best we can do is notify about updates
    return new NSISUpdater(updateBaseUrl);
#else
    return new PassiveUpdateNotifier(QUrl(updateBaseUrl));
#endif
}


qint64 Updater::Helper::versionToInt(qint64 major, qint64 minor, qint64 patch, qint64 build)
{
    return major << 56 | minor << 48 | patch << 40 | build;
}

qint64 Updater::Helper::currentVersionToInt()
{
    return versionToInt(MIRALL_VERSION_MAJOR, MIRALL_VERSION_MINOR,
        MIRALL_VERSION_PATCH, MIRALL_VERSION_BUILD);
}

qint64 Updater::Helper::stringVersionToInt(const QString &version)
{
    if (version.isEmpty())
        return 0;
    QByteArray baVersion = version.toLatin1();
    int major = 0, minor = 0, patch = 0, build = 0;
    sscanf(baVersion, "%d.%d.%d.%d", &major, &minor, &patch, &build);
    return versionToInt(major, minor, patch, build);
}

QString Updater::clientVersion()
{
    return QString::fromLatin1(MIRALL_STRINGIFY(MIRALL_VERSION_FULL));
}

} // namespace OCC
