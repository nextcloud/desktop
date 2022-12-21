/*
 * Copyright (C) Hannah von Reth <hannah.vonreth@owncloud.com>
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#include "abstractcorejob.h"

using namespace OCC;

AbstractCoreJobFactory::AbstractCoreJobFactory(QNetworkAccessManager *nam)
    : QObject()
    , _nam(nam)
{
}

AbstractCoreJobFactory::~AbstractCoreJobFactory()
{
}

QNetworkAccessManager *AbstractCoreJobFactory::nam() const
{
    return _nam;
}

void AbstractCoreJobFactory::setJobResult(CoreJob *job, const QVariant &result)
{
    job->setResult(result);
}

void AbstractCoreJobFactory::setJobError(CoreJob *job, const QString &errorMessage)
{
    job->setError(errorMessage);
}

const QVariant &CoreJob::result() const
{
    return _result;
}

const QString &CoreJob::errorMessage() const
{
    return _errorMessage;
}

QNetworkReply *CoreJob::reply() const
{
    return _reply;
}

bool CoreJob::success() const
{
    return _success;
}

void CoreJob::setResult(const QVariant &result)
{
    assertNotFinished();

    _success = true;
    _result = result;

    Q_EMIT finished();
}

void CoreJob::setError(const QString &errorMessage)
{
    assertNotFinished();

    _errorMessage = errorMessage;

    Q_EMIT finished();
}

CoreJob::CoreJob(QNetworkReply *reply, QObject *parent)
    : QObject(parent)
    , _reply(reply)
{
    Q_ASSERT(!qobject_cast<AbstractCoreJobFactory *>(parent));
    _reply->setParent(this);
}

void CoreJob::assertNotFinished()
{
    Q_ASSERT(_result.isNull());
    Q_ASSERT(_errorMessage.isEmpty());
}
