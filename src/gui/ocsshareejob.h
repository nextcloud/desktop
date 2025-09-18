/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OCSSHAREEJOB_H
#define OCSSHAREEJOB_H

#include "ocsjob.h"

class QJsonDocument;

namespace OCC {

/**
 * @brief The OcsShareeJob class
 * @ingroup gui
 *
 * Fetching sharees from the OCS Sharee API
 */
class OcsShareeJob : public OcsJob
{
    Q_OBJECT
public:
    explicit OcsShareeJob(AccountPtr account);

    /**
     * Get a list of sharees
     *
     * @param path Path to request shares for (default all shares)
     */
    void getSharees(const QString &search, const QString &itemType, int page = 1, int perPage = 50, bool lookup = false);
signals:
    /**
     * Result of the OCS request
     *
     * @param reply The reply
     */
    void shareeJobFinished(const QJsonDocument &reply);

private slots:
    void jobDone(const QJsonDocument &reply);
};
}

#endif // OCSSHAREEJOB_H
