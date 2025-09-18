/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

    [[nodiscard]] QString user() const;
    [[nodiscard]] QString password() const;

private:
    QLineEdit *_user;
    QLineEdit *_password;
};


} // namespace OCC

#endif // MIRALL_AUTHENTICATIONDIALOG_H
