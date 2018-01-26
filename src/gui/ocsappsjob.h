/*
 * Copyright (C) by Camila Ayres <camila@nextcloud.com>
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

#ifndef OCSAPPSJOB_H
#define OCSAPPSJOB_H

#include "ocsjob.h"
class QJsonDocument;

namespace OCC {

/**
 * @brief The OcsAppsJob class
 * @ingroup gui
 *
 * Fetching enabled apps from the OCS Apps API
 */
class OcsAppsJob : public OcsJob
{
    Q_OBJECT
public:
    explicit OcsAppsJob(AccountPtr account);

    /**
     * Get a list of enabled apps for the current user
     */
    void getApps();

signals:
    /**
     * Result of the OCS request
     *
     * @param reply The reply
     */
    void appsJobFinished(const QJsonDocument &reply);

private slots:
    void jobDone(const QJsonDocument &reply);
};
}

#endif // OCSAPPSJOB_H
