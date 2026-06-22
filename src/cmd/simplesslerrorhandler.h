/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef SIMPLESSLERRORHANDLER_H
#define SIMPLESSLERRORHANDLER_H

#include "accountfwd.h"

class QSslError;
class QSslCertificate;

namespace OCC {

/**
 * @brief The SimpleSslErrorHandler class
 * @ingroup cmd
 */
class SimpleSslErrorHandler : public OCC::AbstractSslErrorHandler
{
public:
    bool handleErrors(QList<QSslError> errors, const QSslConfiguration &conf, QList<QSslCertificate> *certs, OCC::AccountPtr) override;
};
}

#endif // SIMPLESSLERRORHANDLER_H
