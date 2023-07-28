/*
 * Copyright © 2023, Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#ifndef CLIENTSIDETOKENSELECTOR_H
#define CLIENTSIDETOKENSELECTOR_H

#include "accountfwd.h"
#include "owncloudlib.h"

#include <QObject>
#include <QFuture>

namespace OCC
{

class OWNCLOUDSYNC_EXPORT ClientSideEncryptionTokenSelector : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isSetup READ isSetup NOTIFY isSetupChanged)

    Q_PROPERTY(QByteArray sha256Fingerprint READ sha256Fingerprint WRITE setSha256Fingerprint NOTIFY sha256FingerprintChanged)

public:
    explicit ClientSideEncryptionTokenSelector(QObject *parent = nullptr);

    [[nodiscard]] bool isSetup() const;

    [[nodiscard]] QByteArray sha256Fingerprint() const;

    void clear();

public slots:
    QFuture<void> searchForCertificates(const OCC::AccountPtr &account);

    void setSha256Fingerprint(const QByteArray &sha256Fingerprint);

signals:

    void isSetupChanged();

    void sha256FingerprintChanged();

    void failedToInitialize(const OCC::AccountPtr &account);

private:
    void discoverCertificates(const OCC::AccountPtr &account);

    [[nodiscard]] QVariantList discoveredCertificates() const;

    void processDiscoveredCertificates();

    QVariantList _discoveredCertificates;

    QByteArray _sha256Fingerprint;
};

}

#endif // CLIENTSIDETOKENSELECTOR_H
