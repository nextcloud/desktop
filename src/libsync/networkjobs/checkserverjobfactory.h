/*
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

#pragma once

#include "abstractcorejob.h"

#include <QJsonObject>

namespace OCC {

class OWNCLOUDSYNC_EXPORT CheckServerJobResult
{

public:
    CheckServerJobResult() = default;
    CheckServerJobResult(const QJsonObject &statusObject, const QUrl &serverUrl);

    QJsonObject statusObject() const;
    QUrl serverUrl() const;

private:
    const QJsonObject _statusObject;
    const QUrl _serverUrl;
};


class OWNCLOUDSYNC_EXPORT CheckServerJobFactory : public AbstractCoreJobFactory
{
    Q_OBJECT

public:
    using AbstractCoreJobFactory::AbstractCoreJobFactory;

    CoreJob *startJob(const QUrl &url) override;

private:
    int _maxRedirectsAllowed = 5;
};

} // OCC

Q_DECLARE_METATYPE(OCC::CheckServerJobResult)
