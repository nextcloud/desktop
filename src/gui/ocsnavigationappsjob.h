/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
