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

#ifndef SPARKLEUPDATER_H
#define SPARKLEUPDATER_H

#include "mirall/updater.h"

#include <QObject>

namespace Mirall {

class SparkleUpdater : public Updater {
public:
    SparkleUpdater(const QString& appCastUrl);
    ~SparkleUpdater();

    // unused in this updater
    Updater::UpdateState updateState() const { return Updater::NoUpdate; }
    void checkForUpdate();
    void backgroundCheckForUpdate();
private:
    class Private;
    Private *d;
};

} // namespace Mirall

#endif // SPARKLEUPDATER_H
