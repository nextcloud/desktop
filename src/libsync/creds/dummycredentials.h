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

#ifndef MIRALL_CREDS_DUMMY_CREDENTIALS_H
#define MIRALL_CREDS_DUMMY_CREDENTIALS_H

#include "creds/abstractcredentials.h"

namespace OCC
{

class DummyCredentials : public AbstractCredentials
{
    Q_OBJECT

public:

    QString _user;
    QString _password;
    void syncContextPreInit(CSYNC* ctx) Q_DECL_OVERRIDE;
    void syncContextPreStart(CSYNC* ctx) Q_DECL_OVERRIDE;
    bool changed(AbstractCredentials* credentials) const Q_DECL_OVERRIDE;
    QString authType() const Q_DECL_OVERRIDE;
    QString user() const Q_DECL_OVERRIDE;
    QNetworkAccessManager* getQNAM() const Q_DECL_OVERRIDE;
    bool ready() const Q_DECL_OVERRIDE;
    bool stillValid(QNetworkReply *reply) Q_DECL_OVERRIDE;
    void fetch(Account*) Q_DECL_OVERRIDE;
    void persist(Account*) Q_DECL_OVERRIDE;
    void invalidateToken(Account *) Q_DECL_OVERRIDE {}
};

} // namespace OCC

#endif
