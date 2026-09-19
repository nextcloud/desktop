/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QSslCertificate>

namespace OCC {

class E2eeDecryptor
{
public:
    struct Options {
        QString metadataPath;
        QString privateKeyPath;
        QString certificatePath;
        QString passphrase;
        QString userId;             // optional; auto-detected from certificate if empty
        QString parentMetadataPath; // optional; required for nested (non-root) folders
        QString encryptedDir;
        QString outputDir;
    };

    explicit E2eeDecryptor(Options options);

    // Returns 0 on full success, 1 on fatal/setup error, 2 on partial file-decryption failure.
    int run();

private:
    struct EncryptedFileEntry {
        QString encryptedFilename;
        QString originalFilename;
        QByteArray encryptionKey;
        QByteArray initializationVector;
        QByteArray authenticationTag;
        QByteArray mimetype;
    };

    struct ParsedMetadata {
        QVector<EncryptedFileEntry> files;
        bool valid = false;
    };

    // Step 1: decrypt the user private key using the supplied passphrase
    [[nodiscard]] QByteArray decryptPrivateKeyFromFile() const;

    // Step 2: RSA-OAEP decrypt the encrypted metadata key
    [[nodiscard]] QByteArray rsaOaepDecryptMetadataKey(const QByteArray &privateKeyPem,
                                                       const QByteArray &base64EncryptedKey) const;

    // Step 3a: extract the binary metadata key from a root folder metadata JSON
    [[nodiscard]] QByteArray extractMetadataKey(const QByteArray &metadataJson,
                                                const QByteArray &privateKeyPem,
                                                const QSslCertificate &certificate) const;

    // Step 3b: decrypt and decompress the metadata ciphertext to obtain the inner JSON
    [[nodiscard]] ParsedMetadata parseMetadata(const QByteArray &metadataJson,
                                               const QByteArray &binaryMetadataKey) const;

    // Step 4: AES-128-GCM decrypt one file
    [[nodiscard]] bool decryptFile(const EncryptedFileEntry &entry) const;

    // Helper: resolve and strip the OCS wrapper if present; return the inner metadata JSON bytes
    [[nodiscard]] static QByteArray unwrapMetadata(const QByteArray &raw);

    Options _options;
};

} // namespace OCC
