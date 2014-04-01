/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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
#ifndef SIMPLESSLERRORHANDLER_H
#define SIMPLESSLERRORHANDLER_H

#include <QtCore>
#include <QDialog>
#include <QSslCertificate>
#include <QList>

#include "mirall/account.h"

class QSslError;
class QSslCertificate;

using namespace Mirall;

class SimpleSslErrorHandler : public AbstractSslErrorHandler {
public:
    bool handleErrors(QList<QSslError> errors, QList<QSslCertificate> *certs, Account*);
};

#endif // SIMPLESSLERRORHANDLER_H
