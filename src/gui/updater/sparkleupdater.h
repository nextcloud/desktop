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

#ifndef SPARKLEUPDATER_H
#define SPARKLEUPDATER_H

#include "updater/updater.h"

#include <QObject>

namespace OCC {

class SparkleUpdater : public Updater
{
    Q_OBJECT
public:
    SparkleUpdater(const QUrl &appCastUrl);
    ~SparkleUpdater();

    void setUpdateUrl(const QUrl &url);

    // unused in this updater
    void checkForUpdate() Q_DECL_OVERRIDE;
    void backgroundCheckForUpdate() Q_DECL_OVERRIDE;
    bool handleStartup() Q_DECL_OVERRIDE { return false; }

    QString statusString();

private:
    class Private;
    Private *d;
};

} // namespace OCC

#endif // SPARKLEUPDATER_H
