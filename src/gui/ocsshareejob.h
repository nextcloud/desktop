/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
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

#ifndef OCSSHAREEJOB_H
#define OCSSHAREEJOB_H

#include "ocsjob.h"
#include <QVariantMap>

namespace OCC {

/**
 * @brief The OcsShareeJob class
 * @ingroup gui
 *
 * Fetching sharees from the OCS Sharee API
 */
class OcsShareeJob : public OcsJob {
    Q_OBJECT
public:

    explicit OcsShareeJob(AccountPtr account);

    /**
     * Get a list of sharees
     *
     * @param path Path to request shares for (default all shares)
     */
    void getSharees(const QString& search, const QString& itemType, int page = 1, int perPage = 50);
signals:
    /**
     * Result of the OCS request
     *
     * @param reply The reply
     */
    void shareeJobFinished(const QVariantMap &reply);

private slots:
    void jobDone(const QVariantMap &reply);

};

}

#endif // OCSSHAREEJOB_H
