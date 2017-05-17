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
    bool handleErrors(QList<QSslError> errors, const QSslConfiguration &conf, QList<QSslCertificate> *certs, OCC::AccountPtr) Q_DECL_OVERRIDE;
};
}

#endif // SIMPLESSLERRORHANDLER_H
