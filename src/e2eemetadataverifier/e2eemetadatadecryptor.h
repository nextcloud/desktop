/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace OCC
{

/**
 * @brief Decrypts a Nextcloud E2EE folder metadata object.
 *
 * Implements a self-contained decryption pipeline for V1.x and V2.x metadata
 * using an unencrypted PKCS8 or traditional RSA PEM private key.
 * No live account, network access, or libsync dependency is needed.
 *
 * Decryption pipeline per version:
 *
 *  V1.0
 *    metadata.metadataKeys[last] — base64 of RSA-OAEP-SHA256 ciphertext of base64(aesKey)
 *    → binary 16-byte AES key
 *    files[name].encrypted — AES-128-GCM( base64(fileJson) ) stored as base64(cipher+tag)|base64(iv)
 *    → decrypted file JSON per entry
 *
 *  V1.1 / V1.2
 *    metadata.metadataKey — base64 of RSA-OAEP-SHA256 ciphertext of base64(base64(aesKey))
 *    → binary 16-byte AES key
 *    files[name].encrypted — same AES-128-GCM scheme as V1.0
 *    → decrypted file JSON per entry
 *
 *  V2.0 / V2.1
 *    users[n].encryptedMetadataKey — base64 of RSA-OAEP-SHA256 ciphertext of binary aesKey
 *    → binary 16-byte AES key
 *    metadata.ciphertext — AES-128-GCM( gzip(innerJson) ) stored as base64(cipher+tag)|base64(iv)
 *    metadata.nonce      — base64 of the 12-byte GCM IV (same IV as in the ciphertext field)
 *    → inner JSON with "files", "folders", "keyChecksums" and "counter"
 *
 * Supported metadata versions: 1.0, 1.1, 1.2, 2.0, 2.1.
 * See https://github.com/nextcloud/end_to_end_encryption_rfc for the spec.
 */
class E2EEMetadataDecryptor
{
public:
    struct Result {
        bool success = false;
        QString error;
        /**
         * The decrypted inner content:
         *  - V2: the full inner ciphertext JSON (files, folders, keyChecksums, counter).
         *  - V1: a "files" object mapping encrypted filenames to their decrypted properties.
         */
        QJsonObject decryptedContent;
    };

    /**
     * Decrypt the metadata contained in @p metadataRoot.
     *
     * @param metadataRoot   Bare metadata JSON object (OCS API envelope already stripped).
     * @param version        Version string as detected by E2EEMetadataVerifier (e.g. "2.1").
     * @param privateKeyPem  Unencrypted PKCS8 or traditional RSA PEM private key of a user
     *                       that has access to the folder.
     * @return               Result with decryptedContent on success, or an error message.
     */
    [[nodiscard]] static Result decrypt(const QJsonObject &metadataRoot,
                                        const QString &version,
                                        const QByteArray &privateKeyPem);
};

} // namespace OCC
