/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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
#include "common/utility.h"
#include "account.h"
#include "simplesslerrorhandler.h"

namespace OCC {

bool SimpleSslErrorHandler::handleErrors(QList<QSslError> errors, const QSslConfiguration &conf, QList<QSslCertificate> *certs, OCC::AccountPtr account)
{
    Q_UNUSED(conf);

    if (!account || !certs) {
        qDebug() << "account and certs parameters are required!";
        return false;
    }

    if (account->trustCertificates()) {
        for (const auto &error : std::as_const(errors)) {
            certs->append(error.certificate());
        }
        return true;
    }

    bool allTrusted = true;

    for (const auto &error : std::as_const(errors)) {
        if (!account->approvedCerts().contains(error.certificate())) {
            allTrusted = false;
        }
        certs->append(error.certificate());
    }

    return allTrusted;
}
}
