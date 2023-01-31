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

#include "createe2eesharejob.h"
#include "clientsideencryption.h"

namespace OCC
{
Q_LOGGING_CATEGORY(lcCreateE2eeShareJob, "nextcloud.gui.createe2eesharejob", QtInfoMsg)

CreateE2eeShareJob::CreateE2eeShareJob(const QString &sharePath,
                                       const ShareePtr sharee,
                                       const Share::Permissions desiredPermissions,
                                       const QSharedPointer<ShareManager> &shareManager,
                                       const AccountPtr &account,
                                       const QString &password,
                                       QObject *parent)
    : QObject{parent}
    , _sharePath(sharePath)
    , _sharee(sharee)
    , _desiredPermissions(desiredPermissions)
    , _manager(shareManager)
    , _account(account)
    , _password(password)
{
    connect(this, &CreateE2eeShareJob::certificateReady, this, &CreateE2eeShareJob::slotCertificateReady);
}

void CreateE2eeShareJob::start()
{
    _account->e2e()->fetchFromKeyChain(_account, _sharee->shareWith());
    connect(_account->e2e(), &ClientSideEncryption::certificateFetchedFromKeychain, this, &CreateE2eeShareJob::slotCertificateFetchedFromKeychain);
}

void CreateE2eeShareJob::slotCertificateFetchedFromKeychain(QSslCertificate certificate)
{
    disconnect(_account->e2e(), &ClientSideEncryption::certificateFetchedFromKeychain, this, &CreateE2eeShareJob::slotCertificateFetchedFromKeychain);
    if (!certificate.isValid()) {
        // get sharee's public key
        _account->e2e()->getUsersPublicKeyFromServer(_account, {_sharee->shareWith()});
        connect(_account->e2e(), &ClientSideEncryption::certificatesFetchedFromServer, this, &CreateE2eeShareJob::slotCertificatesFetchedFromServer);
        return;
    }
    emit certificateReady(certificate);
}

void CreateE2eeShareJob::slotCertificatesFetchedFromServer(const QHash<QString, QSslCertificate> &results)
{
    const auto certificate = results.isEmpty() ? QSslCertificate{} : results.value(_sharee->shareWith());
    if (!certificate.isValid()) {
        emit certificateReady(certificate);
        return;
    }
    _account->e2e()->writeCertificate(_account, _sharee->shareWith(), certificate);
    connect(_account->e2e(), &ClientSideEncryption::certificateWriteComplete, this, &CreateE2eeShareJob::certificateReady);
}

void CreateE2eeShareJob::slotCertificateReady(QSslCertificate certificate)
{
    if (!certificate.isValid()) {
        emit _manager->serverError(404, tr("Could not fetch publicKey for user %1").arg(_sharee->shareWith()));
    } else {
        _manager->createShare(_sharePath, Share::ShareType(_sharee->type()), _sharee->shareWith(), _desiredPermissions, _password);
    }
    this->deleteLater();
}
}
