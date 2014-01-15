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

#include "mirall/updater.h"
#include "mirall/sparkleupdater.h"
#include "mirall/genericupdater.h"

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
#ifdef Q_OS_MAC
    return new SparkleUpdater(QLatin1String("https://updates.owncloud.com/testing/feed.rss"));
#else
    // the best we can do is notify about updates
    return new GenericUpdater(QUrl("https://updates.owncloud.com/testing/"));
#endif
}

} // namespace Mirall
