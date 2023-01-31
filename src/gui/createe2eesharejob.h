/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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


#include "account.h"
#include "sharee.h"
#include "sharemanager.h"

#include <QHash>
#include <QObject>
#include <QSslCertificate>
#include <QString>

namespace OCC {
class CreateE2eeShareJob : public QObject
{
    Q_OBJECT

public:
    explicit CreateE2eeShareJob(const QString &sharePath,
                                const ShareePtr sharee,
                                const Share::Permissions desiredPermissions,
                                const QSharedPointer<ShareManager> &shareManager,
                                const AccountPtr &account,
                                const QString &password,
                                QObject *parent = nullptr);

public slots:
    void start();

private slots:
    void slotCertificatesFetchedFromServer(const QHash<QString, QSslCertificate> &results);
    void slotCertificateFetchedFromKeychain(QSslCertificate certificate);
    void slotCertificateReady(QSslCertificate certificate);

private: signals:
    void certificateReady(QSslCertificate certificate);

private:
    QString _sharePath;
    ShareePtr _sharee;
    Share::Permissions _desiredPermissions;
    QString _password;
    QSharedPointer<ShareManager> _manager;
    AccountPtr _account;
    QHash<QString, QMetaObject::Connection> _fetchPublicKeysConnections;
};

}
