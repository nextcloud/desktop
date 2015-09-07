/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef THUMBNAILJOB_H
#define THUMBNAILJOB_H

#include "networkjobs.h"
#include "accountfwd.h"

namespace OCC {

class ThumbnailJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit ThumbnailJob(const QString& path, AccountPtr account, QObject* parent = 0);
public slots:
    void start() Q_DECL_OVERRIDE;
signals:
    void jobFinished(int statusCode, QByteArray reply);
private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
private:
    QUrl _url;
};

}

#endif // THUMBNAILJOB_H
