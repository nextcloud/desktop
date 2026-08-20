/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "e2eedecryptor.h"

#include "clientsideencryption.h"
#include "clientsideencryptionprimitives.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSslCertificate>
#include <QString>
#include <QVector>

#include <openssl/evp.h>
#include <openssl/rsa.h>

Q_LOGGING_CATEGORY(lcE2eeDecrypt, "nextcloud.e2ee_decrypt", QtInfoMsg)

namespace OCC {

namespace {

// Returns the inner metadata JSON string from an OCS-wrapped document,
// or the original bytes if there is no OCS wrapper.
QByteArray unwrapMetadataImpl(const QByteArray &raw)
{
    const auto doc = QJsonDocument::fromJson(raw);
    if (doc.isNull()) {
        return raw;
    }
    const auto ocsObj = doc.object()[QStringLiteral("ocs")].toObject();
    if (ocsObj.isEmpty()) {
        // Not OCS-wrapped – return original bytes as-is
        return raw;
    }
    const auto metaDataStr = ocsObj[QStringLiteral("data")].toObject()[QStringLiteral("meta-data")].toString();
    return metaDataStr.toUtf8();
}

// Derive a 32-byte key from passphrase + salt using PBKDF2-HMAC-SHA256 with 600 000 iterations.
QByteArray pbkdf2Sha256(const QByteArray &passphrase, const QByteArray &salt)
{
    constexpr auto iterations = 600000;
    constexpr auto keyLength = 32;
    auto key = QByteArray(keyLength, '\0');
    PKCS5_PBKDF2_HMAC(passphrase.constData(),
                      passphrase.size(),
                      reinterpret_cast<const unsigned char *>(salt.constData()),
                      salt.size(),
                      iterations,
                      EVP_sha256(),
                      keyLength,
                      reinterpret_cast<unsigned char *>(key.data()));
    return key;
}

// Derive a 32-byte key from passphrase + salt using PBKDF2-HMAC-SHA1 with 600 000 iterations (deprecated).
QByteArray pbkdf2Sha1x600k(const QByteArray &passphrase, const QByteArray &salt)
{
    constexpr auto iterations = 600000;
    constexpr auto keyLength = 32;
    auto key = QByteArray(keyLength, '\0');
    PKCS5_PBKDF2_HMAC(passphrase.constData(),
                      passphrase.size(),
                      reinterpret_cast<const unsigned char *>(salt.constData()),
                      salt.size(),
                      iterations,
                      EVP_sha1(),
                      keyLength,
                      reinterpret_cast<unsigned char *>(key.data()));
    return key;
}

// Derive a 32-byte key from passphrase + salt using PBKDF2-HMAC-SHA1 with 1 024 iterations (oldest, deprecated).
QByteArray pbkdf2Sha1x1024(const QByteArray &passphrase, const QByteArray &salt)
{
    constexpr auto iterations = 1024;
    constexpr auto keyLength = 32;
    auto key = QByteArray(keyLength, '\0');
    PKCS5_PBKDF2_HMAC(passphrase.constData(),
                      passphrase.size(),
                      reinterpret_cast<const unsigned char *>(salt.constData()),
                      salt.size(),
                      iterations,
                      EVP_sha1(),
                      keyLength,
                      reinterpret_cast<unsigned char *>(key.data()));
    return key;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// E2eeDecryptor
// ---------------------------------------------------------------------------

E2eeDecryptor::E2eeDecryptor(Options options)
    : _options(std::move(options))
{
}

int E2eeDecryptor::run()
{
    // 1. Decrypt the private key
    const auto privateKeyPem = decryptPrivateKeyFromFile();
    if (privateKeyPem.isEmpty()) {
        qCCritical(lcE2eeDecrypt) << "Failed to decrypt private key. Check your passphrase and private-key file.";
        return 1;
    }
    qCInfo(lcE2eeDecrypt) << "Private key decrypted successfully.";

    // 2. Load the user certificate
    auto certFile = QFile{_options.certificatePath};
    if (!certFile.open(QIODevice::ReadOnly)) {
        qCCritical(lcE2eeDecrypt) << "Cannot open certificate file:" << _options.certificatePath;
        return 1;
    }
    const auto certificate = QSslCertificate{certFile.readAll(), QSsl::Pem};
    certFile.close();
    if (certificate.isNull()) {
        qCCritical(lcE2eeDecrypt) << "Failed to parse certificate PEM from:" << _options.certificatePath;
        return 1;
    }

    // 3. Read the metadata file
    auto mdFile = QFile{_options.metadataPath};
    if (!mdFile.open(QIODevice::ReadOnly)) {
        qCCritical(lcE2eeDecrypt) << "Cannot open metadata file:" << _options.metadataPath;
        return 1;
    }
    const auto rawMetadata = mdFile.readAll();
    mdFile.close();
    const auto metadataJson = unwrapMetadata(rawMetadata);

    // 4. Determine the binary metadata key
    auto binaryMetadataKey = QByteArray{};

    // Check whether the metadata has a "users" array (root folder) or not (nested)
    const auto innerDoc = QJsonDocument::fromJson(metadataJson);
    if (innerDoc.isNull()) {
        qCCritical(lcE2eeDecrypt) << "Failed to parse inner metadata JSON from:" << _options.metadataPath;
        return 1;
    }
    const auto innerObj = innerDoc.object();
    const auto hasUsers = innerObj.contains(QStringLiteral("users")) && !innerObj[QStringLiteral("users")].toArray().isEmpty();

    if (hasUsers) {
        binaryMetadataKey = extractMetadataKey(metadataJson, privateKeyPem, certificate);
    } else {
        // Nested folder – need the parent metadata to obtain the metadata key
        if (_options.parentMetadataPath.isEmpty()) {
            qCCritical(lcE2eeDecrypt) << "The metadata has no 'users' array (nested folder). "
                                         "Please supply the root folder metadata via --parent-metadata.";
            return 1;
        }

        auto parentFile = QFile{_options.parentMetadataPath};
        if (!parentFile.open(QIODevice::ReadOnly)) {
            qCCritical(lcE2eeDecrypt) << "Cannot open parent metadata file:" << _options.parentMetadataPath;
            return 1;
        }
        const auto rawParent = parentFile.readAll();
        parentFile.close();
        const auto parentJson = unwrapMetadata(rawParent);
        binaryMetadataKey = extractMetadataKey(parentJson, privateKeyPem, certificate);
    }

    if (binaryMetadataKey.isEmpty()) {
        qCCritical(lcE2eeDecrypt) << "Could not extract binary metadata key.";
        return 1;
    }

    // 5. Parse the metadata to obtain the list of encrypted files
    const auto parsed = parseMetadata(metadataJson, binaryMetadataKey);
    if (!parsed.valid) {
        qCCritical(lcE2eeDecrypt) << "Failed to parse encrypted file metadata.";
        return 1;
    }

    if (parsed.files.isEmpty()) {
        qCInfo(lcE2eeDecrypt) << "No files found in metadata. Nothing to decrypt.";
        return 0;
    }

    // 6. Ensure the output directory exists
    const auto outputDir = QDir{_options.outputDir};
    if (!outputDir.exists() && !QDir{}.mkpath(_options.outputDir)) {
        qCCritical(lcE2eeDecrypt) << "Cannot create output directory:" << _options.outputDir;
        return 1;
    }

    // 7. Decrypt each file; track successes and failures
    auto failedCount = 0;
    for (const auto &entry : parsed.files) {
        if (!decryptFile(entry)) {
            ++failedCount;
        }
    }

    if (failedCount == 0) {
        return 0;
    }

    qCWarning(lcE2eeDecrypt) << failedCount << "file(s) could not be decrypted (see warnings above).";
    // Partial failure: some files succeeded, some did not
    return (failedCount < parsed.files.size()) ? 2 : 1;
}


// ---------------------------------------------------------------------------
// Private key decryption
// ---------------------------------------------------------------------------

QByteArray E2eeDecryptor::decryptPrivateKeyFromFile() const
{
    auto keyFile = QFile{_options.privateKeyPath};
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qCCritical(lcE2eeDecrypt) << "Cannot open private-key file:" << _options.privateKeyPath;
        return {};
    }
    const auto encryptedKeyData = keyFile.readAll().trimmed();
    keyFile.close();

    if (encryptedKeyData.isEmpty()) {
        qCCritical(lcE2eeDecrypt) << "Private-key file is empty:" << _options.privateKeyPath;
        return {};
    }

    // Extract salt (part 3) via the exported EncryptionHelper function
    const auto salt = EncryptionHelper::extractPrivateKeySalt(encryptedKeyData);
    if (salt.isEmpty()) {
        qCCritical(lcE2eeDecrypt) << "Cannot extract salt from private-key file.";
        return {};
    }

    // Normalise passphrase: remove spaces, lower-case (mirrors the desktop app behaviour)
    auto normalised = _options.passphrase;
    normalised.remove(QLatin1Char(' '));
    normalised = normalised.toLower();
    const auto passphraseBytes = normalised.toUtf8();

    // Try three PBKDF2 variants in order, stopping on the first success.
    const auto tryDecrypt = [&](const QByteArray &derivedKey) -> QByteArray {
        return EncryptionHelper::decryptPrivateKey(derivedKey, encryptedKeyData);
    };

    // Current: SHA-256, 600 000 iterations
    if (const auto pem = tryDecrypt(pbkdf2Sha256(passphraseBytes, salt)); !pem.isEmpty()) {
        return pem;
    }
    qCInfo(lcE2eeDecrypt) << "SHA-256/600k PBKDF2 variant failed, trying deprecated SHA-1/600k…";

    // Deprecated: SHA-1, 600 000 iterations
    if (const auto pem = tryDecrypt(pbkdf2Sha1x600k(passphraseBytes, salt)); !pem.isEmpty()) {
        return pem;
    }
    qCInfo(lcE2eeDecrypt) << "SHA-1/600k PBKDF2 variant failed, trying oldest deprecated SHA-1/1024…";

    // Oldest deprecated: SHA-1, 1 024 iterations
    if (const auto pem = tryDecrypt(pbkdf2Sha1x1024(passphraseBytes, salt)); !pem.isEmpty()) {
        return pem;
    }

    qCCritical(lcE2eeDecrypt) << "All PBKDF2 variants exhausted. Wrong passphrase or corrupted key file.";
    return {};
}

// ---------------------------------------------------------------------------
// RSA-OAEP metadata key decryption
// ---------------------------------------------------------------------------

QByteArray E2eeDecryptor::rsaOaepDecryptMetadataKey(const QByteArray &privateKeyPem,
                                                     const QByteArray &base64EncryptedKey) const
{
    // Load the PEM private key
    auto bio = Bio{};
    BIO_write(bio, privateKeyPem.constData(), privateKeyPem.size());
    auto privateKey = PKey::readPrivateKey(bio);
    if (!static_cast<EVP_PKEY *>(privateKey)) {
        qCCritical(lcE2eeDecrypt) << "Failed to load private key PEM for RSA-OAEP decryption.";
        return {};
    }

    auto ctx = PKeyCtx::forKey(privateKey);
    if (!static_cast<EVP_PKEY_CTX *>(ctx)) {
        qCCritical(lcE2eeDecrypt) << "Failed to create EVP_PKEY_CTX.";
        return {};
    }

    if (EVP_PKEY_decrypt_init(ctx) <= 0) {
        qCCritical(lcE2eeDecrypt) << "EVP_PKEY_decrypt_init failed.";
        return {};
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        qCCritical(lcE2eeDecrypt) << "EVP_PKEY_CTX_set_rsa_padding failed.";
        return {};
    }

    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
        qCCritical(lcE2eeDecrypt) << "EVP_PKEY_CTX_set_rsa_oaep_md failed.";
        return {};
    }

    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        qCCritical(lcE2eeDecrypt) << "EVP_PKEY_CTX_set_rsa_mgf1_md failed.";
        return {};
    }

    const auto ciphertext = QByteArray::fromBase64(base64EncryptedKey);
    auto outLen = std::size_t{};
    if (EVP_PKEY_decrypt(ctx, nullptr, &outLen,
                         reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                         static_cast<std::size_t>(ciphertext.size())) <= 0) {
        qCCritical(lcE2eeDecrypt) << "EVP_PKEY_decrypt (size query) failed.";
        return {};
    }

    auto out = QByteArray(static_cast<int>(outLen), '\0');
    if (EVP_PKEY_decrypt(ctx,
                         reinterpret_cast<unsigned char *>(out.data()), &outLen,
                         reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                         static_cast<std::size_t>(ciphertext.size())) <= 0) {
        qCCritical(lcE2eeDecrypt) << "EVP_PKEY_decrypt failed.";
        return {};
    }

    // Trim to actual output length and return the raw binary metadata key
    return QByteArray(out.constData(), static_cast<int>(outLen));
}

// ---------------------------------------------------------------------------
// Metadata key extraction (V2+ root folder)
// ---------------------------------------------------------------------------

QByteArray E2eeDecryptor::extractMetadataKey(const QByteArray &metadataJson,
                                              const QByteArray &privateKeyPem,
                                              const QSslCertificate &certificate) const
{
    const auto doc = QJsonDocument::fromJson(metadataJson);
    if (doc.isNull()) {
        qCCritical(lcE2eeDecrypt) << "Failed to parse inner metadata JSON for key extraction.";
        return {};
    }
    const auto rootObj = doc.object();

    // Version detection – we look both at metadata.version and top-level version
    auto versionStr = QString{};
    const auto metadataObjForVersion = rootObj[QStringLiteral("metadata")].toObject();
    if (metadataObjForVersion.contains(QStringLiteral("version"))) {
        const auto v = metadataObjForVersion[QStringLiteral("version")];
        versionStr = v.isString() ? v.toString() : QString::number(v.toDouble(), 'f', 1);
    } else if (rootObj.contains(QStringLiteral("version"))) {
        const auto v = rootObj[QStringLiteral("version")];
        if (v.isString()) {
            versionStr = v.toString();
        } else if (v.isDouble()) {
            versionStr = QString::number(v.toDouble(), 'f', 1);
        } else {
            versionStr = QString::number(v.toVariant().toInt()) + QStringLiteral(".0");
        }
    }

    // Accept "2", "2.0", "2.1" – reject anything below 2.0
    const auto isV2OrLater = (versionStr == QStringLiteral("2")
                               || versionStr == QStringLiteral("2.0")
                               || versionStr == QStringLiteral("2.1"));
    if (!isV2OrLater) {
        qCCritical(lcE2eeDecrypt) << "Unsupported metadata version:" << versionStr
                                  << "– this tool only supports V2.0 and later.";
        return {};
    }

    // Compute the SHA-256 fingerprint of the supplied certificate (hex, lowercase)
    const auto certFingerprint = certificate.digest(QCryptographicHash::Sha256).toHex();

    // Determine the userId to look for
    const auto targetUserId = _options.userId.isEmpty()
        ? certificate.subjectInfo(QSslCertificate::CommonName).value(0)
        : _options.userId;

    // Iterate the users array
    const auto usersArray = rootObj[QStringLiteral("users")].toArray();
    for (const auto &userVal : usersArray) {
        const auto userObj = userVal.toObject();
        const auto userId = userObj[QStringLiteral("userId")].toString();
        const auto userCertPem = userObj[QStringLiteral("certificate")].toString().toUtf8();
        const auto encryptedMetadataKey = userObj[QStringLiteral("encryptedMetadataKey")].toString().toUtf8();

        // Match by userId (if provided) or by certificate fingerprint
        auto matched = false;
        if (!targetUserId.isEmpty() && userId == targetUserId) {
            matched = true;
        } else if (!userCertPem.isEmpty()) {
            const auto userCert = QSslCertificate{userCertPem, QSsl::Pem};
            if (!userCert.isNull()) {
                const auto fp = userCert.digest(QCryptographicHash::Sha256).toHex();
                matched = (fp == certFingerprint);
            }
        }

        if (!matched) {
            continue;
        }

        if (encryptedMetadataKey.isEmpty()) {
            qCCritical(lcE2eeDecrypt) << "Matched user entry for" << userId << "but encryptedMetadataKey is empty.";
            return {};
        }

        // RSA-OAEP decrypt → raw binary metadata key (16 bytes)
        const auto binaryKey = rsaOaepDecryptMetadataKey(privateKeyPem, encryptedMetadataKey);
        if (binaryKey.isEmpty()) {
            qCCritical(lcE2eeDecrypt) << "RSA-OAEP decryption of metadata key failed for user:" << userId;
            return {};
        }
        qCInfo(lcE2eeDecrypt) << "Metadata key extracted successfully for user:" << userId;
        return binaryKey;
    }

    qCCritical(lcE2eeDecrypt) << "No matching user entry found in metadata. "
                                  "Try specifying --user-id explicitly.";
    return {};
}

// ---------------------------------------------------------------------------
// Metadata ciphertext decryption + file list parsing
// ---------------------------------------------------------------------------

E2eeDecryptor::ParsedMetadata E2eeDecryptor::parseMetadata(const QByteArray &metadataJson,
                                                            const QByteArray &binaryMetadataKey) const
{
    const auto doc = QJsonDocument::fromJson(metadataJson);
    if (doc.isNull()) {
        qCCritical(lcE2eeDecrypt) << "Failed to parse inner metadata JSON for ciphertext decryption.";
        return {};
    }
    const auto rootObj = doc.object();
    const auto metadataObj = rootObj[QStringLiteral("metadata")].toObject();

    const auto nonceB64 = metadataObj[QStringLiteral("nonce")].toString().toLocal8Bit();
    const auto nonce = QByteArray::fromBase64(nonceB64);

    const auto cipherTextFull = metadataObj[QStringLiteral("ciphertext")].toString().toLocal8Bit();
    // The ciphertext field may be "base64data|base64iv" – take only the part before '|'
    const auto parts = cipherTextFull.split('|');
    const auto cipherTextPart = parts.value(0);
    const auto cipherTextBinary = QByteArray::fromBase64(cipherTextPart);

    const auto plaintext = EncryptionHelper::decryptThenUnGzipData(binaryMetadataKey, cipherTextBinary, nonce);
    if (plaintext.isEmpty()) {
        qCCritical(lcE2eeDecrypt) << "decryptThenUnGzipData returned empty result – wrong metadata key or corrupted ciphertext.";
        return {};
    }

    const auto plainDoc = QJsonDocument::fromJson(plaintext);
    if (plainDoc.isNull()) {
        qCCritical(lcE2eeDecrypt) << "Decrypted metadata ciphertext is not valid JSON.";
        return {};
    }
    const auto plainObj = plainDoc.object();

    auto result = ParsedMetadata{};

    // Parse files
    const auto filesObj = plainObj[QStringLiteral("files")].toObject();
    for (auto it = filesObj.constBegin(), end = filesObj.constEnd(); it != end; ++it) {
        const auto fileObj = it.value().toObject();
        const auto originalFilename = fileObj[QStringLiteral("filename")].toString();
        if (originalFilename.isEmpty()) {
            qCWarning(lcE2eeDecrypt) << "Skipping file entry with empty filename, encrypted name:" << it.key();
            continue;
        }

        auto entry = EncryptedFileEntry{};
        entry.encryptedFilename = it.key();
        entry.originalFilename = originalFilename;
        entry.encryptionKey = QByteArray::fromBase64(fileObj[QStringLiteral("key")].toString().toLocal8Bit());
        // V2 uses "nonce"; fall back to "initializationVector" for forward compatibility
        auto iv = QByteArray::fromBase64(fileObj[QStringLiteral("nonce")].toString().toLocal8Bit());
        if (iv.isEmpty()) {
            iv = QByteArray::fromBase64(fileObj[QStringLiteral("initializationVector")].toString().toLocal8Bit());
        }
        entry.initializationVector = iv;
        entry.authenticationTag = QByteArray::fromBase64(fileObj[QStringLiteral("authenticationTag")].toString().toLocal8Bit());
        entry.mimetype = fileObj[QStringLiteral("mimetype")].toString().toLocal8Bit();

        // Normalise legacy mimetype value
        if (entry.mimetype == QByteArrayLiteral("inode/directory")) {
            entry.mimetype = QByteArrayLiteral("httpd/unix-directory");
        }

        result.files.append(std::move(entry));
    }

    // Parse folders (record name mappings; add as directory entries)
    const auto foldersObj = plainObj[QStringLiteral("folders")].toObject();
    for (auto it = foldersObj.constBegin(); it != foldersObj.constEnd(); ++it) {
        const auto folderOriginalName = it.value().toString();
        if (folderOriginalName.isEmpty()) {
            continue;
        }
        auto entry = EncryptedFileEntry{};
        entry.encryptedFilename = it.key();
        entry.originalFilename = folderOriginalName;
        entry.mimetype = QByteArrayLiteral("httpd/unix-directory");
        result.files.append(std::move(entry));
    }

    result.valid = true;
    return result;
}

// ---------------------------------------------------------------------------
// Single-file decryption
// ---------------------------------------------------------------------------

bool E2eeDecryptor::decryptFile(const EncryptedFileEntry &entry) const
{
    // Directories – just create them, no decryption needed
    const auto isDirectory = entry.mimetype == QByteArrayLiteral("httpd/unix-directory")
                             || entry.mimetype.isEmpty();

    const auto outputPath = QDir{_options.outputDir}.filePath(entry.originalFilename);

    if (isDirectory) {
        if (!QDir{}.mkpath(outputPath)) {
            qCWarning(lcE2eeDecrypt) << "Failed to create directory:" << outputPath;
            return false;
        }
        qCInfo(lcE2eeDecrypt) << "Created directory:" << outputPath;
        return true;
    }

    // Locate the encrypted source file
    const auto encryptedPath = QDir{_options.encryptedDir}.filePath(entry.encryptedFilename);
    if (!QFileInfo::exists(encryptedPath)) {
        qCWarning(lcE2eeDecrypt) << "Encrypted file not found:" << encryptedPath;
        return false;
    }

    // Ensure the output parent directory exists
    const auto outputParent = QFileInfo{outputPath}.absolutePath();
    if (!QDir{}.mkpath(outputParent)) {
        qCWarning(lcE2eeDecrypt) << "Cannot create parent directory:" << outputParent;
        return false;
    }

    auto inputFile = QFile{encryptedPath};
    if (!inputFile.open(QIODevice::ReadOnly)) {
        qCWarning(lcE2eeDecrypt) << "Cannot open encrypted file for reading:" << encryptedPath;
        return false;
    }

    auto outputFile = QFile{outputPath};
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcE2eeDecrypt) << "Cannot open output file for writing:" << outputPath;
        inputFile.close();
        return false;
    }

    const auto ok = EncryptionHelper::fileDecryption(entry.encryptionKey,
                                                     entry.initializationVector,
                                                     &inputFile,
                                                     &outputFile);
    inputFile.close();
    outputFile.close();

    if (!ok) {
        qCWarning(lcE2eeDecrypt) << "fileDecryption failed for" << entry.originalFilename
                                 << "(encrypted:" << entry.encryptedFilename << ")";
        // Remove any partial output
        outputFile.remove();
        return false;
    }

    qCInfo(lcE2eeDecrypt) << "Decrypted:" << entry.encryptedFilename << "->" << outputPath;
    return true;
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

QByteArray E2eeDecryptor::unwrapMetadata(const QByteArray &raw)
{
    return unwrapMetadataImpl(raw);
}

} // namespace OCC
