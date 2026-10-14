/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/**
 * nextcloud-e2ee-verify
 *
 * Offline command-line validator for Nextcloud end-to-end encrypted folder
 * metadata.  Supports version 2.0 and 2.1 only (legacy v1.x is rejected).
 *
 * Usage (root folder):
 *   nextcloud-e2ee-verify --private-key key.pem --certificate cert.pem \
 *                         --metadata meta.json [--signature sig.b64]
 *
 * Usage (nested folder – metadata key inherited from root):
 *   nextcloud-e2ee-verify --nested --metadata-key <base64-key> \
 *                         --metadata meta.json
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslCertificate>
#include <QString>
#include <QByteArray>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/cms.h>
#include <openssl/rsa.h>

#include <zlib.h>

#include <QLoggingCategory>

using namespace Qt::StringLiterals;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// Size of the AES-128-GCM authentication tag appended to every ciphertext
static constexpr int kGcmTagSize = 16;

// Expected size of the decrypted binary metadata key
static constexpr int kMetadataKeySize = 16;

// ---------------------------------------------------------------------------
// Console helpers
// ---------------------------------------------------------------------------

static auto g_verbose = false;

Q_LOGGING_CATEGORY(lcE2eeVerify, "nextcloud.e2eeverify", QtInfoMsg)

static void printOk(const QString &msg)
{
    qCInfo(lcE2eeVerify) << "[OK]  " << qPrintable(msg);
}

static void printFail(const QString &msg)
{
    qCCritical(lcE2eeVerify) << "[FAIL]" << qPrintable(msg);
}

static void printInfo(const QString &msg)
{
    qCInfo(lcE2eeVerify) << "[INFO]" << qPrintable(msg);
}

static void printVerbose(const QString &msg)
{
    if (g_verbose) {
        qCDebug(lcE2eeVerify) << "[DBG] " << qPrintable(msg);
    }
}

// ---------------------------------------------------------------------------
// Cryptographic helpers
// ---------------------------------------------------------------------------

/**
 * Compute a lower-case hex-encoded SHA-256 digest of @p data.
 * Equivalent to OCC::calcSha256() but does not depend on libsync.
 */
static QByteArray sha256Hex(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

/**
 * AES-128-GCM decryption.
 *
 * @p inputWithTag  Ciphertext with the 16-byte GCM authentication tag
 *                  appended at the end.
 * @p key           16-byte binary AES key.
 * @p iv            Initialisation vector (nonce).
 * @p output        Decrypted plaintext on success.
 *
 * Returns true on success (including successful tag verification), false on
 * any error.
 */
static bool aes128GcmDecrypt(const QByteArray &key,
                              const QByteArray &iv,
                              const QByteArray &inputWithTag,
                              QByteArray &output)
{
    if (inputWithTag.size() < kGcmTagSize) {
        printVerbose(u"AES-GCM: input too short (%1 bytes, minimum is %2)"_s
                         .arg(inputWithTag.size())
                         .arg(kGcmTagSize));
        return false;
    }

    const int ciphertextLen = inputWithTag.size() - kGcmTagSize;
    const auto *ciphertext = reinterpret_cast<const unsigned char *>(inputWithTag.constData());
    const auto *tag = reinterpret_cast<const unsigned char *>(inputWithTag.constData() + ciphertextLen);

    auto *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }

    output.clear();
    auto ok = false;
    do {
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr)) {
            printVerbose(u"AES-GCM: EVP_DecryptInit_ex (cipher) failed"_s);
            break;
        }

        EVP_CIPHER_CTX_set_padding(ctx, 0);

        if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
            printVerbose(u"AES-GCM: failed to set IV length"_s);
            break;
        }

        if (!EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                                 reinterpret_cast<const unsigned char *>(key.constData()),
                                 reinterpret_cast<const unsigned char *>(iv.constData()))) {
            printVerbose(u"AES-GCM: EVP_DecryptInit_ex (key+iv) failed"_s);
            break;
        }

        output.resize(ciphertextLen);
        auto len = 0;
        if (!EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char *>(output.data()), &len,
                                ciphertext, ciphertextLen)) {
            printVerbose(u"AES-GCM: EVP_DecryptUpdate failed"_s);
            output.clear();
            break;
        }

        // Set expected GCM authentication tag before calling Final.
        // EVP_CIPHER_CTX_ctrl takes a void* but does not modify the tag data when
        // EVP_CTRL_GCM_SET_TAG is used for decryption; the cast is required by the API.
        if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kGcmTagSize,
                                   const_cast<unsigned char *>(tag))) {
            printVerbose(u"AES-GCM: failed to set GCM tag"_s);
            output.clear();
            break;
        }

        auto finalLen = 0;
        if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(output.data()) + len,
                                 &finalLen) <= 0) {
            printVerbose(u"AES-GCM: EVP_DecryptFinal_ex failed (tag mismatch or other error)"_s);
            output.clear();
            break;
        }

        output.resize(len + finalLen);
        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

/**
 * Decompress gzip-compressed data using zlib's inflate with the gzip window
 * bits (15 + 16).
 *
 * Returns the decompressed data, or an empty QByteArray on error.
 */
static QByteArray gunzip(const QByteArray &compressed)
{
    z_stream stream = {};
    // 15 + 16 enables gzip decoding in zlib
    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        printVerbose(u"zlib: inflateInit2 failed_s"_s);
        return {};
    }

    // z_stream.next_in is Bytef* (non-const) in the zlib API even though inflate
    // does not modify the input; the cast is required to satisfy the API signature.
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());

    auto result = QByteArray{};
    auto chunk = QByteArray{64 * 1024, '\0'};
    auto ret = -1;
    do {
        stream.next_out = reinterpret_cast<Bytef *>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            printVerbose(u"zlib: inflate error %1"_s.arg(ret));
            inflateEnd(&stream);
            return {};
        }
        result.append(chunk.constData(), chunk.size() - static_cast<int>(stream.avail_out));
    } while (ret != Z_STREAM_END && stream.avail_in > 0);

    inflateEnd(&stream);
    return result;
}

// ---------------------------------------------------------------------------
// Decrypt and decompress metadata ciphertext
// ---------------------------------------------------------------------------

/**
 * Decrypt the "metadata.ciphertext" field from @p innerDoc and gunzip the
 * result.
 *
 * The ciphertext field format is "base64(encrypted+tag)|base64(iv)" for
 * backwards compatibility; only the first "|"-split part is used because the
 * IV is separately stored in "metadata.nonce".
 *
 * Returns the decompressed plaintext JSON bytes, or empty on failure.
 */
static QByteArray decryptAndDecompressMetadata(const QJsonDocument &innerDoc,
                                                const QByteArray &metadataKey)
{
    const auto metaObj = innerDoc.object().value(QStringLiteral("metadata")).toObject();

    const auto ciphertextField =
        metaObj.value(QStringLiteral("ciphertext")).toString().toLatin1();
    const auto nonce =
        QByteArray::fromBase64(metaObj.value(QStringLiteral("nonce")).toString().toLatin1());

    // Strip the legacy "|iv" suffix – the canonical nonce comes from the
    // separate "nonce" field.
    const auto ciphertextParts = ciphertextField.split('|');
    const auto ciphertextBase64 = ciphertextParts.value(0);
    const auto ciphertextWithTag = QByteArray::fromBase64(ciphertextBase64);

    printVerbose(u"Ciphertext+tag size: %1 bytes"_s.arg(ciphertextWithTag.size()));
    printVerbose(u"Nonce size: %1 bytes"_s.arg(nonce.size()));

    auto decrypted = QByteArray{};
    if (!aes128GcmDecrypt(metadataKey, nonce, ciphertextWithTag, decrypted)) {
        return {};
    }

    printVerbose(u"Decrypted (gzip) size: %1 bytes"_s.arg(decrypted.size()));

    const auto plaintext = gunzip(decrypted);
    return plaintext;
}

// ---------------------------------------------------------------------------
// RSA-OAEP-SHA256 private-key decryption
// ---------------------------------------------------------------------------

/**
 * Decrypt @p encryptedKeyBase64 (base64-encoded RSA-OAEP-SHA256 ciphertext)
 * using the RSA private key given as PEM in @p privateKeyPem.
 *
 * Returns the raw decrypted bytes (the binary metadata key), or an empty
 * QByteArray on failure.
 */
static QByteArray decryptRsaOaepSha256(const QByteArray &encryptedKeyBase64,
                                        const QByteArray &privateKeyPem)
{
    const auto encryptedKey = QByteArray::fromBase64(encryptedKeyBase64);

    auto *keyBio = BIO_new_mem_buf(privateKeyPem.constData(), privateKeyPem.size());
    if (!keyBio) {
        return {};
    }
    auto *pkey = PEM_read_bio_PrivateKey(keyBio, nullptr, nullptr, nullptr);
    BIO_free(keyBio);
    if (!pkey) {
        printVerbose(u"RSA: PEM_read_bio_PrivateKey failed"_s);
        return {};
    }

    auto *ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    EVP_PKEY_free(pkey);
    if (!ctx) {
        return {};
    }

    QByteArray out;
    auto success = false;
    do {
        if (EVP_PKEY_decrypt_init(ctx) <= 0) {
            printVerbose(u"RSA: EVP_PKEY_decrypt_init failed"_s);
            break;
        }
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
            printVerbose(u"RSA: failed to set OAEP padding"_s);
            break;
        }
        if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
            printVerbose(u"RSA: failed to set OAEP SHA-256 digest"_s);
            break;
        }
        if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
            printVerbose(u"RSA: failed to set MGF1 SHA-256 digest"_s);
            break;
        }

        size_t outLen = 0;
        const auto *encPtr = reinterpret_cast<const unsigned char *>(encryptedKey.constData());
        const auto encSize = static_cast<size_t>(encryptedKey.size());
        if (EVP_PKEY_decrypt(ctx, nullptr, &outLen, encPtr, encSize) <= 0) {
            printVerbose(u"RSA: EVP_PKEY_decrypt (size query) failed"_s);
            break;
        }

        out.resize(static_cast<int>(outLen));
        if (EVP_PKEY_decrypt(ctx, reinterpret_cast<unsigned char *>(out.data()),
                              &outLen, encPtr, encSize) <= 0) {
            printVerbose(u"RSA: EVP_PKEY_decrypt (decrypt) failed"_s);
            out.clear();
            break;
        }
        out.resize(static_cast<int>(outLen));
        success = true;
    } while (false);

    EVP_PKEY_CTX_free(ctx);
    return success ? out : QByteArray{};
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------

/**
 * If the file bytes represent an OCS-wrapped response
 * {"ocs":{"data":{"meta-data":"<inner>"}}} this function extracts and parses
 * the inner metadata JSON document.  Otherwise the document is returned as-is.
 *
 * Returns a null QJsonDocument on parse failure.
 */
static QJsonDocument extractInnerMetadata(const QByteArray &rawFileContents)
{
    const auto outerDoc = QJsonDocument::fromJson(rawFileContents);
    if (outerDoc.isNull()) {
        return {};
    }

    const auto metaDataStr =
        outerDoc.object()
            .value(u"ocs"_s)
            .toObject()
            .value(u"data"_s)
            .toObject()
            .value(u"meta-data"_s)
            .toString();

    if (!metaDataStr.isEmpty()) {
        printVerbose(u"OCS envelope detected – extracting inner metadata string"_s);
        const auto innerDoc = QJsonDocument::fromJson(metaDataStr.toUtf8());
        if (innerDoc.isNull()) {
            printVerbose(u"Failed to parse inner metadata string as JSON"_s);
        }
        return innerDoc;
    }

    // Not OCS-wrapped – the file is the inner metadata JSON directly
    return outerDoc;
}

/**
 * Detect the metadata version string from the inner JSON document.
 * Checks "metadata.version" first, then the top-level "version" field,
 * mirroring FolderMetadata::setupVersionFromExistingMetadata().
 *
 * Returns a version string such as "2.0" / "2.1", or an empty string.
 */
static QString detectVersionString(const QJsonDocument &innerDoc)
{
    const auto topObj = innerDoc.object();

    const auto metadataObj = topObj.value(u"metadata"_s).toObject();
    if (metadataObj.contains(u"version"_s)) {
        const auto v = metadataObj.value(u"version"_s);
        if (v.isString()) {
            return v.toString();
        }
        if (v.isDouble()) {
            return QString::number(v.toDouble(), 'f', 1);
        }
    }

    if (topObj.contains(u"version"_s)) {
        const auto v = topObj.value(u"version"_s);
        if (v.isString()) {
            return v.toString();
        }
        if (v.isDouble()) {
            return QString::number(v.toDouble(), 'f', 1);
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// Schema validation
// ---------------------------------------------------------------------------

/**
 * Validate the structural shape of the inner metadata JSON for v2.0+.
 * Prints [FAIL] for each violation; returns true only if all checks pass.
 */
static bool validateSchemaV2(const QJsonDocument &innerDoc, bool isNested)
{
    auto ok = true;
    const auto obj = innerDoc.object();

    if (!obj.value(u"metadata"_s).isObject()) {
        printFail(u"Schema: missing or invalid 'metadata' object"_s);
        ok = false;
    } else {
        const auto meta = obj.value(u"metadata"_s).toObject();
        if (!meta.value(u"ciphertext"_s).isString()) {
            printFail(u"Schema: 'metadata.ciphertext' is missing or not a string"_s);
            ok = false;
        }
        if (!meta.value(u"nonce"_s).isString()) {
            printFail(u"Schema: 'metadata.nonce' is missing or not a string"_s);
            ok = false;
        }
        if (!meta.value(u"authenticationTag"_s).isString()) {
            printFail(u"Schema: 'metadata.authenticationTag' is missing or not a string"_s);
            ok = false;
        }
    }

    if (!obj.value(u"version"_s).isString()) {
        printFail(u"Schema: top-level 'version' is missing or not a string"_s);
        ok = false;
    }

    if (!isNested) {
        if (!obj.value(u"users"_s).isArray()) {
            printFail(u"Schema: 'users' array is missing or invalid (required for root folders)"_s);
            ok = false;
        } else {
            const auto users = obj.value(u"users"_s).toArray();
            if (users.isEmpty()) {
                printFail(u"Schema: 'users' array is empty (root folder must have at least one user)"_s);
                ok = false;
            }
            for (int i = 0; i < users.size(); ++i) {
                const auto user = users.at(i).toObject();
                if (user.value(u"userId"_s).toString().isEmpty()) {
                    printFail(u"Schema: users[%1].userId is missing or empty"_s.arg(i));
                    ok = false;
                }
                if (user.value(u"certificate"_s).toString().isEmpty()) {
                    printFail(u"Schema: users[%1].certificate is missing or empty"_s.arg(i));
                    ok = false;
                }
                if (user.value(u"encryptedMetadataKey"_s).toString().isEmpty()) {
                    printFail(u"Schema: users[%1].encryptedMetadataKey is missing or empty"_s.arg(i));
                    ok = false;
                }
            }
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// Metadata key extraction (root folders)
// ---------------------------------------------------------------------------

/**
 * Find the user entry in the "users" array whose certificate matches
 * @p certificatePem, then RSA-OAEP-decrypt the encryptedMetadataKey
 * using @p privateKeyPem.
 *
 * Sets @p matchedUserId on success.
 * Returns the raw 16-byte binary metadata key, or empty on failure.
 */
static QByteArray extractMetadataKey(const QJsonDocument &innerDoc,
                                      const QByteArray &privateKeyPem,
                                      const QByteArray &certificatePem,
                                      QString &matchedUserId)
{
    const QSslCertificate myCert(certificatePem);
    if (myCert.isNull()) {
        printFail(u"Certificate: failed to parse PEM certificate"_s);
        return {};
    }
    const auto myFingerprint = myCert.digest(QCryptographicHash::Sha256).toHex();
    printVerbose(u"Certificate SHA-256 fingerprint (hex): %1"_s
                     .arg(QString::fromLatin1(myFingerprint)));

    const auto users = innerDoc.object().value(u"users"_s).toArray();
    for (const QJsonValue &userVal : users) {
        const auto userObj = userVal.toObject();
        const auto userCertPem = userObj.value(u"certificate"_s).toString().toUtf8();
        const QSslCertificate userCert(userCertPem);

        if (userCert.isNull()) {
            printVerbose(u"  Skipping user '%1': unparseable certificate"_s
                             .arg(userObj.value(u"userId"_s).toString()));
            continue;
        }

        const auto userFingerprint = userCert.digest(QCryptographicHash::Sha256).toHex();
        printVerbose(u"  Checking user '%1' (fingerprint: %2)"_s
                         .arg(userObj.value(u"userId"_s).toString(),
                              QString::fromLatin1(userFingerprint)));

        if (userFingerprint != myFingerprint) {
            continue;
        }

        matchedUserId = userObj.value(u"userId"_s).toString();

        // encryptedMetadataKey is stored as base64 ASCII text in the JSON
        const auto encryptedKeyBase64 =
            userObj.value(u"encryptedMetadataKey"_s).toString().toUtf8();

        return decryptRsaOaepSha256(encryptedKeyBase64, privateKeyPem);
    }

    printFail(u"Key decryption: no user entry matches the provided certificate"_s);
    return {};
}

// ---------------------------------------------------------------------------
// Decrypted ciphertext validation
// ---------------------------------------------------------------------------

/**
 * Validate the structural correctness of the decrypted inner plaintext JSON.
 * Prints [FAIL] for each violation; returns true only if all checks pass.
 */
static bool validateDecryptedJson(const QJsonDocument &doc, bool isNested)
{
    if (!doc.isObject()) {
        printFail(u"Decrypted JSON: not a JSON object"_s);
        return false;
    }

    auto ok = true;
    const auto obj = doc.object();

    // counter – must be a non-negative JSON number.
    // isDouble() guards against missing/non-numeric values; toInteger() then
    // checks the numeric value is non-negative.
    if (!obj.value(u"counter"_s).isDouble()
        || obj.value(u"counter"_s).toInteger() < 0) {
        printFail(u"Decrypted JSON: 'counter' is missing or not a non-negative integer"_s);
        ok = false;
    } else {
        printVerbose(u"  counter = %1"_s
                         .arg(obj.value(u"counter"_s).toInteger()));
    }

    // files – must be a JSON object; each entry must have required fields
    if (!obj.value(u"files"_s).isObject()) {
        printFail(u"Decrypted JSON: 'files' is missing or not an object"_s);
        ok = false;
    } else {
        const auto files = obj.value(u"files"_s).toObject();
        printVerbose(u"  files entries: %1"_s.arg(files.size()));
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            const auto fileObj = it.value().toObject();
            if (fileObj.value(u"filename"_s).toString().isEmpty()) {
                printFail(u"Decrypted JSON: files['%1'].filename is missing or empty"_s
                              .arg(it.key()));
                ok = false;
            }
            if (fileObj.value(u"key"_s).toString().isEmpty()) {
                printFail(u"Decrypted JSON: files['%1'].key is missing or empty"_s
                              .arg(it.key()));
                ok = false;
            }
            if (fileObj.value(u"mimetype"_s).toString().isEmpty()) {
                printFail(u"Decrypted JSON: files['%1'].mimetype is missing or empty"_s
                              .arg(it.key()));
                ok = false;
            }
            // Accept "nonce" (v2) or legacy "initializationVector" (v1 compat)
            if (fileObj.value(u"nonce"_s).toString().isEmpty()
                && fileObj.value(u"initializationVector"_s).toString().isEmpty()) {
                printFail(u"Decrypted JSON: files['%1'] has neither 'nonce' "
                                          "nor 'initializationVector'"_s
                              .arg(it.key()));
                ok = false;
            }
            if (fileObj.value(u"authenticationTag"_s).toString().isEmpty()) {
                printFail(u"Decrypted JSON: files['%1'].authenticationTag is "
                                          "missing or empty"_s
                              .arg(it.key()));
                ok = false;
            }
        }
    }

    // folders – must be a JSON object; each value must be a non-empty string
    if (!obj.value(u"folders"_s).isObject()) {
        printFail(u"Decrypted JSON: 'folders' is missing or not an object"_s);
        ok = false;
    } else {
        const auto folders = obj.value(u"folders"_s).toObject();
        printVerbose(u"  folder entries: %1"_s.arg(folders.size()));
        for (auto it = folders.constBegin(); it != folders.constEnd(); ++it) {
            if (it.value().toString().isEmpty()) {
                printFail(u"Decrypted JSON: folders['%1'] has an empty folder name"_s
                              .arg(it.key()));
                ok = false;
            }
        }
    }

    // keyChecksums – required in root folders
    if (!isNested) {
        if (!obj.value(u"keyChecksums"_s).isArray()) {
            printFail(u"Decrypted JSON: 'keyChecksums' is missing or not an array "
                                      "(required for root folders)"_s);
            ok = false;
        } else if (obj.value(u"keyChecksums"_s).toArray().isEmpty()) {
            printFail(u"Decrypted JSON: 'keyChecksums' array is empty"_s);
            ok = false;
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// Key-checksum verification
// ---------------------------------------------------------------------------

/**
 * Verify that SHA-256(metadataKey[0..15]) appears in the "keyChecksums"
 * array of the decrypted plaintext JSON.
 *
 * The keyChecksums entries are lower-case hex-encoded SHA-256 digests,
 * as produced by OCC::calcSha256() / sha256Hex() above.
 */
static bool verifyKeyChecksum(const QJsonDocument &decryptedDoc,
                               const QByteArray &metadataKey)
{
    const auto keyFirst16 = metadataKey.left(kMetadataKeySize);
    const auto expectedHex = sha256Hex(keyFirst16);

    printVerbose(u"Expected key checksum (SHA-256 hex of first 16 key bytes): %1"_s
                     .arg(QString::fromLatin1(expectedHex)));

    const auto kcArr =
        decryptedDoc.object().value(u"keyChecksums"_s).toArray();
    for (const QJsonValue &kcVal : kcArr) {
        const auto kcStr = kcVal.toString().toUtf8();
        printVerbose(u"  Stored checksum: %1"_s.arg(QString::fromLatin1(kcStr)));
        if (kcStr == expectedHex) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// CMS signature helpers
// ---------------------------------------------------------------------------

/**
 * Prepare the inner metadata document for CMS signature verification by
 * removing the "filedrop" key and the "encryptedFiledropKey" field from each
 * user entry, then serialising as compact JSON.
 *
 * The result is then base64-encoded before being fed into CMS_verify(),
 * consistent with how the Nextcloud desktop client signs and verifies.
 * Mirrors FolderMetadata::prepareMetadataForSignature().
 */
static QByteArray prepareMetadataForSignature(const QJsonDocument &innerDoc)
{
    auto modifiedObj = innerDoc.object();
    modifiedObj.remove(u"filedrop"_s);

    if (modifiedObj.contains(u"users"_s)) {
        const auto folderUsers = modifiedObj.value(u"users"_s).toArray();
        QJsonArray modifiedUsers;
        for (const QJsonValue &userVal : folderUsers) {
            auto userObj = userVal.toObject();
            userObj.remove(u"encryptedFiledropKey"_s);
            modifiedUsers.push_back(userObj);
        }
        modifiedObj.insert(u"users"_s, modifiedUsers);
    }

    QJsonDocument modified;
    modified.setObject(modifiedObj);
    return modified.toJson(QJsonDocument::Compact);
}

/**
 * Verify the detached CMS (PKCS#7) signature over the inner metadata.
 *
 * @p signatureBase64  Base64-encoded DER CMS structure.
 * @p innerDoc         The inner metadata JSON document.
 * @p certificatePem   The signer's X.509 certificate in PEM format.
 *
 * The data that was originally signed is base64(compact_json_without_filedrop),
 * matching ClientSideEncryption::generateSignatureCryptographicMessageSyntax.
 *
 * Returns true if the signature is cryptographically valid and was produced
 * by the key matching @p certificatePem.
 */
static bool verifyCmsSignature(const QByteArray &signatureBase64,
                                const QJsonDocument &innerDoc,
                                const QByteArray &certificatePem)
{
    const auto signature = QByteArray::fromBase64(signatureBase64);

    auto *sigBio = BIO_new_mem_buf(signature.constData(), signature.size());
    auto *cms = d2i_CMS_bio(sigBio, nullptr);
    BIO_free(sigBio);
    if (!cms) {
        printVerbose(u"CMS: failed to parse DER CMS structure from signature file"_s);
        return false;
    }

    // Reconstruct signed data: base64(compact JSON without filedrop)
    const auto metadataCompact = prepareMetadataForSignature(innerDoc);
    const auto signedData = metadataCompact.toBase64();
    printVerbose(u"CMS signed data (first 80 chars): %1"_s
                     .arg(QString::fromLatin1(signedData.left(80))));

    auto *dataBio = BIO_new_mem_buf(signedData.constData(), signedData.size());
    const auto verifyResult =
        CMS_verify(cms, nullptr, nullptr, dataBio, nullptr,
                   CMS_DETACHED | CMS_NO_SIGNER_CERT_VERIFY);
    BIO_free(dataBio);

    if (verifyResult != 1) {
        CMS_ContentInfo_free(cms);
        printVerbose(u"CMS: CMS_verify failed (cryptographic mismatch)"_s);
        return false;
    }

    // Verify that the signer matches the supplied certificate
    auto *signerInfos = CMS_get0_SignerInfos(cms);
    if (!signerInfos) {
        CMS_ContentInfo_free(cms);
        return false;
    }

    auto *certBio = BIO_new_mem_buf(certificatePem.constData(), certificatePem.size());
    auto *cert = PEM_read_bio_X509(certBio, nullptr, nullptr, nullptr);
    BIO_free(certBio);
    if (!cert) {
        CMS_ContentInfo_free(cms);
        printVerbose(u"CMS: failed to parse certificate PEM for signer matching"_s);
        return false;
    }

    auto certMatches = false;
    const auto numSigners = sk_CMS_SignerInfo_num(signerInfos);
    for (int i = 0; i < numSigners; ++i) {
        auto *si = sk_CMS_SignerInfo_value(signerInfos, i);
        if (CMS_SignerInfo_cert_cmp(si, cert) == 0) {
            certMatches = true;
            break;
        }
    }

    X509_free(cert);
    CMS_ContentInfo_free(cms);
    return certMatches;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"nextcloud-e2ee-verify"_s);
    QCoreApplication::setApplicationVersion(u"1.0"_s);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        u"Validate Nextcloud end-to-end encrypted folder metadata (v2.0+)."_s);
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption privateKeyOption(
        u"private-key"_s,
        u"Path to the RSA private key PEM file (required for root folders)."_s,
        u"file"_s);
    parser.addOption(privateKeyOption);

    QCommandLineOption certificateOption(
        u"certificate"_s,
        u"Path to the X.509 certificate PEM file matching the private key "
                       "(required for root folders)."_s,
        u"file"_s);
    parser.addOption(certificateOption);

    QCommandLineOption metadataOption(
        u"metadata"_s,
        u"Path to the metadata JSON file (raw inner JSON or OCS-wrapped)."_s,
        u"file"_s);
    parser.addOption(metadataOption);

    QCommandLineOption signatureOption(
        u"signature"_s,
        u"Path to a file containing the base64-encoded detached CMS signature "
                       "(optional; enables CMS signature verification for root folders)."_s,
        u"file"_s);
    parser.addOption(signatureOption);

    QCommandLineOption nestedOption(
        u"nested"_s,
        u"Treat the metadata as belonging to a nested (non-root) encrypted folder. "
                       "--metadata-key must be supplied."_s);
    parser.addOption(nestedOption);

    QCommandLineOption metadataKeyOption(
        u"metadata-key"_s,
        u"Base64-encoded binary metadata key (required for --nested)."_s,
        u"base64"_s);
    parser.addOption(metadataKeyOption);

    QCommandLineOption verboseOption(
        u"verbose"_s,
        u"Print detailed debug information."_s);
    parser.addOption(verboseOption);

    parser.process(app);

    g_verbose = parser.isSet(verboseOption);
    const bool isNested = parser.isSet(nestedOption);

    // -----------------------------------------------------------------------
    // Validate required arguments
    // -----------------------------------------------------------------------

    if (!parser.isSet(metadataOption)) {
        qCCritical(lcE2eeVerify) << "Error: --metadata is required.";
        parser.showHelp(1);
    }

    if (!isNested && !parser.isSet(privateKeyOption)) {
        qCCritical(lcE2eeVerify) << "Error: --private-key is required for root folder metadata.";
        parser.showHelp(1);
    }

    if (!isNested && !parser.isSet(certificateOption)) {
        qCCritical(lcE2eeVerify) << "Error: --certificate is required for root folder metadata.";
        parser.showHelp(1);
    }

    if (isNested && !parser.isSet(metadataKeyOption)) {
        qCCritical(lcE2eeVerify) << "Error: --metadata-key is required when --nested is specified.";
        parser.showHelp(1);
    }

    // -----------------------------------------------------------------------
    // Load files
    // -----------------------------------------------------------------------

    const auto readFile = [](const QString &path) -> QByteArray {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            qCCritical(lcE2eeVerify) << "Error: cannot open" << qPrintable(path) << ":" << qPrintable(f.errorString());
            return {};
        }
        return f.readAll();
    };

    const QByteArray metadataFileContents = readFile(parser.value(metadataOption));
    if (metadataFileContents.isEmpty()) {
        qCCritical(lcE2eeVerify) << "Error: metadata file is empty or could not be read.";
        return 1;
    }

    QByteArray privateKeyPem, certificatePem, signatureBase64;

    if (!isNested) {
        privateKeyPem = readFile(parser.value(privateKeyOption));
        if (privateKeyPem.isEmpty()) {
            return 1;
        }
        certificatePem = readFile(parser.value(certificateOption));
        if (certificatePem.isEmpty()) {
            return 1;
        }
    }

    if (parser.isSet(signatureOption)) {
        signatureBase64 = readFile(parser.value(signatureOption)).trimmed();
        if (signatureBase64.isEmpty()) {
            qCCritical(lcE2eeVerify) << "Error: signature file is empty or could not be read.";
            return 1;
        }
    }

    // -----------------------------------------------------------------------
    // Step 1 – Parse JSON and handle OCS wrapping
    // -----------------------------------------------------------------------

    qCInfo(lcE2eeVerify) << "=== Nextcloud E2EE Metadata Validator ===";

    const auto innerDoc = extractInnerMetadata(metadataFileContents);
    if (innerDoc.isNull()) {
        printFail(u"JSON parsing: could not parse metadata file as valid JSON"_s);
        return 1;
    }
    printOk(u"JSON parsed successfully"_s);

    // -----------------------------------------------------------------------
    // Step 2 – Version check (reject < 2.0)
    // -----------------------------------------------------------------------

    const auto versionStr = detectVersionString(innerDoc);
    if (versionStr.isEmpty()) {
        printFail(u"Version check: version field not found in metadata"_s);
        return 1;
    }

    if (versionStr != u"2.0"_s && versionStr != u"2.1"_s) {
        printFail(u"Version check: metadata version '%1' is not supported. "
                                  "Only version 2.0 and later are accepted by this tool."_s
                      .arg(versionStr));
        return 1;
    }

    printOk(u"Metadata format: version %1"_s.arg(versionStr));

    // -----------------------------------------------------------------------
    // Step 3 – Structural schema validation
    // -----------------------------------------------------------------------

    auto allPassed = true;

    if (validateSchemaV2(innerDoc, isNested)) {
        printOk(u"Schema validation passed"_s);
    } else {
        allPassed = false;
    }

    // -----------------------------------------------------------------------
    // Step 4 – Obtain the metadata key
    // -----------------------------------------------------------------------

    QByteArray metadataKey;

    if (isNested) {
        metadataKey = QByteArray::fromBase64(parser.value(metadataKeyOption).toUtf8());
        if (metadataKey.isEmpty()) {
            printFail(u"Metadata key: --metadata-key value could not be decoded as base64"_s);
            return 1;
        }
        printOk(u"Metadata key loaded from --metadata-key (%1 bytes)"_s
                    .arg(metadataKey.size()));
    } else {
        QString matchedUserId;
        metadataKey = extractMetadataKey(innerDoc, privateKeyPem, certificatePem, matchedUserId);
        if (metadataKey.isEmpty()) {
            printFail(u"Metadata key: RSA-OAEP-SHA256 decryption failed – "
                                      "check that the private key matches a user in the metadata"_s);
            return 1;
        }
        printOk(u"Metadata key decrypted successfully (%1 bytes, userId: %2)"_s
                    .arg(metadataKey.size())
                    .arg(matchedUserId));
    }

    // -----------------------------------------------------------------------
    // Step 5 – Validate metadata key size
    // -----------------------------------------------------------------------

    if (metadataKey.size() != kMetadataKeySize) {
        printFail(u"Metadata key size: expected %1 bytes, got %2 – "
                                  "decryption will likely fail"_s
                      .arg(kMetadataKeySize)
                      .arg(metadataKey.size()));
        allPassed = false;
    } else {
        printOk(u"Metadata key size valid (%1 bytes)"_s.arg(metadataKey.size()));
    }

    // -----------------------------------------------------------------------
    // Step 6 – Decrypt and decompress the ciphertext
    // -----------------------------------------------------------------------

    const QByteArray decryptedBytes = decryptAndDecompressMetadata(innerDoc, metadataKey);
    if (decryptedBytes.isEmpty()) {
        printFail(u"Ciphertext decryption: AES-128-GCM decrypt or gzip decompress failed "
                                  "(wrong key, corrupted data, or bad GCM tag)"_s);
        return 1;
    }
    printOk(u"Ciphertext decrypted and decompressed (%1 bytes)"_s.arg(decryptedBytes.size()));

    // -----------------------------------------------------------------------
    // Step 7 – Validate decrypted JSON structure
    // -----------------------------------------------------------------------

    const auto decryptedDoc = QJsonDocument::fromJson(decryptedBytes);
    if (decryptedDoc.isNull()) {
        printFail(u"Decrypted JSON: failed to parse decrypted bytes as JSON"_s);
        allPassed = false;
    } else if (validateDecryptedJson(decryptedDoc, isNested)) {
        printOk(u"Decrypted JSON structure valid"_s);
    } else {
        allPassed = false;
    }

    // -----------------------------------------------------------------------
    // Step 8 – Verify metadata key checksum (root folders only)
    // -----------------------------------------------------------------------

    if (!isNested && !decryptedDoc.isNull()) {
        if (verifyKeyChecksum(decryptedDoc, metadataKey)) {
            printOk(u"Key checksum verified (SHA-256 of metadata key found in keyChecksums)"_s);
        } else {
            printFail(u"Key checksum: SHA-256 of metadata key not found in keyChecksums array"_s);
            allPassed = false;
        }
    }

    // -----------------------------------------------------------------------
    // Step 9 – Verify CMS signature (root folders, when --signature is given)
    // -----------------------------------------------------------------------

    if (!isNested && !signatureBase64.isEmpty()) {
        if (verifyCmsSignature(signatureBase64, innerDoc, certificatePem)) {
            printOk(u"CMS signature verified (cryptographically valid, "
                                    "matches provided certificate)"_s);
        } else {
            printFail(u"CMS signature: verification failed (invalid signature "
                                      "or certificate mismatch)"_s);
            allPassed = false;
        }
    } else if (!isNested && signatureBase64.isEmpty()) {
        printInfo(u"CMS signature check skipped (provide --signature to enable)"_s);
    }

    // -----------------------------------------------------------------------
    // Summary: list files and folders from decrypted JSON
    // -----------------------------------------------------------------------

    if (!decryptedDoc.isNull()) {
        const auto decryptedObj = decryptedDoc.object();
        const auto files = decryptedObj.value(u"files"_s).toObject();
        const auto folders = decryptedObj.value(u"folders"_s).toObject();

        printInfo(u"Files found: %1"_s.arg(files.size()));
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            const auto fileObj = it.value().toObject();
            const auto name = fileObj.value(u"filename"_s).toString();
            const auto mime = fileObj.value(u"mimetype"_s).toString();
            qCInfo(lcE2eeVerify) << "  -" << qPrintable(name) << " (enc:" << qPrintable(it.key().left(16)) << "...)" << qPrintable(mime);
        }

        printInfo(u"Folders found: %1"_s.arg(folders.size()));
        for (auto it = folders.constBegin(); it != folders.constEnd(); ++it) {
            const auto folderName = it.value().toString();
            qCInfo(lcE2eeVerify) << "  -" << qPrintable(folderName) << "/ (enc:" << qPrintable(it.key().left(16)) << "...)";
        }
    }

    // -----------------------------------------------------------------------
    // Final result
    // -----------------------------------------------------------------------


    if (allPassed) {
        qCInfo(lcE2eeVerify) << "=== All checks PASSED ===";
        return 0;
    } else {
        qCCritical(lcE2eeVerify) << "=== One or more checks FAILED ===";
        return 1;
    }
}
