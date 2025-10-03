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

#ifndef OCSNAVIGATIONAPPSJOB_H
#define OCSNAVIGATIONAPPSJOB_H

#include "ocsjob.h"
class QJsonDocument;

namespace OCC {

/**
 * @brief The OcsAppsJob class
 * @ingroup gui
 *
 * Fetching enabled apps from the OCS Apps API
 */
class OcsNavigationAppsJob : public OcsJob
{
    Q_OBJECT
public:
    explicit OcsNavigationAppsJob(AccountPtr account);

    /**
     * Get a list of enabled apps and external sites
     * visible in the Navigation menu
     */
    void getNavigationApps();

signals:
    /**
     * Result of the OCS request
     *
     * @param reply The reply
     * @param statusCode the status code of the response
     */
    void appsJobFinished(const QJsonDocument &reply, int statusCode);

private slots:
    void jobDone(const QJsonDocument &reply, int statusCode);
};
}

#endif // OCSNAVIGATIONAPPSJOB_H
