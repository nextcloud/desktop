/*
 * Copyright (C) 2014 by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_AUTHENTICATIONDIALOG_H
#define MIRALL_AUTHENTICATIONDIALOG_H

#include <QDialog>

class QLineEdit;

namespace OCC {

/**
 * @brief Authenticate a user for a specific credential given his credentials
 * @ingroup gui
 */
class AuthenticationDialog : public QDialog
{
    Q_OBJECT
public:
    AuthenticationDialog(const QString &realm, const QString &domain, QWidget *parent = nullptr);

    QString user() const;
    QString password() const;

private:
    QLineEdit *_user;
    QLineEdit *_password;
};


} // namespace OCC

#endif // MIRALL_AUTHENTICATIONDIALOG_H
