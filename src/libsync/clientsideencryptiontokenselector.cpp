/*
 * Copyright © 2023, Matthieu Gallien <matthieu.gallien@nextcloud.com>
 * Copyright (C) 2017 The Qt Company Ltd.
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
 *
 * Commercial License Usage
 * Licensees holding valid commercial Qt licenses may use this file in
 * accordance with the commercial license agreement provided with the
 * Software or, alternatively, in accordance with the terms contained in
 * a written agreement between you and The Qt Company. For licensing terms
 * and conditions see https://www.qt.io/terms-conditions. For further
 * information use the contact form at https://www.qt.io/contact-us.
 *
 * GNU Lesser General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU Lesser
 * General Public License version 3 as published by the Free Software
 * Foundation and appearing in the file LICENSE.LGPL3 included in the
 * packaging of this file. Please review the following information to
 * ensure the GNU Lesser General Public License version 3 requirements
 * will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 2.0 or (at your option) the GNU General
 * Public license version 3 or any later version approved by the KDE Free
 * Qt Foundation. The licenses are as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
 * included in the packaging of this file. Please review the following
 * information to ensure the GNU General Public License requirements will
 * be met: https://www.gnu.org/licenses/gpl-2.0.html and
 * https://www.gnu.org/licenses/gpl-3.0.html.
 */

#include "clientsideencryptiontokenselector.h"

#include "clientsideencryptionprimitives.h"
#include "account.h"

#include <QLoggingCategory>
#include <QtConcurrentRun>
#include <QSslCertificateExtension>

#if defined(Q_OS_WIN)
#include <wincrypt.h>
#endif

#include <libp11.h>

#include <openssl/pem.h>

namespace {

static unsigned char* unsignedData(QByteArray& array)
{
    return (unsigned char*)array.data();
}

static QByteArray BIO2ByteArray(OCC::Bio &b) {
    auto pending = static_cast<int>(BIO_ctrl_pending(b));
    QByteArray res(pending, '\0');
    BIO_read(b, unsignedData(res), pending);
    return res;
}

}

namespace OCC
{

Q_LOGGING_CATEGORY(lcCseSelector, "nextcloud.sync.clientsideencryption.selector", QtInfoMsg)

ClientSideEncryptionTokenSelector::ClientSideEncryptionTokenSelector(QObject *parent)
    : QObject{parent}
{

}

bool ClientSideEncryptionTokenSelector::isSetup() const
{
    return !_sha256Fingerprint.isEmpty();
}

QVariantList ClientSideEncryptionTokenSelector::discoveredCertificates() const
{
    return _discoveredCertificates;
}

QByteArray ClientSideEncryptionTokenSelector::sha256Fingerprint() const
{
    return _sha256Fingerprint;
}

void ClientSideEncryptionTokenSelector::clear()
{
    _discoveredCertificates.clear();
    _sha256Fingerprint.clear();
}

QFuture<void> ClientSideEncryptionTokenSelector::searchForCertificates(const AccountPtr &account)
{
    return QtConcurrent::run([this, account] () -> void {
        discoverCertificates(account);
    });
}

void ClientSideEncryptionTokenSelector::setSha256Fingerprint(const QByteArray &sha256Fingerprint)
{
    if (_sha256Fingerprint == sha256Fingerprint) {
        return;
    }

    _sha256Fingerprint = sha256Fingerprint;
    Q_EMIT sha256FingerprintChanged();
}

void ClientSideEncryptionTokenSelector::discoverCertificates(const AccountPtr &account)
{
    auto ctx = Pkcs11Context{Pkcs11Context::State::CreateContext};

    auto rc = PKCS11_CTX_load(ctx, account->encryptionHardwareTokenDriverPath().toLatin1().constData());
    if (rc) {
        qCWarning(lcCseSelector()) << "loading pkcs11 engine failed:" << ERR_reason_error_string(ERR_get_error()) << account->encryptionHardwareTokenDriverPath();

        Q_EMIT failedToInitialize(account);
        return;
    }

    auto tokensCount = 0u;
    PKCS11_SLOT *tempTokenSlots = nullptr;
    /* get information on all slots */
    if (PKCS11_enumerate_slots(ctx, &tempTokenSlots, &tokensCount) < 0) {
        qCWarning(lcCseSelector()) << "no slots available" << ERR_reason_error_string(ERR_get_error());

        Q_EMIT failedToInitialize(account);
        return;
    }

    auto deleter = [&ctx, tokensCount] (PKCS11_SLOT* pointer) noexcept -> void {
        PKCS11_release_all_slots(ctx, pointer, tokensCount);
    };

    auto tokenSlots = std::unique_ptr<PKCS11_SLOT[], decltype(deleter)>{tempTokenSlots, deleter};

    if (!tokensCount) {
        qCWarning(lcCseSelector()) << "no tokens found";

        Q_EMIT failedToInitialize(account);
        return;
    }

    _discoveredCertificates.clear();
    auto currentSlot = static_cast<PKCS11_SLOT*>(nullptr);
    for(auto tokenIndex = 0u; tokenIndex < tokensCount; ++tokenIndex) {
        currentSlot = PKCS11_find_next_token(ctx, tokenSlots.get(), tokensCount, currentSlot);
        if (currentSlot == nullptr || currentSlot->token == nullptr) {
            break;
        }

        qCDebug(lcCseSelector()) << "Slot manufacturer......:" << currentSlot->manufacturer;
        qCDebug(lcCseSelector()) << "Slot description.......:" << currentSlot->description;
        qCDebug(lcCseSelector()) << "Slot token label.......:" << currentSlot->token->label;
        qCDebug(lcCseSelector()) << "Slot token manufacturer:" << currentSlot->token->manufacturer;
        qCDebug(lcCseSelector()) << "Slot token model.......:" << currentSlot->token->model;
        qCDebug(lcCseSelector()) << "Slot token serialnr....:" << currentSlot->token->serialnr;

        auto keysCount = 0u;
        auto certificatesFromToken = static_cast<PKCS11_CERT*>(nullptr);
        if (PKCS11_enumerate_certs(currentSlot->token, &certificatesFromToken, &keysCount)) {
            qCWarning(lcCseSelector()) << "PKCS11_enumerate_certs failed" << ERR_reason_error_string(ERR_get_error());

            Q_EMIT failedToInitialize(account);
            return;
        }

        for (auto certificateIndex = 0u; certificateIndex < keysCount; ++certificateIndex) {
            const auto currentCertificate = &certificatesFromToken[certificateIndex];
            qCInfo(lcCseSelector()) << "certificate metadata:"
                                    << "label:" << currentCertificate->label;

            const auto certificateId = QByteArray{reinterpret_cast<char*>(currentCertificate->id), static_cast<int>(currentCertificate->id_len)};
            qCInfo(lcCseSelector()) << "new certificate ID:" << certificateId.toBase64();

            Bio out;
            const auto ret = PEM_write_bio_X509(out, currentCertificate->x509);
            if (ret <= 0){
                qCWarning(lcCseSelector()) << "PEM_write_bio_X509 failed" << ERR_reason_error_string(ERR_get_error());

                Q_EMIT failedToInitialize(account);
                return;
            }

            const auto result = BIO2ByteArray(out);
            const auto sslCertificate = QSslCertificate{result, QSsl::Pem};
            const auto certificateDigest = sslCertificate.digest(QCryptographicHash::Sha256).toBase64();

            qCInfo(lcCseSelector()) << "newly found certificate"
                                    << "subject:" << sslCertificate.subjectDisplayName()
                                    << "issuer:" << sslCertificate.issuerDisplayName()
                                    << "valid since:" << sslCertificate.effectiveDate()
                                    << "valid until:" << sslCertificate.expiryDate()
                                    << "serial number:" << sslCertificate.serialNumber()
                                    << "SHA256 fingerprint:" << certificateDigest;

            if (sslCertificate.isSelfSigned()) {
                qCDebug(lcCseSelector()) << "newly found certificate is self signed: goint to ignore it";
                continue;
            }

            auto hasNeededExtendedKeyUsageExtension = false;
            const auto &allExtensions = sslCertificate.extensions();
            for (const auto &oneExtension : allExtensions) {
                qCDebug(lcCseSelector()) << "extension:" << (oneExtension.isCritical() ? "is critical" : "") << (oneExtension.isSupported() ? "is supported" : "") << oneExtension.name() << oneExtension.value() << oneExtension.oid();
                if (oneExtension.oid() == QStringLiteral("2.5.29.37")) {
                    const auto extendedKeyUsageList = oneExtension.value().toList();
                    for (const auto &oneExtendedKeyUsageValue : extendedKeyUsageList) {
                        qCDebug(lcCseSelector()) << "EKU:" << oneExtendedKeyUsageValue;
                        if (oneExtendedKeyUsageValue == QStringLiteral("E-mail Protection")) {
                            hasNeededExtendedKeyUsageExtension = true;
                            break;
                        }
                    }
                }
            }
            if (!hasNeededExtendedKeyUsageExtension) {
                qCDebug(lcCseSelector()) << "newly found certificate is missing the required EKU extension: Secure Email (1.3.6.1.5.5.7.3.4)";
                continue;
            }

            _discoveredCertificates.push_back(QVariantMap{
                                                          {QStringLiteral("label"), QString::fromLatin1(currentCertificate->label)},
                                                          {QStringLiteral("subject"), sslCertificate.subjectDisplayName()},
                                                          {QStringLiteral("issuer"), sslCertificate.issuerDisplayName()},
                                                          {QStringLiteral("serialNumber"), sslCertificate.serialNumber()},
                                                          {QStringLiteral("validSince"), sslCertificate.effectiveDate()},
                                                          {QStringLiteral("validUntil"), sslCertificate.expiryDate()},
                                                          {QStringLiteral("sha256Fingerprint"), certificateDigest},
                                                          {QStringLiteral("certificate"), QVariant::fromValue(sslCertificate)},
                                                          });

            std::sort(_discoveredCertificates.begin(), _discoveredCertificates.end(), [] (const auto &first, const auto &second) -> bool {
                return first.toMap()[QStringLiteral("validSince")].toDateTime() > second.toMap()[QStringLiteral("validSince")].toDateTime();
            });
        }
    }

    processDiscoveredCertificates();
}

void ClientSideEncryptionTokenSelector::processDiscoveredCertificates()
{
    const auto &allCertificates = discoveredCertificates();
    for (const auto &oneCertificate : allCertificates) {
        const auto certificateData = oneCertificate.toMap();
        const auto sslCertificate = certificateData[QStringLiteral("certificate")].value<QSslCertificate>();
        if (sslCertificate.isNull()) {
            qCDebug(lcCseSelector()) << "null certificate";
            continue;
        }
        const auto sslErrors = QSslCertificate::verify({sslCertificate});
        if (!sslErrors.isEmpty()) {
            for (const auto &oneError : sslErrors) {
                qCInfo(lcCseSelector()) << oneError;
            }
            continue;
        }

        const auto &sha256Fingerprint = sslCertificate.digest(QCryptographicHash::Sha256).toBase64();
        qCInfo(lcCseSelector()) << "selected certificate" << certificateData[QStringLiteral("subject")] << "from" << certificateData[QStringLiteral("issuer")] << "fingerprint" << sha256Fingerprint << "serialNumber" << sslCertificate.serialNumber();

        setSha256Fingerprint(sha256Fingerprint);
        Q_EMIT isSetupChanged();
        return;
    }
}

}
