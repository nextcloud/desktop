/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_CREDS_SHIBBOLETH_REFRESHER_H
#define MIRALL_CREDS_SHIBBOLETH_REFRESHER_H

#include <QObject>

#include <csync.h>

class QByteArray;

namespace OCC
{

class Account;
class ShibbolethCredentials;

/**
 * @brief The ShibbolethRefresher class
 * @ingroup gui
 */
class ShibbolethRefresher : public QObject
{
    Q_OBJECT

public:
    ShibbolethRefresher(AccountPtr account, ShibbolethCredentials* creds, CSYNC* csync_ctx, QObject* parent = 0);

    void refresh();

private Q_SLOTS:
    void onInvalidatedAndFetched(const QByteArray& cookieData);

private:
    AccountPtr _account;
    ShibbolethCredentials* _creds;
    CSYNC* _csync_ctx;
};

} // namespace OCC

#endif
