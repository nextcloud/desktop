/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "updater/updater.h"
#include "updater/sparkleupdater.h"
#include "updater/ocupdater.h"

#include "mirall/version.h"

#include "config.h"

namespace Mirall {

Updater *Updater::_instance = 0;

Updater * Updater::instance()
{
    if(!_instance) {
        _instance = create();
    }
    return _instance;
}

Updater *Updater::create()
{
    QString updateBaseUrl(QLatin1String(APPLICATION_UPDATE_URL));
#ifdef Q_OS_MAC
    return new SparkleUpdater(updateBaseUrl+QLatin1String("/rss/");
#elif defined (Q_OS_WIN32)
    // the best we can do is notify about updates
    return new NSISUpdater(QUrl(updateBaseUrl));
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

qint64 Updater::Helper::stringVersionToInt(const QString& version)
{
    QByteArray baVersion = version.toLatin1();
    int major = 0, minor = 0, patch = 0, build = 0;
    sscanf(baVersion, "%d.%d.%d.%d", &major, &minor, &patch, &build);
    return versionToInt(major, minor, patch, build);
}


} // namespace Mirall
