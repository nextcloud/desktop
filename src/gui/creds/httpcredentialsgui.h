/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "creds/httpcredentials.h"
#include <QPointer>
#include <QTcpServer>

namespace OCC {

/**
 * @brief The HttpCredentialsGui class
 * @ingroup gui
 */
class HttpCredentialsGui : public HttpCredentials
{
    Q_OBJECT
public:
    explicit HttpCredentialsGui()
        : HttpCredentials()
    {
    }
    HttpCredentialsGui(const QString &user, const QString &password,
            const QByteArray &clientCertBundle, const QByteArray &clientCertPassword)
        : HttpCredentials(user, password, clientCertBundle, clientCertPassword)
    {
    }

    void askFromUser() override;

    static QString requestAppPasswordText(const Account *account);
private slots:
    void showDialog();
    void askFromUserAsync();
};

} // namespace OCC
