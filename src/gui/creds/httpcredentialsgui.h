/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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
#include "creds/httpcredentials.h"

namespace OCC
{

/**
 * @brief The HttpCredentialsGui class
 * @ingroup gui
 */
class HttpCredentialsGui : public HttpCredentials {
    Q_OBJECT
public:
    explicit HttpCredentialsGui() : HttpCredentials() {}
    HttpCredentialsGui(const QString& user, const QString& password, const QSslCertificate& certificate, const QSslKey& key) : HttpCredentials(user, password, certificate, key) {}
    void askFromUser() Q_DECL_OVERRIDE;
    Q_INVOKABLE void askFromUserAsync();

    static QString requestAppPasswordText(const Account *account);
};

} // namespace OCC

