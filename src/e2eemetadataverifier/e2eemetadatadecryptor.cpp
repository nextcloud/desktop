/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "e2eemetadatadecryptor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <zlib.h>

#include <memory>
#include <cstring>

namespace OCC
{

namespace
{

// ─── OpenSSL / zlib RAII helpers ─────────────────────────────────────────────

using EvpPkeyPtr    = std::unique_ptr<EVP_PKEY,     decltype(&EVP_PKEY_free)>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
using BioPtr        = std::unique_ptr<BIO,           decltype(&BIO_free_all)>;

// Drain the OpenSSL error queue into a human-readable string.
QString opensslErrors()
{
    QString result;
    unsigned long code = 0;
    while ((code = ERR_get_error()) != 0) {
        char buf[256];
        ERR_error_string_n(code, buf, sizeof(buf));
        if (!result.isEmpty()) {
            result += QLatin1Char('\n');
        }
        result += QString::fromLatin1(buf);
    }
    return result.isEmpty() ? QStringLiteral("(no OpenSSL error detail)") : result;
}

// ─── Primitive: load a PEM private key (encrypted or plain) ──────────────────

// OpenSSL passphrase callback used when the caller supplies the passphrase
// out-of-band (e.g. after reading it from the terminal).
// The `userdata` parameter points to the QByteArray holding the passphrase.
static int pemPassphraseCallback(char *buf, int size, int /*rwflag*/, void *userdata)
{
    const auto *passphrase = static_cast<const QByteArray *>(userdata);
    const int len = qMin(size, passphrase->size());
    std::memcpy(buf, passphrase->constData(), static_cast<size_t>(len));
    return len;
}

// OpenSSL passphrase callback that always returns 0 (empty passphrase) and
// suppresses the default interactive terminal prompt.
static int emptyPassphraseCallback(char * /*buf*/, int /*size*/, int /*rwflag*/, void * /*userdata*/)
{
    return 0;
}

EvpPkeyPtr loadPemPrivateKey(const QByteArray &pem, const QByteArray &passphrase, QString &error)
{
    BioPtr bio{BIO_new_mem_buf(pem.constData(), pem.size()), BIO_free_all};
    if (!bio) {
        error = QStringLiteral("Failed to create BIO for private key PEM");
        return {nullptr, EVP_PKEY_free};
    }

    EVP_PKEY *raw = nullptr;
    if (passphrase.isEmpty()) {
        // Key is assumed to be unencrypted.  Provide a no-op callback so that
        // OpenSSL does not fall back to prompting on the terminal.
        raw = PEM_read_bio_PrivateKey(bio.get(), nullptr, emptyPassphraseCallback, nullptr);
        if (!raw) {
            error = QStringLiteral("Failed to read private key PEM — "
                                   "if the key is passphrase-protected, supply the passphrase: ")
                    + opensslErrors();
            return {nullptr, EVP_PKEY_free};
        }
    } else {
        // Key is passphrase-protected: supply the passphrase via callback.
        raw = PEM_read_bio_PrivateKey(bio.get(), nullptr, pemPassphraseCallback,
                                      const_cast<void *>(static_cast<const void *>(&passphrase)));
        if (!raw) {
            error = QStringLiteral("Failed to read private key PEM — "
                                   "wrong passphrase or corrupted key: ")
                    + opensslErrors();
            return {nullptr, EVP_PKEY_free};
        }
    }

    return {raw, EVP_PKEY_free};
}

// ─── Primitive: RSA-OAEP-SHA256 decrypt ──────────────────────────────────────

// Decrypt @p ciphertext (raw binary, not base64) with RSA OAEP using SHA-256
// for both the OAEP digest and the MGF1 mask generation function.
// Returns the raw plaintext bytes on success, or an empty array on failure.
QByteArray rsaOaepDecrypt(EVP_PKEY *pkey, const QByteArray &ciphertext, QString &error)
{
    EvpPkeyCtxPtr ctx{EVP_PKEY_CTX_new(pkey, nullptr), EVP_PKEY_CTX_free};
    if (!ctx) {
        error = QStringLiteral("EVP_PKEY_CTX_new failed: ") + opensslErrors();
        return {};
    }
    if (EVP_PKEY_decrypt_init(ctx.get()) <= 0) {
        error = QStringLiteral("EVP_PKEY_decrypt_init failed: ") + opensslErrors();
        return {};
    }
    if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0) {
        error = QStringLiteral("Failed to set OAEP padding: ") + opensslErrors();
        return {};
    }
    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), EVP_sha256()) <= 0) {
        error = QStringLiteral("Failed to set OAEP digest to SHA-256: ") + opensslErrors();
        return {};
    }
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), EVP_sha256()) <= 0) {
        error = QStringLiteral("Failed to set MGF1 digest to SHA-256: ") + opensslErrors();
        return {};
    }

    const auto *inData = reinterpret_cast<const unsigned char *>(ciphertext.constData());
    const auto  inLen  = static_cast<size_t>(ciphertext.size());

    size_t outLen = 0;
    if (EVP_PKEY_decrypt(ctx.get(), nullptr, &outLen, inData, inLen) <= 0) {
        error = QStringLiteral("Failed to determine RSA output size: ") + opensslErrors();
        return {};
    }

    QByteArray out(static_cast<int>(outLen), '\0');
    auto *outData = reinterpret_cast<unsigned char *>(out.data());
    if (EVP_PKEY_decrypt(ctx.get(), outData, &outLen, inData, inLen) <= 0) {
        error = QStringLiteral("RSA OAEP decrypt failed: ") + opensslErrors();
        return {};
    }
    out.resize(static_cast<int>(outLen));
    return out;
}

// ─── Primitive: AES-128-GCM decrypt ──────────────────────────────────────────

constexpr int gcmTagSize = 16;

// Decrypt @p ciphertextWithTag using AES-128-GCM.
// The last gcmTagSize bytes of @p ciphertextWithTag are the GCM authentication tag.
// Returns the plaintext on success, or an empty array on failure (including tag mismatch).
QByteArray aes128GcmDecrypt(const QByteArray &key,
                             const QByteArray &iv,
                             const QByteArray &ciphertextWithTag,
                             QString &error)
{
    if (ciphertextWithTag.size() < gcmTagSize) {
        error = QStringLiteral("Ciphertext is too short to contain a %1-byte GCM authentication tag").arg(gcmTagSize);
        return {};
    }

    const QByteArray ciphertext = ciphertextWithTag.left(ciphertextWithTag.size() - gcmTagSize);
    QByteArray tag = ciphertextWithTag.right(gcmTagSize); // needs non-const pointer for EVP_CTRL

    EvpCipherCtxPtr ctx{EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free};
    if (!ctx) {
        error = QStringLiteral("EVP_CIPHER_CTX_new failed: ") + opensslErrors();
        return {};
    }

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_gcm(), nullptr, nullptr, nullptr) != 1) {
        error = QStringLiteral("EVP_DecryptInit_ex (cipher selection) failed: ") + opensslErrors();
        return {};
    }

    EVP_CIPHER_CTX_set_padding(ctx.get(), 0);

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
        error = QStringLiteral("Failed to set GCM IV length: ") + opensslErrors();
        return {};
    }

    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) != 1) {
        error = QStringLiteral("EVP_DecryptInit_ex (key/iv) failed: ") + opensslErrors();
        return {};
    }

    // Allocate enough space: GCM never expands the ciphertext.
    QByteArray plaintext(ciphertext.size(), '\0');
    int updateLen = 0;
    if (EVP_DecryptUpdate(ctx.get(),
                          reinterpret_cast<unsigned char *>(plaintext.data()), &updateLen,
                          reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                          ciphertext.size()) != 1) {
        error = QStringLiteral("EVP_DecryptUpdate failed: ") + opensslErrors();
        return {};
    }

    // The GCM tag must be set before calling Final.
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, gcmTagSize,
                             reinterpret_cast<unsigned char *>(tag.data())) != 1) {
        error = QStringLiteral("Failed to set GCM authentication tag: ") + opensslErrors();
        return {};
    }

    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx.get(),
                            reinterpret_cast<unsigned char *>(plaintext.data()) + updateLen,
                            &finalLen) <= 0) {
        // This is the most common failure point when the wrong key is used.
        error = QStringLiteral("GCM authentication tag verification failed — wrong key or corrupted data");
        return {};
    }

    plaintext.resize(updateLen + finalLen);
    return plaintext;
}

// ─── Primitive: gzip decompress ──────────────────────────────────────────────

// Decompress @p compressed using the gzip format (RFC 1952).
// Returns the decompressed data on success, or an empty array on failure.
QByteArray gzipDecompress(const QByteArray &compressed, QString &error)
{
    // inflateInit2 with (MAX_WBITS | 16) enables automatic gzip header detection.
    z_stream zs{};
    if (inflateInit2(&zs, MAX_WBITS | 16) != Z_OK) {
        error = QStringLiteral("inflateInit2 failed — zlib not available");
        return {};
    }

    zs.next_in  = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    zs.avail_in = static_cast<uInt>(compressed.size());

    QByteArray output;
    output.reserve(compressed.size() * 4); // heuristic starting capacity

    QByteArray chunk(65536, '\0');
    int ret = Z_OK;
    do {
        zs.next_out  = reinterpret_cast<Bytef *>(chunk.data());
        zs.avail_out = static_cast<uInt>(chunk.size());
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&zs);
            error = QStringLiteral("gzip inflate failed with zlib error %1").arg(ret);
            return {};
        }
        output.append(chunk.constData(),
                      chunk.size() - static_cast<int>(zs.avail_out));
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
    return output;
}

// ─── V2.x decryption ─────────────────────────────────────────────────────────

E2EEMetadataDecryptor::Result decryptV2(const QJsonObject &root, EVP_PKEY *pkey)
{
    // Step 1 — decrypt the metadata key for this private key.
    //
    // The "users" array is only present in root folder metadata.  Nested folder
    // metadata inherits the key from the root and therefore cannot be decrypted
    // without first decrypting the root folder metadata.
    const auto usersVal = root[QStringLiteral("users")];
    if (!usersVal.isArray()) {
        return {false,
                QStringLiteral("\"users\" array is absent — this looks like nested folder metadata.\n"
                               "Nested folders inherit their encryption key from the root folder metadata;\n"
                               "provide the root folder metadata file instead.")};
    }

    QByteArray binaryMetadataKey;
    QString matchedUserId;

    for (const auto &userVal : usersVal.toArray()) {
        if (!userVal.isObject()) {
            continue;
        }
        const auto userObj = userVal.toObject();
        const auto encKeyB64 = userObj[QStringLiteral("encryptedMetadataKey")].toString().toUtf8();
        if (encKeyB64.isEmpty()) {
            continue;
        }

        // The field holds base64( rsaEncrypt( binaryAesKey ) ).
        // Decode the base64 wrapper to obtain the raw RSA ciphertext.
        const auto ciphertext = QByteArray::fromBase64(encKeyB64);
        QString rsaError;
        const auto decrypted = rsaOaepDecrypt(pkey, ciphertext, rsaError);
        if (!decrypted.isEmpty()) {
            binaryMetadataKey = decrypted;
            matchedUserId = userObj[QStringLiteral("userId")].toString();
            break;
        }
    }

    if (binaryMetadataKey.isEmpty()) {
        return {false,
                QStringLiteral("Could not decrypt the metadata key with the provided private key.\n"
                               "Make sure the key belongs to a user listed in the metadata's \"users\" array.")};
    }

    // Step 2 — AES-128-GCM decrypt the ciphertext payload.
    //
    // "ciphertext" format: base64( cipherdataWithTag ) | base64( iv )
    // "nonce"      format: base64( iv )  (identical IV as in "ciphertext")
    const auto metadataObj = root[QStringLiteral("metadata")].toObject();
    const auto ciphertextField = metadataObj[QStringLiteral("ciphertext")].toString().toUtf8();
    if (ciphertextField.isEmpty()) {
        return {false, QStringLiteral("metadata.ciphertext field is missing")};
    }

    const auto ciphertextWithTag = QByteArray::fromBase64(ciphertextField.split('|').at(0));

    const auto nonceB64 = metadataObj[QStringLiteral("nonce")].toString().toUtf8();
    if (nonceB64.isEmpty()) {
        return {false, QStringLiteral("metadata.nonce field is missing")};
    }
    const auto nonce = QByteArray::fromBase64(nonceB64);

    QString aesError;
    const auto compressed = aes128GcmDecrypt(binaryMetadataKey, nonce, ciphertextWithTag, aesError);
    if (compressed.isEmpty()) {
        return {false, QStringLiteral("AES-128-GCM decryption failed: ") + aesError};
    }

    // Step 3 — gzip-decompress the AES plaintext.
    QString gzipError;
    const auto innerJsonBytes = gzipDecompress(compressed, gzipError);
    if (innerJsonBytes.isEmpty()) {
        return {false, QStringLiteral("gzip decompression of decrypted payload failed: ") + gzipError};
    }

    // Step 4 — parse the resulting JSON.
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(innerJsonBytes, &parseError);
    if (doc.isNull()) {
        return {false,
                QStringLiteral("Decrypted payload is not valid JSON: ") + parseError.errorString()};
    }

    return {true, {}, doc.object()};
}

// ─── V1.x decryption ─────────────────────────────────────────────────────────

// Decrypt a single V1.x file entry's "encrypted" field.
//
// The field holds  base64( AES-128-GCM( base64(fileJson) ) ) | base64( iv )
// where the last gcmTagSize bytes of the AES output are the GCM tag.
// Returns the decrypted file JSON object, or an empty object on failure.
QJsonObject decryptV1FileEntry(const QString &encryptedField,
                               const QByteArray &binaryKey,
                               QString &error)
{
    const auto parts = encryptedField.toUtf8().split('|');
    if (parts.size() < 2) {
        error = QStringLiteral("Unexpected format (expected \"<base64>|<base64>\")");
        return {};
    }

    const auto ciphertextWithTag = QByteArray::fromBase64(parts.at(0));
    const auto iv                = QByteArray::fromBase64(parts.at(1));

    QString aesError;
    const auto decryptedBase64 = aes128GcmDecrypt(binaryKey, iv, ciphertextWithTag, aesError);
    if (decryptedBase64.isEmpty()) {
        error = QStringLiteral("AES-128-GCM decryption failed: ") + aesError;
        return {};
    }

    // The AES plaintext is itself base64-encoded (the legacy code base64-encoded
    // the file JSON before encrypting it).
    const auto fileJsonBytes = QByteArray::fromBase64(decryptedBase64);

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(fileJsonBytes, &parseError);
    if (doc.isNull()) {
        error = QStringLiteral("Decrypted file entry is not valid JSON: ") + parseError.errorString();
        return {};
    }
    return doc.object();
}

E2EEMetadataDecryptor::Result decryptV1(const QJsonObject &root,
                                         const QString &version,
                                         EVP_PKEY *pkey)
{
    const auto metadataObj = root[QStringLiteral("metadata")].toObject();

    // Step 1 — decrypt the metadata key.
    //
    // All V1.x metadata keys are RSA-OAEP-SHA256 encrypted and base64-encoded in
    // the JSON.  The number of base64 layers wrapping the raw AES key bytes inside
    // the RSA plaintext depends on the version:
    //
    //  V1.0   RSA plaintext = base64( aesKey )          → 1 fromBase64 after RSA decrypt
    //  V1.1   RSA plaintext = base64( base64( aesKey )) → 2 fromBase64 after RSA decrypt
    //  V1.2   same as V1.1
    QByteArray binaryKey;

    if (version == QStringLiteral("1.0")) {
        // V1.0 uses a "metadataKeys" map; use the last (highest-indexed) entry.
        const auto metadataKeys = metadataObj[QStringLiteral("metadataKeys")].toObject();
        if (metadataKeys.isEmpty()) {
            return {false, QStringLiteral("metadata.metadataKeys object is missing or empty")};
        }
        const auto lastKeyName = metadataKeys.keys().last();
        const auto encKeyB64   = metadataKeys[lastKeyName].toString().toUtf8();
        if (encKeyB64.isEmpty()) {
            return {false,
                    QStringLiteral("metadata.metadataKeys[\"%1\"] value is empty").arg(lastKeyName)};
        }

        QString rsaError;
        const auto rawDecrypted = rsaOaepDecrypt(pkey, QByteArray::fromBase64(encKeyB64), rsaError);
        if (rawDecrypted.isEmpty()) {
            return {false,
                    QStringLiteral("RSA decrypt of metadataKeys[\"%1\"] failed: %2")
                        .arg(lastKeyName, rsaError)};
        }
        // V1.0: RSA plaintext = base64( aesKey )
        binaryKey = QByteArray::fromBase64(rawDecrypted);

    } else {
        // V1.1 / V1.2 — single "metadataKey" field.
        const auto encKeyB64 = metadataObj[QStringLiteral("metadataKey")].toString().toUtf8();
        if (encKeyB64.isEmpty()) {
            return {false, QStringLiteral("metadata.metadataKey field is missing or empty")};
        }

        QString rsaError;
        const auto rawDecrypted = rsaOaepDecrypt(pkey, QByteArray::fromBase64(encKeyB64), rsaError);
        if (rawDecrypted.isEmpty()) {
            return {false,
                    QStringLiteral("RSA decrypt of metadata.metadataKey failed: ") + rsaError};
        }
        // V1.1/V1.2: RSA plaintext = base64( base64( aesKey ) )
        binaryKey = QByteArray::fromBase64(QByteArray::fromBase64(rawDecrypted));
    }

    if (binaryKey.isEmpty()) {
        return {false, QStringLiteral("Derived binary metadata key is empty after RSA decryption")};
    }

    // Step 2 — decrypt each file entry.
    const auto filesObj = root[QStringLiteral("files")].toObject();
    if (filesObj.isEmpty()) {
        // An empty folder is valid — return an empty files object.
        return {true, {}, {{QStringLiteral("files"), QJsonObject{}}}};
    }

    QJsonObject decryptedFiles;
    for (auto it = filesObj.constBegin(); it != filesObj.constEnd(); ++it) {
        const auto encryptedFilename = it.key();
        const auto fileEntry         = it.value().toObject();
        const auto encryptedField    = fileEntry[QStringLiteral("encrypted")].toString();

        if (encryptedField.isEmpty()) {
            decryptedFiles.insert(encryptedFilename,
                                  QJsonObject{{QStringLiteral("error"),
                                               QStringLiteral("\"encrypted\" field is absent")}});
            continue;
        }

        QString fileError;
        const auto decryptedObj = decryptV1FileEntry(encryptedField, binaryKey, fileError);
        if (decryptedObj.isEmpty()) {
            decryptedFiles.insert(encryptedFilename,
                                  QJsonObject{{QStringLiteral("error"), fileError}});
        } else {
            decryptedFiles.insert(encryptedFilename, decryptedObj);
        }
    }

    return {true, {}, {{QStringLiteral("files"), decryptedFiles}}};
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

bool E2EEMetadataDecryptor::isPemEncrypted(const QByteArray &privateKeyPem)
{
    // PKCS#8 encrypted format:  -----BEGIN ENCRYPTED PRIVATE KEY-----
    // Traditional encrypted format has a "Proc-Type: 4,ENCRYPTED" DEK-Info header.
    // Both formats contain the word "ENCRYPTED" in their PEM header, so a single
    // substring scan is sufficient.
    return privateKeyPem.contains("ENCRYPTED");
}

E2EEMetadataDecryptor::Result E2EEMetadataDecryptor::decrypt(const QJsonObject &metadataRoot,
                                                              const QString &version,
                                                              const QByteArray &privateKeyPem,
                                                              const QByteArray &passphrase)
{
    QString keyError;
    auto pkey = loadPemPrivateKey(privateKeyPem, passphrase, keyError);
    if (!pkey) {
        return {false, keyError};
    }

    if (version == QStringLiteral("1.0")
        || version == QStringLiteral("1.1")
        || version == QStringLiteral("1.2")) {
        return decryptV1(metadataRoot, version, pkey.get());
    }

    if (version == QStringLiteral("2.0")
        || version == QStringLiteral("2")
        || version == QStringLiteral("2.1")) {
        return decryptV2(metadataRoot, pkey.get());
    }

    return {false,
            QStringLiteral("Unsupported or unrecognized metadata version: \"%1\"").arg(version)};
}

} // namespace OCC
