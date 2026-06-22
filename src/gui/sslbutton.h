/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SSLBUTTON_H
#define SSLBUTTON_H

#include <QToolButton>
#include <QPointer>
#include <QSsl>

class QAction;
class QSslCertificate;
class QSslConfiguration;

namespace OCC {

class AccountState;

/**
 * @brief The SslButton class
 * @ingroup gui
 */
class SslButton : public QToolButton
{
    Q_OBJECT
public:
    explicit SslButton(QWidget *parent = nullptr);
    void updateAccountState(AccountState *accountState);

public slots:
    void slotUpdateMenu();

private:
    QMenu *buildCertMenu(QMenu *parent, const QSslCertificate &cert,
        const QList<QSslCertificate> &userApproved, int pos, const QList<QSslCertificate> &systemCaCertificates);
    QPointer<AccountState> _accountState;
    QMenu *_menu;
};

} // namespace OCC

#endif // SSLBUTTON_H
