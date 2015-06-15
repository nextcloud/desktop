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

#include <QEventLoop>

#include "account.h"
#include "creds/shibboleth/shibbolethrefresher.h"
#include "creds/shibbolethcredentials.h"

namespace OCC
{

ShibbolethRefresher::ShibbolethRefresher(AccountPtr account, ShibbolethCredentials* creds, CSYNC* csync_ctx, QObject* parent)
    : QObject(parent),
      _account(account),
      _creds(creds),
      _csync_ctx(csync_ctx)
{}

void ShibbolethRefresher::refresh()
{
    QEventLoop loop;

    connect(_creds, SIGNAL(invalidatedAndFetched(QByteArray)),
            this, SLOT(onInvalidatedAndFetched(QByteArray)));
    connect(_creds, SIGNAL(invalidatedAndFetched(QByteArray)),
            &loop, SLOT(quit()));
    QMetaObject::invokeMethod(_creds, "invalidateAndFetch",Qt::QueuedConnection,
                              Q_ARG(AccountPtr, _account));
    loop.exec();
    disconnect(_creds, SIGNAL(invalidatedAndFetched(QByteArray)),
               &loop, SLOT(quit()));
}

void ShibbolethRefresher::onInvalidatedAndFetched(const QByteArray& cookies)
{
    // "cookies" is const and its data() return const void*. We want just void*.
    QByteArray myCookies(cookies);

    disconnect(_creds, SIGNAL(invalidatedAndFetched(QByteArray)),
               this, SLOT(onInvalidatedAndFetched(QByteArray)));
    csync_set_module_property(_csync_ctx, "session_key", myCookies.data());
}

} // namespace OCC
