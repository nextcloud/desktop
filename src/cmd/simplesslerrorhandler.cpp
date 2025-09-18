/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
