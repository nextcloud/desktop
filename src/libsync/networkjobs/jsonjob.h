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

#include "networkjobs.h"

#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

namespace OCC {


class OWNCLOUDSYNC_EXPORT JsonJob : public SimpleNetworkJob
{
    Q_OBJECT
public:
    using SimpleNetworkJob::SimpleNetworkJob;
    virtual ~JsonJob();

    const QJsonObject &data() const;
    const QJsonParseError &parseError() const;

protected:
    bool finished() override;

    virtual void parse(const QByteArray &data);


private:
    QJsonParseError _parseError;
    QJsonObject _data;
};


/**
 * @brief Job to check an API that return JSON
 *
 * Note! you need to be in the connected state before calling this because of a server bug:
 * https://github.com/owncloud/core/issues/12930
 *
 * To be used like this:
 * \code
 * _job = new JsonApiJob(account, QLatin1String("ocs/v1.php/foo/bar"), this);
 * connect(job, SIGNAL(jsonReceived(QJsonDocument)), ...)
 * The received QVariantMap is null in case of error
 * \encode
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT JsonApiJob : public JsonJob
{
    Q_OBJECT
public:
    explicit JsonApiJob(AccountPtr account, const QString &path, const QByteArray &verb, const UrlQuery &arguments, const QNetworkRequest &req, QObject *parent);
    explicit JsonApiJob(AccountPtr account, const QString &path, const UrlQuery &arguments, const QNetworkRequest &req, QObject *parent)
        : JsonApiJob(account, path, QByteArrayLiteral("GET"), arguments, req, parent)
    {
    }

    // the OCS status code: 100 (!) for success
    int ocsStatus() const;

    const QString &ocsMessage() const;

    // whether the job was successful
    bool ocsSuccess() const;

private:
    // using JsonJob::JsonJob;/

    int _ocsStatus = 0;
    QString _ocsMessage;

protected:
    virtual void parse(const QByteArray &data) override;
};

}
