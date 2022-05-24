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
namespace OCC {

class OWNCLOUDSYNC_EXPORT FetchUserInfoResult
{
public:
    FetchUserInfoResult() = default;

    FetchUserInfoResult(const QString &userName, const QString &displayName)
    {
        _userName = userName;
        _displayName = displayName;
    }

    QString userName() const
    {
        return _userName;
    }

    QString displayName() const
    {
        return _displayName;
    };

private:
    QString _userName;
    QString _displayName;
};

class OWNCLOUDSYNC_EXPORT FetchUserInfoJobFactory : public AbstractCoreJobFactory
{
    Q_OBJECT

public:
    static FetchUserInfoJobFactory fromBasicAuthCredentials(QNetworkAccessManager *nam, const QString &username, const QString &token, QObject *parent = nullptr);
    static FetchUserInfoJobFactory fromOAuth2Credentials(QNetworkAccessManager *nam, const QString &bearerToken, QObject *parent = nullptr);

    CoreJob *startJob(const QUrl &url) override;

private:
    FetchUserInfoJobFactory(QNetworkAccessManager *nam, const QString &authHeaderValue, QObject *parent = nullptr);

    QString _authorizationHeader;
};

} // OCC

Q_DECLARE_METATYPE(OCC::FetchUserInfoResult)
