/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef THUMBNAILJOB_H
#define THUMBNAILJOB_H

#include "networkjobs.h"
#include "accountfwd.h"

namespace OCC {

/**
 * @brief Job to fetch a thumbnail for a file
 * @ingroup gui
 *
 * Job that allows fetching a preview (of 150x150 for now) of a given file.
 * Once the job has finished the jobFinished signal will be emitted.
 */
class ThumbnailJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit ThumbnailJob(const QString &path, AccountPtr account, QObject *parent = nullptr);
public slots:
    void start() override;
signals:
    /**
     * @param statusCode the HTTP status code
     * @param reply the content of the reply
     *
     * Signal that the job is done. If the statusCode is 200 (success).
     * If the status code is different the content is invalid.
     */
    void jobFinished(int statusCode, QPixmap reply);
private slots:
    bool finished() override;
};
}

#endif // THUMBNAILJOB_H
