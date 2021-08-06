/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include <QLoggingCategory>
#include <QString>
#include <QCoreApplication>

#include "account.h"
#include "common/asserts.h"
#include "creds/abstractcredentials.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcCredentials, "sync.credentials", QtInfoMsg)

AbstractCredentials::AbstractCredentials()
    : _account(nullptr)
    , _wasFetched(false)
{
}

void AbstractCredentials::setAccount(Account *account)
{
    OC_ENFORCE_X(!_account, "should only setAccount once");
    _account = account;
}

} // namespace OCC
