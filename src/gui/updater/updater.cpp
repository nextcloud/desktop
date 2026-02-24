/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QUrl>
#include <QUrlQuery>
#include <QProcess>

#include "updater/updater.h"
#include "updater/sparkleupdater.h"
#include "updater/ocupdater.h"
#ifdef Q_OS_LINUX
#include "updater/appimageupdater.h"
#endif

#include "theme.h"
#include "common/utility.h"
#include "version.h"
#include "configfile.h"

#include "config.h"
#include "configfile.h"

#include <QSysInfo>

namespace OCC {

Q_LOGGING_CATEGORY(lcUpdater, "nextcloud.gui.updater", QtInfoMsg)

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
        updateBaseUrl = QUrl(QLatin1String(APPLICATION_UPDATE_URL));
    }
    if (!updateBaseUrl.isValid() || updateBaseUrl.host() == ".") {
        return QUrl();
    }

    auto urlQuery = getQueryParams();

#if defined(Q_OS_MACOS) && defined(HAVE_SPARKLE)
    if (SparkleUpdater::autoUpdaterAllowed()) {
        urlQuery.addQueryItem(QLatin1String("sparkle"), QLatin1String("true"));
    }
#ifdef BUILD_FILE_PROVIDER_MODULE
    urlQuery.addQueryItem(QLatin1String("fileprovider"), QLatin1String("true"));
#endif
#endif
#if defined(Q_OS_WIN)
    urlQuery.addQueryItem(QLatin1String("msi"), QLatin1String("true"));
#endif

    updateBaseUrl.setQuery(urlQuery);

    return updateBaseUrl;
}

QUrlQuery Updater::getQueryParams()
{
    auto platform = QStringLiteral("stranger");
    if (Utility::isLinux()) {
        platform = QStringLiteral("linux");
    } else if (Utility::isBSD()) {
        platform = QStringLiteral("bsd");
    } else if (Utility::isWindows()) {
        platform = QStringLiteral("win32");
    } else if (Utility::isMac()) {
        platform = QStringLiteral("macos");
    }

    QUrlQuery query;
    if (const auto sysInfo = getSystemInfo(); !sysInfo.isEmpty()) {
        query.addQueryItem(QStringLiteral("client"), sysInfo);
    }
    query.addQueryItem(QStringLiteral("version"), clientVersion());
    query.addQueryItem(QStringLiteral("platform"), platform);
    query.addQueryItem(QStringLiteral("osRelease"), QSysInfo::productType());
    query.addQueryItem(QStringLiteral("osVersion"), QSysInfo::productVersion());
    query.addQueryItem(QStringLiteral("kernelVersion"), QSysInfo::kernelVersion());
    query.addQueryItem(QStringLiteral("oem"), Theme::instance()->appName());
    query.addQueryItem(QStringLiteral("buildArch"), QSysInfo::buildCpuArchitecture());
    query.addQueryItem(QStringLiteral("currentArch"), QSysInfo::currentCpuArchitecture());
    query.addQueryItem(QStringLiteral("versionsuffix"), Theme::instance()->versionSuffix());
    query.addQueryItem(QStringLiteral("channel"), ConfigFile().currentUpdateChannel());

    return query;
}

QString Updater::getSystemInfo()
{
#ifdef Q_OS_LINUX
    QProcess process;
    process.start(QLatin1String("lsb_release"), { QStringLiteral("-a") });
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
    auto url = updateUrl();
    qCDebug(lcUpdater) << url;
    if (url.isEmpty()) {
        qCWarning(lcUpdater) << "Not a valid updater URL, will not do update check";
        return nullptr;
    }

#if defined(Q_OS_MACOS) && defined(HAVE_SPARKLE) && defined(BUILD_OWNCLOUD_OSX_BUNDLE)
    if (SparkleUpdater::autoUpdaterAllowed()) {
        return new SparkleUpdater(url);
    }

    return new PassiveUpdateNotifier(url);
#elif defined(Q_OS_WIN32)
    // Also for MSI
    return new NSISUpdater(url);
#elif defined(Q_OS_LINUX)
    // Use AppImageUpdater when running as AppImage, otherwise fall back to passive notifier
    if (AppImageUpdater::isRunningAppImage()) {
        return new AppImageUpdater(url);
    }
    return new PassiveUpdateNotifier(url);
#else
    // the best we can do is notify about updates
    return new PassiveUpdateNotifier(url);
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
