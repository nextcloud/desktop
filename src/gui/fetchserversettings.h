/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#pragma once

#include "libsync/accountfwd.h"

#include <QObject>

namespace OCC {
class Capabilities;

class FetchServerSettingsJob : public QObject
{
    Q_OBJECT
public:
    FetchServerSettingsJob(const AccountPtr &account, QObject *parent);

    void start();

Q_SIGNALS:
    /***
     * The version of the server is unsupported
     */
    void unsupportedServerDetected();

    /***
     * We failed to detect the server version
     */
    void unknownServerDetected();

    void finishedSignal();

private:
    void runAsyncUpdates();

    bool checkServerInfo();

    // returns whether the started jobs should be excluded from the retry queue
    bool isAuthJob() const;


    const AccountPtr _account;
};

}
