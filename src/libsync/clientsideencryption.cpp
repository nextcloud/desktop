/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "clientsideencryption.h"

#include "account.h"
#include "capabilities.h"
#include "networkjobs.h"
#include "clientsideencryptionjobs.h"
#include "theme.h"
#include "creds/abstractcredentials.h"
#include "common/utility.h"
#include "common/constants.h"
#include <common/checksums.h>
#include "wordlist.h"

#include <qt6keychain/keychain.h>

#include <KCompressionDevice>

#include <QDebug>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>
#include <QXmlStreamReader>
#include <QXmlStreamNamespaceDeclaration>
#include <QStack>
#include <QInputDialog>
#include <QMessageBox>
#include <QWidget>
#include <QLineEdit>
#include <QIODevice>
#include <QUuid>
#include <QScopeGuard>
#include <QRandomGenerator>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QSslCertificate>
#include <QSslCertificateExtension>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/rand.h>
#include <openssl/cms.h>

#include <map>
#include <string>
#include <algorithm>
#include <optional>
#include <cstdio>

QDebug operator<<(QDebug out, const std::string& str)
{
    out << QString::fromStdString(str);
    return out;
}

using namespace QKeychain;
using namespace Qt::StringLiterals;

namespace OCC
{

Q_LOGGING_CATEGORY(lcCse, "nextcloud.sync.clientsideencryption", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCseDecryption, "nextcloud.sync.clientsideencryption.decryption", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCseEncryption, "nextcloud.sync.clientsideencryption.encryption", QtInfoMsg)

QString e2eeBaseUrl(const OCC::AccountPtr &account)
{
    Q_ASSERT(account);
    if (!account) {
        qCWarning(lcCse()) << "Account must be not null!";
    }
    const QString apiVersion = account && account->capabilities().clientSideEncryptionVersion() >= 2.0
        ? QStringLiteral("v2")
        : QStringLiteral("v1");
    return QStringLiteral("ocs/v2.php/apps/end_to_end_encryption/api/%1/").arg(apiVersion);
}

namespace {
constexpr char accountProperty[] = "account";

constexpr char e2e_cert[] = "_e2e-certificate";
constexpr auto e2e_cert_sharing = "_sharing";
constexpr char e2e_private[] = "_e2e-private";
constexpr char e2e_public[] = "_e2e-public";
constexpr char e2e_mnemonic[] = "_e2e-mnemonic";

constexpr qint64 blockSize = 1024;

QList<QByteArray> oldCipherFormatSplit(const QByteArray &cipher)
{
    const auto separator = QByteArrayLiteral("fA=="); // BASE64 encoded '|'
    auto result = QList<QByteArray>();

    auto data = cipher;
    auto index = data.indexOf(separator);
    while (index >=0) {
        result.append(data.left(index));
        data = data.mid(index + separator.size());
        index = data.indexOf(separator);
    }

    result.append(data);
    return result;
}

QList<QByteArray> splitCipherParts(const QByteArray &data)
{
    const auto isOldFormat = !data.contains('|');
    const auto parts = isOldFormat ? oldCipherFormatSplit(data) : data.split('|');
    qCInfo(lcCse()) << "found parts:" << parts << "old format?" << isOldFormat;
    return parts;
}
} // ns

namespace {
unsigned char* unsignedData(QByteArray& array)
{
    return (unsigned char*)array.data();
}

       //
       // Simple classes for safe (RAII) handling of OpenSSL
       // data structures
       //

class CipherCtx {
public:
    CipherCtx()
        : _ctx(EVP_CIPHER_CTX_new())
    {
    }

    ~CipherCtx()
    {
        EVP_CIPHER_CTX_free(_ctx);
    }

    operator EVP_CIPHER_CTX*()
    {
        return _ctx;
    }

private:
    Q_DISABLE_COPY(CipherCtx)

    EVP_CIPHER_CTX* _ctx;
};
}

namespace
{
class X509Certificate {
public:
    ~X509Certificate()
    {
        X509_free(_certificate);
    }

    // The move constructor is needed for pre-C++17 where
    // return-value optimization (RVO) is not obligatory
    // and we have a static functions that return
    // an instance of this class
    X509Certificate(X509Certificate&& other)
    {
        std::swap(_certificate, other._certificate);
    }

    X509Certificate& operator=(X509Certificate&& other) = delete;

    static X509Certificate readCertificate(Bio &bio)
    {
        X509Certificate result;
        result._certificate = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
        return result;
    }

    operator X509*()
    {
        return _certificate;
    }

    operator X509*() const
    {
        return _certificate;
    }

private:
    Q_DISABLE_COPY(X509Certificate)

    X509Certificate() = default;

    X509* _certificate = nullptr;
};

QByteArray BIO2ByteArray(Bio &b)
{
    auto pending = static_cast<int>(BIO_ctrl_pending(b));
    QByteArray res(pending, '\0');
    BIO_read(b, unsignedData(res), pending);
    return res;
}

QByteArray handleErrors()
{
    Bio bioErrors;
    ERR_print_errors(bioErrors); // This line is not printing anything.
    return BIO2ByteArray(bioErrors);
}
}


namespace EncryptionHelper {
QByteArray generateRandomFilename()
{
    return QUuid::createUuid().toRfc4122().toHex();
}

QByteArray generateRandom(int size)
{
    QByteArray result(size, '\0');

    int ret = RAND_bytes(unsignedData(result), size);
    if (ret != 1) {
        qCInfo(lcCse()) << "Random byte generation failed!";
        // Error out?
    }

    return result;
}

QByteArray deprecatedGeneratePassword(const QString& wordlist, const QByteArray& salt)
{
    const auto iterationCount = 1024;
    const auto keyStrength = 256;
    const auto keyLength = keyStrength / 8;

    QByteArray secretKey(keyLength, '\0');

    const auto ret = PKCS5_PBKDF2_HMAC(wordlist.toLocal8Bit().constData(),     // const char *password,
                                       wordlist.size(),                        // int password length,
                                       (const unsigned char *)salt.constData(),// const unsigned char *salt,
                                       salt.size(),                            // int saltlen,
                                       iterationCount,                         // int iterations,
                                       EVP_sha1(),                             // deprecated digest algorithm
                                       keyLength,                              // int keylen,
                                       unsignedData(secretKey));               // unsigned char *out

    if (ret != 1) {
        qCWarning(lcCse()) << "Failed to generate encryption key";
        // Error out?
    }

    return secretKey;
}

QByteArray deprecatedSha1GeneratePassword(const QString& wordlist, const QByteArray& salt)
{
    const auto iterationCount = 600000;
    const auto keyStrength = 256;
    const auto keyLength = keyStrength / 8;

    QByteArray secretKey(keyLength, '\0');

    const auto ret = PKCS5_PBKDF2_HMAC(wordlist.toLocal8Bit().constData(),     // const char *password,
                                       wordlist.size(),                        // int password length,
                                       (const unsigned char *)salt.constData(),// const unsigned char *salt,
                                       salt.size(),                            // int saltlen,
                                       iterationCount,                         // int iterations,
                                       EVP_sha1(),                             // deprecated digest algorithm
                                       keyLength,                              // int keylen,
                                       unsignedData(secretKey));               // unsigned char *out

    if (ret != 1) {
        qCWarning(lcCse()) << "Failed to generate encryption key";
        // Error out?
    }

    return secretKey;
}

QByteArray generatePassword(const QString& wordlist, const QByteArray& salt)
{
    const auto iterationCount = 600000;
    const auto keyStrength = 256;
    const auto keyLength = keyStrength / 8;

    QByteArray secretKey(keyLength, '\0');

    const auto ret = PKCS5_PBKDF2_HMAC(wordlist.toLocal8Bit().constData(),     // const char *password,
                                       wordlist.size(),                        // int password length,
                                       (const unsigned char *)salt.constData(),// const unsigned char *salt,
                                       salt.size(),                            // int saltlen,
                                       iterationCount,                         // int iterations,
                                       EVP_sha256(),                           // digest algorithm
                                       keyLength,                              // int keylen,
                                       unsignedData(secretKey));               // unsigned char *out

    if (ret != 1) {
        qCWarning(lcCse()) << "Failed to generate encryption key";
        // Error out?
    }

    return secretKey;
}

QByteArray encryptPrivateKey(
    const QByteArray& key,
    const QByteArray& privateKey,
    const QByteArray& salt
    ) {

    QByteArray iv = generateRandom(12);

    CipherCtx ctx;

    /* Create and initialise the context */
    if(!ctx) {
        qCInfo(lcCse()) << "Error creating cipher" << handleErrors();
    }

    /* Initialise the decryption operation. */
    if(!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)) {
        qCInfo(lcCse()) << "Error initializing context with aes_256" << handleErrors();
    }

    // No padding
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
        qCInfo(lcCse()) << "Error setting iv length" << handleErrors();
    }

    /* Initialise key and IV */
    if(!EVP_EncryptInit_ex(ctx, nullptr, nullptr, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv" << handleErrors();
    }

    // We write the base64 encoded private key
    QByteArray privateKeyB64 = privateKey.toBase64();

    // Make sure we have enough room in the cipher text
    QByteArray ctext(privateKeyB64.size() + 32, '\0');

    // Do the actual encryption
    int len = 0;
    if(!EVP_EncryptUpdate(ctx, unsignedData(ctext), &len, (unsigned char *)privateKeyB64.constData(), privateKeyB64.size())) {
        qCInfo(lcCse()) << "Error encrypting" << handleErrors();
    }

    int clen = len;

    /* Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if(1 != EVP_EncryptFinal_ex(ctx, unsignedData(ctext) + len, &len)) {
        qCInfo(lcCse()) << "Error finalizing encryption" << handleErrors();
    }
    clen += len;

    /* Get the e2EeTag */
    QByteArray e2EeTag(OCC::Constants::e2EeTagSize, '\0');
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, OCC::Constants::e2EeTagSize, unsignedData(e2EeTag))) {
        qCInfo(lcCse()) << "Error getting the e2EeTag" << handleErrors();
    }

    QByteArray cipherTXT;
    cipherTXT.reserve(clen + OCC::Constants::e2EeTagSize);
    cipherTXT.append(ctext, clen);
    cipherTXT.append(e2EeTag);

    QByteArray result = cipherTXT.toBase64();
    result += '|';
    result += iv.toBase64();
    result += '|';
    result += salt.toBase64();

    return result;
}

QByteArray decryptPrivateKey(const QByteArray& key, const QByteArray& data) {
    qCInfo(lcCse()) << "decryptStringSymmetric key: " << key;
    qCInfo(lcCse()) << "decryptStringSymmetric data: " << data;

    const auto parts = splitCipherParts(data);
    if (parts.size() < 2) {
        qCInfo(lcCse()) << "Not enough parts found";
        return QByteArray();
    }

    QByteArray cipherTXT64 = parts.at(0);
    QByteArray ivB64 = parts.at(1);

    qCInfo(lcCse()) << "decryptStringSymmetric cipherTXT: " << cipherTXT64;
    qCInfo(lcCse()) << "decryptStringSymmetric IV: " << ivB64;

    QByteArray cipherTXT = QByteArray::fromBase64(cipherTXT64);
    QByteArray iv = QByteArray::fromBase64(ivB64);

    const QByteArray e2EeTag = cipherTXT.right(OCC::Constants::e2EeTagSize);
    cipherTXT.chop(OCC::Constants::e2EeTagSize);

    // Init
    CipherCtx ctx;

    /* Create and initialise the context */
    if(!ctx) {
        qCInfo(lcCse()) << "Error creating cipher";
        return QByteArray();
    }

    /* Initialise the decryption operation. */
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)) {
        qCInfo(lcCse()) << "Error initialising context with aes 256";
        return QByteArray();
    }

    /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
        qCInfo(lcCse()) << "Error setting IV size";
        return QByteArray();
    }

    /* Initialise key and IV */
    if(!EVP_DecryptInit_ex(ctx, nullptr, nullptr, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv";
        return QByteArray();
    }

    QByteArray ptext(cipherTXT.size() + OCC::Constants::e2EeTagSize, '\0');
    int plen = 0;

    /* Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if(!EVP_DecryptUpdate(ctx, unsignedData(ptext), &plen, (unsigned char *)cipherTXT.constData(), cipherTXT.size())) {
        qCInfo(lcCse()) << "Could not decrypt";
        return QByteArray();
    }

    /* Set expected e2EeTag value. Works in OpenSSL 1.0.1d and later */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, e2EeTag.size(), (unsigned char *)e2EeTag.constData())) {
        qCInfo(lcCse()) << "Could not set e2EeTag";
        return QByteArray();
    }

    /* Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    int len = plen;
    if (EVP_DecryptFinal_ex(ctx, unsignedData(ptext) + plen, &len) == 0) {
        qCInfo(lcCse()) << "Tag did not match!";
        return QByteArray();
    }

    QByteArray result(ptext, plen);
    return QByteArray::fromBase64(result);
}

QByteArray extractPrivateKeySalt(const QByteArray &data)
{
    const auto parts = splitCipherParts(data);
    if (parts.size() < 3) {
        qCInfo(lcCse()) << "Not enough parts found";
        return QByteArray();
    }

    return QByteArray::fromBase64(parts.at(2));
}

QByteArray decryptStringSymmetric(const QByteArray& key, const QByteArray& data) {
    qCInfo(lcCse()) << "decryptStringSymmetric key: " << key;
    qCInfo(lcCse()) << "decryptStringSymmetric data: " << data;

    const auto parts = splitCipherParts(data);
    if (parts.size() < 2) {
        qCInfo(lcCse()) << "Not enough parts found";
        return QByteArray();
    }

    QByteArray cipherTXT64 = parts.at(0);
    QByteArray ivB64 = parts.at(1);

    qCInfo(lcCse()) << "decryptStringSymmetric cipherTXT: " << cipherTXT64;
    qCInfo(lcCse()) << "decryptStringSymmetric IV: " << ivB64;

    QByteArray cipherTXT = QByteArray::fromBase64(cipherTXT64);
    QByteArray iv = QByteArray::fromBase64(ivB64);

    const QByteArray e2EeTag = cipherTXT.right(OCC::Constants::e2EeTagSize);
    cipherTXT.chop(OCC::Constants::e2EeTagSize);

    // Init
    CipherCtx ctx;

    /* Create and initialise the context */
    if(!ctx) {
        qCInfo(lcCse()) << "Error creating cipher";
        return QByteArray();
    }

    /* Initialise the decryption operation. */
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr)) {
        qCInfo(lcCse()) << "Error initialising context with aes 128";
        return QByteArray();
    }

    /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
        qCInfo(lcCse()) << "Error setting IV size";
        return QByteArray();
    }

    /* Initialise key and IV */
    if(!EVP_DecryptInit_ex(ctx, nullptr, nullptr, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv";
        return QByteArray();
    }

    QByteArray ptext(cipherTXT.size() + OCC::Constants::e2EeTagSize, '\0');
    int plen = 0;

    /* Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if(!EVP_DecryptUpdate(ctx, unsignedData(ptext), &plen, (unsigned char *)cipherTXT.constData(), cipherTXT.size())) {
        qCInfo(lcCse()) << "Could not decrypt";
        return QByteArray();
    }

    /* Set expected e2EeTag value. Works in OpenSSL 1.0.1d and later */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, e2EeTag.size(), (unsigned char *)e2EeTag.constData())) {
        qCInfo(lcCse()) << "Could not set e2EeTag";
        return QByteArray();
    }

    /* Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    int len = plen;
    if (EVP_DecryptFinal_ex(ctx, unsignedData(ptext) + plen, &len) == 0) {
        qCInfo(lcCse()) << "Tag did not match!";
        return QByteArray();
    }

    return QByteArray::fromBase64(QByteArray(ptext, plen));
}

QByteArray privateKeyToPem(const QByteArray key) {
    Bio privateKeyBio;
    BIO_write(privateKeyBio, key.constData(), key.size());
    auto pkey = PKey::readPrivateKey(privateKeyBio);

    Bio pemBio;
    PEM_write_bio_PKCS8PrivateKey(pemBio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    QByteArray pem = BIO2ByteArray(pemBio);

    return pem;
}

namespace internals {

[[nodiscard]] OCC::Result<QByteArray, OCC::ClientSideEncryption::EncryptionErrorType> encryptStringAsymmetric(ClientSideEncryption &encryptionEngine,
                                                                                                              EVP_PKEY *publicKey,
                                                                                                              int pad_mode,
                                                                                                              const QByteArray& binaryData);

[[nodiscard]] OCC::Result<QByteArray, OCC::ClientSideEncryption::EncryptionErrorType> decryptStringAsymmetric(ClientSideEncryption &encryptionEngine,
                                                                                                              EVP_PKEY *privateKey,
                                                                                                              int pad_mode,
                                                                                                              const QByteArray& binaryData);

}

std::optional<QByteArray> encryptStringAsymmetric(const CertificateInformation &selectedCertificate,
                                                  const int paddingMode,
                                                  ClientSideEncryption &encryptionEngine,
                                                  const QByteArray &binaryData)
{
    if (!encryptionEngine.isInitialized()) {
        qCWarning(lcCseDecryption()) << "end-to-end encryption is disabled";
        return {};
    }

    if (encryptionEngine.useTokenBasedEncryption()) {
        qCDebug(lcCseDecryption()) << "use certificate on hardware token" << selectedCertificate.sha256Fingerprint();
    } else {
        qCDebug(lcCseDecryption()) << "use certificate on software storage" << selectedCertificate.sha256Fingerprint();
    }

    auto encryptedBase64Result = QByteArray{};
    bool needHardwareTokenEncryptionInit = false;
    for (auto i = 0; i < 2; ++i) {
        if (needHardwareTokenEncryptionInit) {
            encryptionEngine.initializeHardwareTokenEncryption(nullptr);
        }

        const auto publicKey = selectedCertificate.getEvpPublicKey();
        Q_ASSERT(publicKey);

        const auto encryptionResult = internals::encryptStringAsymmetric(encryptionEngine, publicKey, paddingMode, binaryData);

        if (encryptionResult) {
            encryptedBase64Result = *encryptionResult;
            break;
        } else if (encryptionResult.error() == ClientSideEncryption::EncryptionErrorType::RetryOnError) {
            qCInfo(lcCseDecryption()) << "retry encryption after error";
            needHardwareTokenEncryptionInit = true;
            continue;
        } else {
            qCWarning(lcCseEncryption()) << "encrypt failed";
            return {};
        }
    }

    if (encryptedBase64Result.isEmpty()) {
        qCWarning(lcCseEncryption()) << "ERROR. Could not encrypt data";
        return {};
    }

    return encryptedBase64Result;
}

std::optional<QByteArray> decryptStringAsymmetric(const CertificateInformation &selectedCertificate,
                                                  const int paddingMode,
                                                  ClientSideEncryption &encryptionEngine,
                                                  const QByteArray &base64Data)
{
    if (!encryptionEngine.isInitialized()) {
        qCWarning(lcCseDecryption()) << "end-to-end encryption is disabled";
        return {};
    }

    if (encryptionEngine.useTokenBasedEncryption()) {
        qCDebug(lcCseDecryption()) << "use certificate on hardware token" << selectedCertificate.sha256Fingerprint();
    } else {
        qCDebug(lcCseDecryption()) << "use certificate on software storage" << selectedCertificate.sha256Fingerprint();
    }

    auto decryptBase64Result = QByteArray{};
    bool needHardwareTokenEncryptionInit = false;
    for (auto i = 0; i < 2; ++i) {
        if (needHardwareTokenEncryptionInit) {
            encryptionEngine.initializeHardwareTokenEncryption(nullptr);
        }

        const auto key = selectedCertificate.getEvpPrivateKey();
        if (!key) {
            qCWarning(lcCseDecryption()) << "invalid private key handle";
            return {};
        }

        const auto decryptionResult = internals::decryptStringAsymmetric(encryptionEngine, key, paddingMode, QByteArray::fromBase64(base64Data));
        if (decryptionResult) {
            decryptBase64Result = *decryptionResult;
            break;
        } else if (decryptionResult.error() == ClientSideEncryption::EncryptionErrorType::RetryOnError) {
            qCInfo(lcCseDecryption()) << "retry decryption after error";
            needHardwareTokenEncryptionInit = true;
            continue;
        } else {
            qCWarning(lcCseDecryption()) << "decrypt failed";
            return {};
        }
    }

    if (decryptBase64Result.isEmpty()) {
        qCWarning(lcCseDecryption()) << "ERROR. Could not decrypt data";
        return {};
    }
    return decryptBase64Result;
}

QByteArray encryptStringSymmetric(const QByteArray& key, const QByteArray& data) {
    QByteArray iv = generateRandom(16);

    CipherCtx ctx;

    /* Create and initialise the context */
    if(!ctx) {
        qCInfo(lcCse()) << "Error creating cipher" << handleErrors();
        return {};
    }

    /* Initialise the decryption operation. */
    if(!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr)) {
        qCInfo(lcCse()) << "Error initializing context with aes_128" << handleErrors();
        return {};
    }

    // No padding
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
        qCInfo(lcCse()) << "Error setting iv length" << handleErrors();
        return {};
    }

    /* Initialise key and IV */
    if(!EVP_EncryptInit_ex(ctx, nullptr, nullptr, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv" << handleErrors();
        return {};
    }

    // We write the data base64 encoded
    QByteArray dataB64 = data.toBase64();

    // Make sure we have enough room in the cipher text
    QByteArray ctext(dataB64.size() + 16, '\0');

    // Do the actual encryption
    int len = 0;
    if(!EVP_EncryptUpdate(ctx, unsignedData(ctext), &len, (unsigned char *)dataB64.constData(), dataB64.size())) {
        qCInfo(lcCse()) << "Error encrypting" << handleErrors();
        return {};
    }

    int clen = len;

    /* Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if(1 != EVP_EncryptFinal_ex(ctx, unsignedData(ctext) + len, &len)) {
        qCInfo(lcCse()) << "Error finalizing encryption" << handleErrors();
        return {};
    }
    clen += len;

    /* Get the e2EeTag */
    QByteArray e2EeTag(OCC::Constants::e2EeTagSize, '\0');
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, OCC::Constants::e2EeTagSize, unsignedData(e2EeTag))) {
        qCInfo(lcCse()) << "Error getting the e2EeTag" << handleErrors();
        return {};
    }

    QByteArray cipherTXT;
    cipherTXT.reserve(clen + OCC::Constants::e2EeTagSize);
    cipherTXT.append(ctext, clen);
    cipherTXT.append(e2EeTag);

    QByteArray result = cipherTXT.toBase64();
    result += '|';
    result += iv.toBase64();

    return result;
}

namespace internals {

OCC::Result<QByteArray, OCC::ClientSideEncryption::EncryptionErrorType> decryptStringAsymmetric(ClientSideEncryption &encryptionEngine,
                                                                                                EVP_PKEY *privateKey,
                                                                                                int pad_mode,
                                                                                                const QByteArray& binaryData)
{
    const auto sslEngine = encryptionEngine.sslEngine();
    int err = -1;

    auto ctx = PKeyCtx::forKey(privateKey, sslEngine);
    if (!ctx) {
        qCInfo(lcCseDecryption()) << "Could not create the PKEY context." << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    err = EVP_PKEY_decrypt_init(ctx);
    if (err <= 0) {
        qCInfo(lcCseDecryption()) << "Could not init the decryption of the metadata" << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, pad_mode) <= 0) {
        qCInfo(lcCseDecryption()) << "Error setting the encryption padding." << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    if (pad_mode != RSA_PKCS1_PADDING && EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCseDecryption()) << "Error setting OAEP SHA 256" << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    if (pad_mode != RSA_PKCS1_PADDING && EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCseDecryption()) << "Error setting MGF1 padding" << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    size_t outlen = 0;
    err = EVP_PKEY_decrypt(ctx, nullptr, &outlen,  (unsigned char *)binaryData.constData(), binaryData.size());
    if (err <= 0) {
        qCInfo(lcCseDecryption()) << "Could not determine the buffer length" << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    QByteArray out(static_cast<int>(outlen), '\0');

    if (EVP_PKEY_decrypt(ctx, unsignedData(out), &outlen, (unsigned char *)binaryData.constData(), binaryData.size()) <= 0) {
        const auto error = handleErrors();
        if (ClientSideEncryption::checkEncryptionErrorForHardwareTokenResetState(error)) {
            return {OCC::ClientSideEncryption::EncryptionErrorType::RetryOnError};
        }
        qCCritical(lcCseDecryption()) << "Could not decrypt the data." << error;
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    // we don't need extra zeroes in out, so let's only return meaningful data
    out = QByteArray(out.constData(), outlen);
    return out.toBase64();
}

OCC::Result<QByteArray, ClientSideEncryption::EncryptionErrorType> encryptStringAsymmetric(ClientSideEncryption &encryptionEngine,
                                                                                           EVP_PKEY *publicKey,
                                                                                           int pad_mode,
                                                                                           const QByteArray& binaryData) {
    const auto sslEngine = encryptionEngine.sslEngine();
    auto ctx = PKeyCtx::forKey(publicKey, sslEngine);
    if (!ctx) {
        qCInfo(lcCseEncryption()) << "Could not initialize the pkey context." << publicKey << sslEngine;
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    if (EVP_PKEY_encrypt_init(ctx) != 1) {
        qCInfo(lcCseEncryption()) << "Error initilaizing the encryption." << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, pad_mode) <= 0) {
        qCInfo(lcCseEncryption()) << "Error setting the encryption padding." << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    if (pad_mode != RSA_PKCS1_PADDING && EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCseEncryption()) << "Error setting OAEP SHA 256" << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    if (pad_mode != RSA_PKCS1_PADDING && EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCseEncryption()) << "Error setting MGF1 padding" << handleErrors();
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    size_t outLen = 0;
    if (EVP_PKEY_encrypt(ctx, nullptr, &outLen, (unsigned char *)binaryData.constData(), binaryData.size()) != 1) {
        const auto error = handleErrors();
        if (ClientSideEncryption::checkEncryptionErrorForHardwareTokenResetState(error)) {
            encryptionEngine.initializeHardwareTokenEncryption(nullptr);
            return {OCC::ClientSideEncryption::EncryptionErrorType::RetryOnError};
        }
        qCCritical(lcCseDecryption()) << "Error retrieving the size of the encrypted data" << error;
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    QByteArray out(static_cast<int>(outLen), '\0');
    if (EVP_PKEY_encrypt(ctx, unsignedData(out), &outLen, (unsigned char *)binaryData.constData(), binaryData.size()) != 1) {
        const auto error = handleErrors();
        if (ClientSideEncryption::checkEncryptionErrorForHardwareTokenResetState(error)) {
            encryptionEngine.initializeHardwareTokenEncryption(nullptr);
            return {OCC::ClientSideEncryption::EncryptionErrorType::RetryOnError};
        }
        qCCritical(lcCseEncryption()) << "Could not encrypt key." << error;
        return {OCC::ClientSideEncryption::EncryptionErrorType::FatalError};
    }

    // Transform the encrypted data into base64.
    return out.toBase64();
}

}

void debugOpenssl()
{
    if (ERR_peek_error() == 0) {
        return;
    }

    const char *file;
    char errorMessage[255];
    int line;
    while (const auto errorNumber = ERR_get_error_line(&file, &line)) {
        ERR_error_string(errorNumber, errorMessage);
        qCWarning(lcCse()) << errorMessage << file << line;
    }
}

}


ClientSideEncryption::ClientSideEncryption()
{
}

bool ClientSideEncryption::isInitialized() const
{
    return useTokenBasedEncryption() || !getMnemonic().isEmpty();
}

QSslKey ClientSideEncryption::getPublicKey() const
{
    return _encryptionCertificate.getSslPublicKey();
}

const QByteArray &ClientSideEncryption::getPrivateKey() const
{
    return _encryptionCertificate.getPrivateKeyData();
}

void ClientSideEncryption::setPrivateKey(const QByteArray &privateKey)
{
    _encryptionCertificate.setPrivateKeyData(privateKey);
}

const CertificateInformation &ClientSideEncryption::getCertificateInformation() const
{
    Q_ASSERT( _encryptionCertificate.canDecrypt() &&  _encryptionCertificate.canDecrypt());
    return _encryptionCertificate;
}

CertificateInformation ClientSideEncryption::getCertificateInformationByFingerprint(const QByteArray &certificateFingerprint) const
{
    CertificateInformation result;

    Q_ASSERT(!certificateFingerprint.isEmpty());

    if (_encryptionCertificate.sha256Fingerprint() == certificateFingerprint) {
        result = _encryptionCertificate;
    } else {
        for(const auto &oneCertificate : _otherCertificates) {
            if (oneCertificate.sha256Fingerprint() == certificateFingerprint) {
                result = oneCertificate;
                break;
            }
        }
    }

    Q_ASSERT(result.canDecrypt() && result.canDecrypt());

    return result;
}

int ClientSideEncryption::paddingMode() const
{
    if (useTokenBasedEncryption()) {
        return RSA_PKCS1_PADDING;
    } else {
        return RSA_PKCS1_OAEP_PADDING;
    }
}

CertificateInformation ClientSideEncryption::getTokenCertificateByFingerprint(const QByteArray &expectedFingerprint) const
{
    CertificateInformation result;

    if (_encryptionCertificate.sha256Fingerprint() == expectedFingerprint) {
        result = _encryptionCertificate;
        return result;
    }

    const auto itCertificate = std::find_if(_otherCertificates.begin(), _otherCertificates.end(), [expectedFingerprint] (const auto &oneCertificate) {
        return oneCertificate.sha256Fingerprint() == expectedFingerprint;
    });
    if (itCertificate != _otherCertificates.end()) {
        result = *itCertificate;
        return result;
    }

    return result;
}

bool ClientSideEncryption::useTokenBasedEncryption() const
{
    return _encryptionCertificate.getPkcs11PrivateKey();
}

const QString &ClientSideEncryption::getMnemonic() const
{
    return _mnemonic;
}

void ClientSideEncryption::setCertificate(const QSslCertificate &certificate)
{
    _encryptionCertificate = CertificateInformation{useTokenBasedEncryption() ? CertificateInformation::CertificateType::HardwareCertificate : CertificateInformation::CertificateType::SoftwareNextcloudCertificate,
                                                    _encryptionCertificate.getPrivateKeyData(),
                                                    QSslCertificate{certificate}};
}

const QSslCertificate& ClientSideEncryption::getCertificate() const
{
    return _encryptionCertificate.getCertificate();
}

ENGINE* ClientSideEncryption::sslEngine() const
{
    return ENGINE_get_default_RSA();
}

ClientSideEncryptionTokenSelector *ClientSideEncryption::usbTokenInformation()
{
    return &_usbTokenInformation;
}

bool ClientSideEncryption::canEncrypt() const
{
    if (!isInitialized()) {
        return false;
    }
    if (useTokenBasedEncryption()) {
        return _encryptionCertificate.canEncrypt();
    }

    return true;
}

bool ClientSideEncryption::canDecrypt() const
{
    return isInitialized();
}

bool ClientSideEncryption::userCertificateNeedsMigration() const
{
    if (!isInitialized()) {
        return false;
    }
    if (useTokenBasedEncryption()) {
        return _encryptionCertificate.userCertificateNeedsMigration();
    }

    return false;
}

QByteArray ClientSideEncryption::certificateSha256Fingerprint() const
{
    return _encryptionCertificate.sha256Fingerprint();
}

void ClientSideEncryption::setAccount(const AccountPtr &account)
{
    _account = account;
}

void ClientSideEncryption::initialize(QWidget *settingsDialog)
{
    Q_ASSERT(_account);

    qCInfo(lcCse()) << "Initializing";
    if (!_account->capabilities().clientSideEncryptionAvailable()) {
        qCInfo(lcCse()) << "No Client side encryption available on server.";
        emit initializationFinished();
        return;
    }

    if (_account->enforceUseHardwareTokenEncryption()) {
        addExtraRootCertificates();
        if (_usbTokenInformation.isSetup()) {
            initializeHardwareTokenEncryption(settingsDialog);
        } else if (_account->e2eEncryptionKeysGenerationAllowed() && _account->askUserForMnemonic()) {
            Q_EMIT startingDiscoveryEncryptionUsbToken();
            auto futureTokenDiscoveryResult = new QFutureWatcher<void>(this);
            auto tokenDiscoveryResult = _usbTokenInformation.searchForCertificates(_account);
            futureTokenDiscoveryResult->setFuture(tokenDiscoveryResult);
            connect(futureTokenDiscoveryResult, &QFutureWatcher<void>::finished,
                    this, [this, settingsDialog, futureTokenDiscoveryResult] () {
                completeHardwareTokenInitialization(settingsDialog);
                futureTokenDiscoveryResult->deleteLater();
                Q_EMIT finishedDiscoveryEncryptionUsbToken();
            });
        } else {
            emit initializationFinished();
        }
    } else {
        fetchCertificateFromKeyChain();
    }
}

void ClientSideEncryption::addExtraRootCertificates()
{
#if defined(Q_OS_WIN)
    auto sslConfig = QSslConfiguration::defaultConfiguration();

    for (const auto &storeName : std::vector<std::wstring>{L"CA"}) {
        auto systemStore = CertOpenSystemStore(0, storeName.data());
        if (systemStore) {
            auto certificatePointer = PCCERT_CONTEXT{nullptr};
            while (true) {
                certificatePointer = CertFindCertificateInStore(systemStore, X509_ASN_ENCODING, 0, CERT_FIND_ANY, nullptr, certificatePointer);
                if (!certificatePointer) {
                    break;
                }
                const auto der = QByteArray{reinterpret_cast<const char *>(certificatePointer->pbCertEncoded),
                                            static_cast<int>(certificatePointer->cbCertEncoded)};
                const auto cert = QSslCertificate{der, QSsl::Der};

                qCDebug(lcCse()) << "found certificate" << cert.subjectDisplayName() << cert.issuerDisplayName() << "from store" << storeName;

                sslConfig.addCaCertificate(cert);
            }
            CertCloseStore(systemStore, 0);
        }
    }

    QSslConfiguration::setDefaultConfiguration(sslConfig);
#endif

    qCDebug(lcCse()) << "existing CA certificates";
    const auto currentSslConfig = QSslConfiguration::defaultConfiguration();
    const auto &caCertificates = currentSslConfig.caCertificates();
    for (const auto &oneCaCertificate : caCertificates) {
        qCDebug(lcCse()) << oneCaCertificate.subjectDisplayName() << oneCaCertificate.issuerDisplayName();
    }
}

void ClientSideEncryption::initializeHardwareTokenEncryption(QWidget *settingsDialog)
{
    auto ctx = Pkcs11Context{Pkcs11Context::State::CreateContext};
    _tokenSlots.reset();
    _encryptionCertificate.clear();
    _otherCertificates.clear();
    _context.clear();

    if (PKCS11_CTX_load(ctx, _account->encryptionHardwareTokenDriverPath().toLatin1().constData())) {
        qCWarning(lcCse()) << "loading pkcs11 engine failed:" << ERR_reason_error_string(ERR_get_error());

        failedToInitialize();
        return;
    }

    auto tokensCount = 0u;
    PKCS11_SLOT *tempTokenSlots = nullptr;
    /* get information on all slots */
    if (PKCS11_enumerate_slots(ctx, &tempTokenSlots, &tokensCount) < 0) {
        qCWarning(lcCse()) << "no slots available" << ERR_reason_error_string(ERR_get_error());

        failedToInitialize();
        return;
    }

    auto deleter = [ctx = static_cast<PKCS11_CTX*>(ctx), tokensCount] (PKCS11_SLOT* pointer) noexcept -> void {
        qCWarning(lcCse()) << "destructor" << pointer << ctx;
        if (pointer) {
            qCWarning(lcCse()) << "destructor" << pointer << ctx;
            PKCS11_release_all_slots(ctx, pointer, tokensCount);
        }
    };

    auto tokenSlots = decltype(_tokenSlots){tempTokenSlots, deleter};

    auto currentSlot = static_cast<PKCS11_SLOT*>(nullptr);
    for(auto i = 0u; i < tokensCount; ++i) {
        currentSlot = PKCS11_find_next_token(ctx, tokenSlots.get(), tokensCount, currentSlot);
        if (currentSlot == nullptr || currentSlot->token == nullptr) {
            break;
        }

        qCDebug(lcCse()) << "Slot manufacturer......:" << currentSlot->manufacturer;
        qCDebug(lcCse()) << "Slot description.......:" << currentSlot->description;
        qCDebug(lcCse()) << "Slot token label.......:" << currentSlot->token->label;
        qCDebug(lcCse()) << "Slot token manufacturer:" << currentSlot->token->manufacturer;
        qCDebug(lcCse()) << "Slot token model.......:" << currentSlot->token->model;
        qCDebug(lcCse()) << "Slot token serialnr....:" << currentSlot->token->serialnr;

        if (PKCS11_open_session(currentSlot, 0) != 0) {
            qCWarning(lcCse()) << "PKCS11_open_session failed" << ERR_reason_error_string(ERR_get_error());

            failedToInitialize();
            return;
        }

        auto logged_in = 0;
        if (PKCS11_is_logged_in(currentSlot, 0, &logged_in) != 0) {
            qCWarning(lcCse()) << "PKCS11_is_logged_in failed" << ERR_reason_error_string(ERR_get_error());

            failedToInitialize();
            return;
        }

        while (true) {
            auto pinHasToBeCached = false;
            auto newPin = _cachedPin;

            if (newPin.isEmpty()) {
                /* perform pkcs #11 login */
                bool ok;
                newPin = QInputDialog::getText(settingsDialog,
                                               tr("Input PIN code", "Please keep it short and shorter than \"Enter Certificate USB Token PIN:\""),
                                               tr("Enter Certificate USB Token PIN:"),
                                               QLineEdit::Password,
                                               {},
                                               &ok,
                                               Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
                if (!ok || newPin.isEmpty()) {
                    qCWarning(lcCse()) << "an USER pin is required";

                    Q_EMIT initializationFinished();
                    return;
                }

                pinHasToBeCached = true;
            }

            const auto newPinData = newPin.toLatin1();
            if (PKCS11_login(currentSlot, 0, newPinData.data()) != 0) {
                QMessageBox::warning(settingsDialog,
                                     tr("Invalid PIN. Login failed"),
                                     tr("Login to the token failed after providing the user PIN. It may be invalid or wrong. Please try again!"),
                                     QMessageBox::Ok);
                _cachedPin.clear();
                continue;
            }

            /* check if user is logged in */
            if (PKCS11_is_logged_in(currentSlot, 0, &logged_in) != 0) {
                qCWarning(lcCse()) << "PKCS11_is_logged_in failed" << ERR_reason_error_string(ERR_get_error());

                _cachedPin.clear();
                failedToInitialize();
                return;
            }
            if (!logged_in) {
                qCWarning(lcCse()) << "PKCS11_is_logged_in says user is not logged in, expected to be logged in";

                _cachedPin.clear();
                failedToInitialize();
                return;
            }

            if (pinHasToBeCached) {
                cacheTokenPin(newPin);
            }

            break;
        }

        auto keysCount = 0u;
        auto certificatesFromToken = static_cast<PKCS11_CERT*>(nullptr);
        if (PKCS11_enumerate_certs(currentSlot->token, &certificatesFromToken, &keysCount)) {
            qCWarning(lcCse()) << "PKCS11_enumerate_certs failed" << ERR_reason_error_string(ERR_get_error());

            failedToInitialize();
            return;
        }

        for (auto certificateIndex = 0u; certificateIndex < keysCount; ++certificateIndex) {
            const auto currentCertificate = &certificatesFromToken[certificateIndex];

            Bio out;
            const auto ret = PEM_write_bio_X509(out, currentCertificate->x509);
            if (ret <= 0){
                qCWarning(lcCse()) << "PEM_write_bio_X509 failed" << ERR_reason_error_string(ERR_get_error());

                failedToInitialize();
                return;
            }

            const auto result = BIO2ByteArray(out);
            auto sslCertificate = QSslCertificate{result, QSsl::Pem};

            if (sslCertificate.isSelfSigned()) {
                qCDebug(lcCse()) << "newly found certificate is self signed: goint to ignore it";
                continue;
            }

            const auto certificateKey = PKCS11_find_key(currentCertificate);
            if (!certificateKey) {
                qCWarning(lcCse()) << "PKCS11_find_key failed" << ERR_reason_error_string(ERR_get_error());

                failedToInitialize();
                return;
            }

            qCDebug(lcCse) << "checking the type of the key associated to the certificate" << sslCertificate.digest(QCryptographicHash::Sha256).toBase64();
            qCDebug(lcCse) << "key type" << Qt::hex << PKCS11_get_key_type(certificateKey) << certificateKey;

            auto newCertificateInformation = CertificateInformation{currentCertificate, std::move(sslCertificate)};

            if (newCertificateInformation.isSelfSigned()) {
                qCDebug(lcCse()) << "newly found certificate is self signed: goint to ignore it";
                continue;
            }

            const auto &sslErrors = newCertificateInformation.verify();
            for (const auto &sslError : sslErrors) {
                qCInfo(lcCse()) << "certificate validation error" << sslError;
            }

            _otherCertificates.push_back(std::move(newCertificateInformation));
        }
    }

    for (const auto &oneCertificateInformation : _otherCertificates) {
        if (!_usbTokenInformation.sha256Fingerprint().isEmpty() && oneCertificateInformation.sha256Fingerprint() != _usbTokenInformation.sha256Fingerprint()) {
            qCDebug(lcCse()) << "skipping certificate from" << "with fingerprint" << oneCertificateInformation.sha256Fingerprint() << "different from" << _usbTokenInformation.sha256Fingerprint();
            continue;
        }

        setEncryptionCertificate(oneCertificateInformation);

        if (oneCertificateInformation.canEncrypt() && !checkEncryptionIsWorking(oneCertificateInformation)) {
            qCWarning(lcCse()) << "encryption is not properly setup";

            failedToInitialize();
            return;
        }

        sendPublicKey();

        _tokenSlots = std::move(tokenSlots);
        _context = std::move(ctx);

        return;
    }

    failedToInitialize();
}

void ClientSideEncryption::fetchCertificateFromKeyChain()
{
    const QString kck = AbstractCredentials::keychainKey(
        _account->url().toString(),
        _account->credentials()->user() + e2e_cert,
        _account->id()
        );

    const auto job = new ReadPasswordJob(Theme::instance()->appName());
    job->setProperty(accountProperty, QVariant::fromValue(_account));
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::publicCertificateFetched);
    job->start();
}

void ClientSideEncryption::fetchCertificateFromKeyChain(const QString &userId)
{
    const auto keyChainKey = AbstractCredentials::keychainKey(_account->url().toString(), userId + e2e_cert + e2e_cert_sharing, userId);

    const auto job = new ReadPasswordJob(Theme::instance()->appName());
    job->setProperty(accountProperty, QVariant::fromValue(_account));
    job->setInsecureFallback(false);
    job->setKey(keyChainKey);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::publicKeyFetchedForUserId);
    job->start();
}

void ClientSideEncryption::fetchPublicKeyFromKeyChain()
{
    const QString kck = AbstractCredentials::keychainKey(
        _account->url().toString(),
        _account->credentials()->user() + e2e_public,
        _account->id()
        );

    auto *job = new ReadPasswordJob(Theme::instance()->appName());
    job->setProperty(accountProperty, QVariant::fromValue(_account));
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::publicKeyFetched);
    job->start();
}

bool ClientSideEncryption::checkEncryptionIsWorking(const CertificateInformation &currentCertificate)
{
    qCInfo(lcCse) << "check encryption is working before enabling end-to-end encryption feature";
    QByteArray data = EncryptionHelper::generateRandom(64);

    auto encryptedData = EncryptionHelper::encryptStringAsymmetric(currentCertificate, paddingMode(), *this, data);
    if (!encryptedData) {
        qCWarning(lcCse()) << "encryption error";
        return false;
    }

    qCDebug(lcCse) << "encryption is working with" << currentCertificate.sha256Fingerprint();

    const auto decryptionResult = EncryptionHelper::decryptStringAsymmetric(currentCertificate, paddingMode(), *this, *encryptedData);
    if (!decryptionResult) {
        qCWarning(lcCse()) << "encryption error";
        return false;
    }

    qCDebug(lcCse) << "decryption is working with" << currentCertificate.sha256Fingerprint();

    QByteArray decryptResult = QByteArray::fromBase64(*decryptionResult);

    if (data != decryptResult) {
        qCInfo(lcCse()) << "recovered data does not match the initial data after encryption and decryption of it";
        return false;
    }

    qCInfo(lcCse) << "end-to-end encryption is working with" << currentCertificate.sha256Fingerprint();

    return true;
}

bool ClientSideEncryption::checkServerPublicKeyValidity(const QByteArray &serverPublicKeyString) const
{
    Bio serverPublicKeyBio;
    BIO_write(serverPublicKeyBio, serverPublicKeyString.constData(), serverPublicKeyString.size());
    const auto serverPublicKey = PKey::readPrivateKey(serverPublicKeyBio);

    Bio certificateBio;
    const auto certificatePem = _encryptionCertificate.getCertificate().toPem();
    BIO_write(certificateBio, certificatePem.constData(), certificatePem.size());
    const auto x509Certificate = X509Certificate::readCertificate(certificateBio);
    if (!x509Certificate) {
        qCInfo(lcCse()) << "Client certificate is invalid. Could not check it against the server public key";
        return false;
    }

    if (X509_verify(x509Certificate, serverPublicKey) == 0) {
        qCInfo(lcCse()) << "Client certificate is not valid against the server public key";
        return false;
    }

    qCDebug(lcCse()) << "Client certificate is valid against server public key";
    return true;
}

void ClientSideEncryption::publicCertificateFetched(Job *incoming)
{
    auto *readJob = dynamic_cast<ReadPasswordJob *>(incoming);
    auto account = readJob->property(accountProperty).value<AccountPtr>();
    Q_ASSERT(account);

    // Error or no valid public key error out
    if (readJob->error() != NoError || readJob->binaryData().length() == 0) {
        fetchPublicKeyFromKeyChain();
        return;
    }

    _encryptionCertificate = CertificateInformation{useTokenBasedEncryption() ? CertificateInformation::CertificateType::HardwareCertificate : CertificateInformation::CertificateType::SoftwareNextcloudCertificate,
                                                    _encryptionCertificate.getPrivateKeyData(),
                                                    QSslCertificate{readJob->binaryData(), QSsl::Pem}};

    if (_encryptionCertificate.getCertificate().isNull()) {
        fetchPublicKeyFromKeyChain();
        return;
    }

    qCInfo(lcCse()) << "Public key fetched from keychain";

    const QString kck = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_private,
        account->id()
        );

    auto *job = new ReadPasswordJob(Theme::instance()->appName());
    job->setProperty(accountProperty, QVariant::fromValue(account));
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::privateKeyFetched);
    job->start();
}

QByteArray ClientSideEncryption::generateSignatureCryptographicMessageSyntax(const QByteArray &data) const
{
    Bio certificateBio;
    const auto certificatePem = _encryptionCertificate.getCertificate().toPem();
    BIO_write(certificateBio, certificatePem.constData(), certificatePem.size());
    const auto x509Certificate = X509Certificate::readCertificate(certificateBio);
    if (!x509Certificate) {
        qCInfo(lcCse()) << "Client certificate is invalid. Could not check it against the server public key";
        return {};
    }

    const auto privateKey = _encryptionCertificate.getEvpPrivateKey();

    Bio dataBio;
    BIO_write(dataBio, data.constData(), data.size());

    const auto contentInfo = CMS_sign(x509Certificate, privateKey, nullptr, dataBio, CMS_DETACHED);

    Q_ASSERT(contentInfo);
    if (!contentInfo) {
        return {};
    }

    Bio i2dCmsBioOut;
    [[maybe_unused]] auto resultI2dCms = i2d_CMS_bio(i2dCmsBioOut, contentInfo);
    const auto i2dCmsBio = BIO2ByteArray(i2dCmsBioOut);

    CMS_ContentInfo_free(contentInfo);

    return i2dCmsBio;
}

bool ClientSideEncryption::verifySignatureCryptographicMessageSyntax(const QByteArray &cmsContent, const QByteArray &data, const QVector<QByteArray> &certificatePems) const
{
    Bio cmsContentBio;
    BIO_write(cmsContentBio, cmsContent.constData(), cmsContent.size());
    const auto cmsDataFromBio = d2i_CMS_bio(cmsContentBio, nullptr);
    if (!cmsDataFromBio) {
        return false;
    }

    Bio detachedData;
    BIO_write(detachedData, data.constData(), data.size());

    if (CMS_verify(cmsDataFromBio, nullptr, nullptr, detachedData, nullptr, CMS_DETACHED | CMS_NO_SIGNER_CERT_VERIFY) != 1) {
        CMS_ContentInfo_free(cmsDataFromBio);
        return false;
    }

    const auto signerInfos = CMS_get0_SignerInfos(cmsDataFromBio);

    if (!signerInfos) {
        CMS_ContentInfo_free(cmsDataFromBio);
        return false;
    }

    const auto numSignerInfos = sk_CMS_SignerInfo_num(signerInfos);

    for (const auto &certificatePem : certificatePems) {
        Bio certificateBio;
        BIO_write(certificateBio, certificatePem.constData(), certificatePem.size());
        const auto x509Certificate = X509Certificate::readCertificate(certificateBio);

        if (!x509Certificate) {
            continue;
        }

        for (auto i = 0; i < numSignerInfos; ++i) {
            const auto signerInfo = sk_CMS_SignerInfo_value(signerInfos, i);
            if (CMS_SignerInfo_cert_cmp(signerInfo, x509Certificate) == 0) {
                CMS_ContentInfo_free(cmsDataFromBio);
                return true;
            }
        }
    }
    CMS_ContentInfo_free(cmsDataFromBio);
    return false;
}

void ClientSideEncryption::publicKeyFetched(QKeychain::Job *incoming)
{
    const auto readJob = dynamic_cast<ReadPasswordJob *>(incoming);
    const auto account = readJob->property(accountProperty).value<AccountPtr>();
    Q_ASSERT(account);

           // Error or no valid public key error out
    if (readJob->error() != NoError || readJob->binaryData().length() == 0) {
        getPublicKeyFromServer();
        return;
    }

    const auto publicKey =  QSslKey(readJob->binaryData(), QSsl::Rsa, QSsl::Pem, QSsl::PublicKey);

    if (publicKey.isNull()) {
        getPublicKeyFromServer();
        return;
    }

    Q_UNUSED(publicKey)

    const QString kck = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_private,
        account->id()
        );

    const auto job = new ReadPasswordJob(Theme::instance()->appName());
    job->setProperty(accountProperty, QVariant::fromValue(account));
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::privateKeyFetched);
    job->start();
}

void ClientSideEncryption::publicKeyFetchedForUserId(QKeychain::Job *incoming)
{
    const auto readJob = dynamic_cast<ReadPasswordJob *>(incoming);
    Q_ASSERT(readJob);

    if (readJob->error() != NoError || readJob->binaryData().isEmpty()) {
        emit certificateFetchedFromKeychain(QSslCertificate{});
        return;
    }
    emit certificateFetchedFromKeychain(QSslCertificate(readJob->binaryData(), QSsl::Pem));
}

void ClientSideEncryption::privateKeyFetched(Job *incoming)
{
    auto *readJob = dynamic_cast<ReadPasswordJob *>(incoming);
    auto account = readJob->property(accountProperty).value<AccountPtr>();
    Q_ASSERT(account);

    // Error or no valid public key error out
    if (readJob->error() != NoError || readJob->binaryData().length() == 0) {
        forgetSensitiveData();
        getPublicKeyFromServer();
        return;
    }

    _encryptionCertificate.setPrivateKeyData(readJob->binaryData());

    if (getPrivateKey().isNull()) {
        getPrivateKeyFromServer();
        return;
    }

    qCInfo(lcCse()) << "Private key fetched from keychain";

    const QString kck = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_mnemonic,
        account->id()
        );

    auto *job = new ReadPasswordJob(Theme::instance()->appName());
    job->setProperty(accountProperty, QVariant::fromValue(account));
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::mnemonicKeyFetched);
    job->start();
}

void ClientSideEncryption::mnemonicKeyFetched(QKeychain::Job *incoming)
{
    auto *readJob = dynamic_cast<ReadPasswordJob *>(incoming);
    auto account = readJob->property(accountProperty).value<AccountPtr>();
    Q_ASSERT(account);

    // Error or no valid public key error out
    if (readJob->error() != NoError || readJob->textData().length() == 0) {
        forgetSensitiveData();
        getPublicKeyFromServer();
        return;
    }

    setMnemonic(readJob->textData());

    qCInfo(lcCse()) << "Mnemonic key fetched from keychain";

    checkServerHasSavedKeys();
}

void ClientSideEncryption::writePrivateKey()
{
    const QString kck = AbstractCredentials::keychainKey(
        _account->url().toString(),
        _account->credentials()->user() + e2e_private,
        _account->id()
        );

    auto *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setBinaryData(getPrivateKey());
    connect(job, &WritePasswordJob::finished, job, [](Job *incoming) {
        Q_UNUSED(incoming);
        qCInfo(lcCse()) << "Private key stored in keychain";
    });
    job->start();
}

void ClientSideEncryption::writeCertificate()
{
    const QString kck = AbstractCredentials::keychainKey(
        _account->url().toString(),
        _account->credentials()->user() + e2e_cert,
        _account->id()
        );

    auto *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setBinaryData(_encryptionCertificate.getCertificate().toPem());
    connect(job, &WritePasswordJob::finished, job, [](Job *incoming) {
        Q_UNUSED(incoming);
        qCInfo(lcCse()) << "Certificate stored in keychain";
    });
    job->start();
}

void ClientSideEncryption::writeCertificate(const QString &userId, const QSslCertificate &certificate)
{
    const auto keyChainKey = AbstractCredentials::keychainKey(_account->url().toString(), userId + e2e_cert + e2e_cert_sharing, userId);

    const auto job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(keyChainKey);
    job->setBinaryData(certificate.toPem());
    connect(job, &WritePasswordJob::finished, job, [this, certificate](Job *incoming) {
        Q_UNUSED(incoming);
        qCInfo(lcCse()) << "Certificate stored in keychain";
        emit certificateWriteComplete(certificate);
    });
    job->start();
}

void ClientSideEncryption::completeHardwareTokenInitialization(QWidget *settingsDialog)
{
    if (_usbTokenInformation.isSetup()) {
        initializeHardwareTokenEncryption(settingsDialog);
    } else {
        emit initializationFinished();
    }
}

void ClientSideEncryption::setMnemonic(const QString &mnemonic)
{
    if (_mnemonic == mnemonic) {
        return;
    }

    _mnemonic = mnemonic;

    Q_EMIT canEncryptChanged();
    Q_EMIT canDecryptChanged();
}

void ClientSideEncryption::setEncryptionCertificate(CertificateInformation certificateInfo)
{
    if (_encryptionCertificate == certificateInfo) {
        return;
    }

    const auto oldValueForUserCertificateNeedsMigration = _encryptionCertificate.userCertificateNeedsMigration();

    _encryptionCertificate = std::move(certificateInfo);

    Q_EMIT canEncryptChanged();
    Q_EMIT canDecryptChanged();

    if (oldValueForUserCertificateNeedsMigration != _encryptionCertificate.userCertificateNeedsMigration()) {
        Q_EMIT userCertificateNeedsMigrationChanged();
    }
}

void ClientSideEncryption::generateMnemonic()
{
    const auto list = WordList::getRandomWords(12);
    setMnemonic(list.join(' '));
}

template <typename L>
void ClientSideEncryption::writeMnemonic(L nextCall)
{
    const QString kck = AbstractCredentials::keychainKey(
        _account->url().toString(),
        _account->credentials()->user() + e2e_mnemonic,
        _account->id()
        );

    auto *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setTextData(_mnemonic);
    connect(job, &WritePasswordJob::finished, [this, nextCall = std::move(nextCall)](Job *incoming) mutable {
        if (incoming->error() != Error::NoError) {
            failedToInitialize();
            return;
        }

        nextCall();
    });
    job->start();
}

void ClientSideEncryption::forgetSensitiveData()
{
    if (!sensitiveDataRemaining()) {
        checkAllSensitiveDataDeleted();
        return;
    }

    const auto createDeleteJob = [this](const QString user) {
        auto *job = new DeletePasswordJob(Theme::instance()->appName());
        job->setInsecureFallback(false);
        job->setKey(AbstractCredentials::keychainKey(_account->url().toString(), user, _account->id()));
        return job;
    };

    if (!_account->credentials()) {
        return;
    }

    const auto user = _account->credentials()->user();
    const auto deletePrivateKeyJob = createDeleteJob(user + e2e_private);
    const auto deleteCertJob = createDeleteJob(user + e2e_cert);
    const auto deleteMnemonicJob = createDeleteJob(user + e2e_mnemonic);

    connect(deletePrivateKeyJob, &DeletePasswordJob::finished, this, &ClientSideEncryption::handlePrivateKeyDeleted);
    connect(deleteCertJob, &DeletePasswordJob::finished, this, &ClientSideEncryption::handleCertificateDeleted);
    connect(deleteMnemonicJob, &DeletePasswordJob::finished, this, &ClientSideEncryption::handleMnemonicDeleted);
    deletePrivateKeyJob->start();
    deleteCertJob->start();
    deleteMnemonicJob->start();
    _usbTokenInformation.setSha256Fingerprint({});
    _account->setEncryptionCertificateFingerprint({});
    _tokenSlots.reset();
    _encryptionCertificate.clear();
    _otherCertificates.clear();
    _context.clear();
    Q_EMIT canDecryptChanged();
    Q_EMIT canEncryptChanged();
    Q_EMIT userCertificateNeedsMigrationChanged();
}

void ClientSideEncryption::getUsersPublicKeyFromServer(const QStringList &userIds)
{
    qCInfo(lcCse()) << "Retrieving public keys from server, for users:" << userIds;
    const auto job = new JsonApiJob(_account, e2eeBaseUrl(_account) + QStringLiteral("public-key"), this);
    connect(job, &JsonApiJob::jsonReceived, job, [this, userIds](const QJsonDocument &doc, int retCode) {
        if (retCode == 200) {
            QHash<QString, NextcloudSslCertificate> results;
            const auto &docObj = doc.object();
            const auto &ocsObj = docObj[QStringLiteral("ocs")].toObject();
            const auto &dataObj = ocsObj[QStringLiteral("data")].toObject();
            const auto &publicKeys = dataObj[QStringLiteral("public-keys")].toObject();
            const auto &allKeys = publicKeys.keys();
            for (const auto &userId : allKeys) {
                if (userIds.contains(userId)) {
                    results.insert(userId, QSslCertificate(publicKeys.value(userId).toString().toLocal8Bit(), QSsl::Pem));
                }
            }
            emit certificatesFetchedFromServer(results);
        } else if (retCode == 404) {
            qCInfo(lcCse()) << "No public key on the server";
            emit certificatesFetchedFromServer({});
        } else {
            qCInfo(lcCse()) << "Error while requesting public keys for users: " << retCode;
            emit certificatesFetchedFromServer({});
        }
    });
    QUrlQuery urlQuery;
    const auto userIdsJSON = QJsonDocument::fromVariant(userIds);
    urlQuery.addQueryItem(QStringLiteral("users"), userIdsJSON.toJson(QJsonDocument::Compact).toPercentEncoding());
    job->addQueryParams(urlQuery);
    job->start();
}

void ClientSideEncryption::migrateCertificate()
{
    _usbTokenInformation.clear();
}

void ClientSideEncryption::handlePrivateKeyDeleted(const QKeychain::Job* const incoming)
{
    const auto error = incoming->error();
    if (error != QKeychain::NoError && error != QKeychain::EntryNotFound) {
        qCWarning(lcCse) << "Private key could not be deleted:" << incoming->errorString();
        return;
    }

    qCDebug(lcCse) << "Private key successfully deleted from keychain. Clearing.";
    _encryptionCertificate.clear();

    Q_EMIT privateKeyDeleted();
    checkAllSensitiveDataDeleted();
}

void ClientSideEncryption::handleCertificateDeleted(const QKeychain::Job* const incoming)
{
    const auto error = incoming->error();
    if (error != QKeychain::NoError && error != QKeychain::EntryNotFound) {
        qCWarning(lcCse) << "Certificate could not be deleted:" << incoming->errorString();
        return;
    }

    qCDebug(lcCse) << "Certificate successfully deleted from keychain. Clearing.";
    _encryptionCertificate.clear();
    Q_EMIT certificateDeleted();
    checkAllSensitiveDataDeleted();
}

void ClientSideEncryption::handleMnemonicDeleted(const QKeychain::Job* const incoming)
{
    const auto error = incoming->error();
    if (error != QKeychain::NoError && error != QKeychain::EntryNotFound) {
        qCWarning(lcCse) << "Mnemonic could not be deleted:" << incoming->errorString();
        return;
    }

    qCDebug(lcCse) << "Mnemonic successfully deleted from keychain. Clearing.";
    setMnemonic({});
    Q_EMIT mnemonicDeleted();
    checkAllSensitiveDataDeleted();
}

void ClientSideEncryption::handlePublicKeyDeleted(const QKeychain::Job * const incoming)
{
    const auto error = incoming->error();
    if (error != QKeychain::NoError && error != QKeychain::EntryNotFound) {
        qCWarning(lcCse) << "Public key could not be deleted:" << incoming->errorString();
        return;
    }

    Q_EMIT publicKeyDeleted();
    checkAllSensitiveDataDeleted();
}

bool ClientSideEncryption::sensitiveDataRemaining() const
{
    return !getPrivateKey().isEmpty() || !_encryptionCertificate.getCertificate().isNull() || !_mnemonic.isEmpty() || !_usbTokenInformation.sha256Fingerprint().isEmpty() || _encryptionCertificate.sensitiveDataRemaining();
}

void ClientSideEncryption::failedToInitialize()
{
    forgetSensitiveData();
    Q_EMIT initializationFinished();
}

void ClientSideEncryption::saveCertificateIdentification() const
{
    _account->setEncryptionCertificateFingerprint(_usbTokenInformation.sha256Fingerprint());
}

void ClientSideEncryption::cacheTokenPin(const QString pin)
{
    _cachedPin = pin;
    QTimer::singleShot(86400000, this, [this] () {
        _cachedPin.clear();
    });
}

bool ClientSideEncryption::checkEncryptionErrorForHardwareTokenResetState(const QByteArray &errorString)
{
    return errorString.contains(":Device removed:") || errorString.contains(":Session handle invalid:") || errorString.contains(":Object handle invalid:");
}

void ClientSideEncryption::checkAllSensitiveDataDeleted()
{
    if (sensitiveDataRemaining()) {
        qCWarning(lcCse) << "Some sensitive data emaining:"
                       << "Private key:" << (getPrivateKey().isEmpty() ? "is empty" : "is not empty")
                         << "Certificate is null:" << (_encryptionCertificate.getCertificate().isNull() ? "true" : "false")
                         << "Mnemonic:" << (_mnemonic.isEmpty() ? "is empty" : "is not empty");
        return;
    }

    Q_EMIT sensitiveDataForgotten();
}

void ClientSideEncryption::generateKeyPair()
{
    // AES/GCM/NoPadding,
    // metadataKeys with RSA/ECB/OAEPWithSHA-256AndMGF1Padding
    qCInfo(lcCse()) << "No public key, generating a pair.";
    const int rsaKeyLen = 2048;


    // Init RSA
    PKeyCtx ctx(EVP_PKEY_RSA);

    if(EVP_PKEY_keygen_init(ctx) <= 0) {
        qCInfo(lcCse()) << "Couldn't initialize the key generator";
        failedToInitialize();
        return;
    }

    if(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, rsaKeyLen) <= 0) {
        qCInfo(lcCse()) << "Couldn't initialize the key generator bits";
        failedToInitialize();
        return;
    }

    auto localKeyPair = PKey::generate(ctx);
    if(!localKeyPair) {
        qCInfo(lcCse()) << "Could not generate the key";
        failedToInitialize();
        return;
    }

    {
        Bio privKey;
        if (PEM_write_bio_PrivateKey(privKey, localKeyPair, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
            qCWarning(lcCse()) << "Could not read private key from bio.";
            failedToInitialize();
            return;
        }

        _encryptionCertificate.setPrivateKeyData(BIO2ByteArray(privKey));
    }

    Bio privKey;
    if (PEM_write_bio_PrivateKey(privKey, localKeyPair, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
        qCInfo(lcCse()) << "Could not read private key from bio.";
        failedToInitialize();
        return;
    }

    qCDebug(lcCse()) << "Key correctly generated";

    auto csrContent = generateCSR(std::move(localKeyPair), PKey::readPrivateKey(privKey));
    writeMnemonic([keyPair = std::move(csrContent.second), csrContent = std::move(csrContent.first), this]() mutable -> void {
        writeKeyPair(std::move(keyPair), csrContent);
    });
}

std::pair<QByteArray, PKey> ClientSideEncryption::generateCSR(PKey keyPair,
                                                              PKey privateKey)
{
    auto result = QByteArray{};

    // OpenSSL expects const char.
    auto cnArray = _account->davUser().toLocal8Bit();

    auto certParams = std::map<const char *, const char*>{
        {"C", "DE"},
        {"ST", "Baden-Wuerttemberg"},
        {"L", "Stuttgart"},
        {"O","Nextcloud"},
        {"CN", cnArray.constData()}
    };

    int ret = 0;
    int nVersion = 0; // X.509 certificate requests only support version 1

    // 2. set version of x509 req
    auto x509_req = X509_REQ_new();
    auto release_on_exit_x509_req = qScopeGuard([&] {
        X509_REQ_free(x509_req);
    });

    ret = X509_REQ_set_version(x509_req, nVersion);
    if (ret != 1) {
        const auto errorCode = ERR_get_error();
        qCWarning(lcCse()) << "Error setting the version on the certificate signing request" << nVersion << ret << ERR_lib_error_string(errorCode) << ERR_reason_error_string(errorCode);
        return {result, std::move(keyPair)};
    }

    // 3. set subject of x509 req
    auto x509_name = X509_REQ_get_subject_name(x509_req);

    for(const auto& v : certParams) {
        ret = X509_NAME_add_entry_by_txt(x509_name, v.first,  MBSTRING_ASC, (const unsigned char*) v.second, -1, -1, 0);
        if (ret != 1) {
            qCWarning(lcCse()) << "Error Generating the Certificate while adding" << v.first << v.second;
            return {result, std::move(keyPair)};
        }
    }

    ret = X509_REQ_set_pubkey(x509_req, keyPair);
    if (ret != 1){
        qCWarning(lcCse()) << "Error setting the public key on the csr";
        return {result, std::move(keyPair)};
    }

    ret = X509_REQ_sign(x509_req, privateKey, EVP_sha256());    // return x509_req->signature->length
    if (ret <= 0){
        qCWarning(lcCse()) << "Error signing the csr with the private key";
        return {result, std::move(keyPair)};
    }

    Bio out;
    ret = PEM_write_bio_X509_REQ(out, x509_req);
    if (ret <= 0){
        qCWarning(lcCse()) << "Error exporting the csr to the BIO";
        return {result, std::move(keyPair)};
    }

    result = BIO2ByteArray(out);

    qCDebug(lcCse()) << "CSR generated";

    if (_mnemonic.isEmpty()) {
        generateMnemonic();
    }

    return {result, std::move(keyPair)};
}

void ClientSideEncryption::sendSignRequestCSR(PKey keyPair,
                                              const QByteArray &csrContent)
{
    auto job = new SignPublicKeyApiJob(_account, e2eeBaseUrl(_account) + "public-key", this);
    job->setCsr(csrContent);

    connect(job, &SignPublicKeyApiJob::jsonReceived, job, [this, keyPair = std::move(keyPair)](const QJsonDocument& json, const int retCode) {
        if (retCode == 200) {
            const auto cert = json.object().value("ocs").toObject().value("data").toObject().value("public-key").toString();
            _encryptionCertificate = CertificateInformation{useTokenBasedEncryption() ? CertificateInformation::CertificateType::HardwareCertificate : CertificateInformation::CertificateType::SoftwareNextcloudCertificate,
                                                            _encryptionCertificate.getPrivateKeyData(),
                                                            QSslCertificate{cert.toLocal8Bit(), QSsl::Pem}};
            Bio certificateBio;
            const auto certificatePem = _encryptionCertificate.getCertificate().toPem();
            BIO_write(certificateBio, certificatePem.constData(), certificatePem.size());
            const auto x509Certificate = X509Certificate::readCertificate(certificateBio);
            if (!X509_check_private_key(x509Certificate, keyPair)) {
                auto lastError = ERR_get_error();
                while (lastError) {
                    qCWarning(lcCse()) << ERR_lib_error_string(lastError);
                    lastError = ERR_get_error();
                }
                failedToInitialize();
                return;
            }
            fetchAndValidatePublicKeyFromServer();
        } else {
            qCWarning(lcCse()) << retCode;
            failedToInitialize();
            return;
        }
    });
    job->start();
}

void ClientSideEncryption::sendPublicKey()
{
    // Send public key to the server
    auto job = new StorePublicKeyApiJob(_account, e2eeBaseUrl(_account) + "public-key", this);
    job->setPublicKey(_encryptionCertificate.getCertificate().toPem());
    connect(job, &StorePublicKeyApiJob::jsonReceived, job, [this](const QJsonDocument& doc, int retCode) {
        Q_UNUSED(doc);
        switch(retCode) {
        case 200:
        case 409:
            saveCertificateIdentification();
            emit initializationFinished();

            break;
        default:
            qCWarning(lcCse) << "Store certificate failed, return code:" << retCode;
            failedToInitialize();
        }
    });
    job->start();
}

void ClientSideEncryption::writeKeyPair(PKey keyPair,
                                        const QByteArray &csrContent)
{
    const auto privateKeyKeychainId = AbstractCredentials::keychainKey(
        _account->url().toString(),
        _account->credentials()->user() + e2e_private,
        _account->id()
        );

    const auto publicKeyKeychainId = AbstractCredentials::keychainKey(
        _account->url().toString(),
        _account->credentials()->user() + e2e_public,
        _account->id()
        );

    Bio privateKey;
    if (PEM_write_bio_PrivateKey(privateKey, keyPair, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
        qCWarning(lcCse()) << "Could not read private key from bio.";
        failedToInitialize();
        return;
    }
    const auto bytearrayPrivateKey = BIO2ByteArray(privateKey);

    const auto privateKeyJob = new WritePasswordJob(Theme::instance()->appName());
    privateKeyJob->setInsecureFallback(false);
    privateKeyJob->setKey(privateKeyKeychainId);
    privateKeyJob->setBinaryData(bytearrayPrivateKey);
    connect(privateKeyJob, &WritePasswordJob::finished, privateKeyJob, [keyPair = std::move(keyPair), publicKeyKeychainId, csrContent, this] (Job *incoming) mutable {
        if (incoming->error() != Error::NoError) {
            failedToInitialize();
            return;
        }

        Bio publicKey;
        if (PEM_write_bio_PUBKEY(publicKey, keyPair) <= 0) {
            qCWarning(lcCse()) << "Could not read public key from bio.";
            failedToInitialize();
            return;
        }

        const auto bytearrayPublicKey = BIO2ByteArray(publicKey);

        const auto publicKeyJob = new WritePasswordJob(Theme::instance()->appName());
        publicKeyJob->setInsecureFallback(false);
        publicKeyJob->setKey(publicKeyKeychainId);
        publicKeyJob->setBinaryData(bytearrayPublicKey);
        connect(publicKeyJob, &WritePasswordJob::finished, publicKeyJob, [keyPair = std::move(keyPair), csrContent, this](Job *incoming) mutable {
            if (incoming->error() != Error::NoError) {
                failedToInitialize();
                return;
            }

            sendSignRequestCSR(std::move(keyPair), csrContent);
        });
        publicKeyJob->start();
    });
    privateKeyJob->start();
}

void ClientSideEncryption::checkServerHasSavedKeys()
{
    const auto keyIsNotOnServer = [this] () {
        qCInfo(lcCse) << "server is missing keys. deleting local keys";

        failedToInitialize();
    };

    const auto privateKeyOnServerIsValid = [this] () {
        Q_EMIT initializationFinished();
    };

    const auto publicKeyOnServerIsValid = [this, privateKeyOnServerIsValid, keyIsNotOnServer] () {
        checkUserPrivateKeyOnServer(privateKeyOnServerIsValid, keyIsNotOnServer);
    };

    checkUserPublicKeyOnServer(publicKeyOnServerIsValid, keyIsNotOnServer);
}

template <typename SUCCESS_CALLBACK, typename ERROR_CALLBACK>
void ClientSideEncryption::checkUserKeyOnServer(const QString &keyType,
                                                SUCCESS_CALLBACK nextCheck,
                                                ERROR_CALLBACK onError)
{
    auto job = new JsonApiJob(_account, e2eeBaseUrl(_account) + keyType, this);
    connect(job, &JsonApiJob::jsonReceived, [nextCheck, onError](const QJsonDocument& doc, int retCode) {
        Q_UNUSED(doc)

        if (retCode == 200) {
            nextCheck();
        } else {
            onError();
        }
    });
    job->start();
}

template <typename SUCCESS_CALLBACK, typename ERROR_CALLBACK>
void ClientSideEncryption::checkUserPublicKeyOnServer(SUCCESS_CALLBACK nextCheck,
                                                      ERROR_CALLBACK onError)
{
    checkUserKeyOnServer("public-key", nextCheck, onError);
}

template <typename SUCCESS_CALLBACK, typename ERROR_CALLBACK>
void ClientSideEncryption::checkUserPrivateKeyOnServer(SUCCESS_CALLBACK nextCheck, ERROR_CALLBACK onError)
{
    checkUserKeyOnServer("private-key", nextCheck, onError);
}

void ClientSideEncryption::encryptPrivateKey()
{
    if (_mnemonic.isEmpty()) {
        generateMnemonic();
    }

    auto passPhrase = _mnemonic;
    passPhrase = passPhrase.remove(' ').toLower();
    qCDebug(lcCse) << "Passphrase Generated";

    auto salt = EncryptionHelper::generateRandom(40);
    auto secretKey = EncryptionHelper::generatePassword(passPhrase, salt);
    auto cryptedText = EncryptionHelper::encryptPrivateKey(secretKey, EncryptionHelper::privateKeyToPem(getPrivateKey()), salt);

    // Send private key to the server
    auto job = new StorePrivateKeyApiJob(_account, e2eeBaseUrl(_account) + "private-key", this);
    job->setPrivateKey(cryptedText);
    connect(job, &StorePrivateKeyApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
        Q_UNUSED(doc);
        switch(retCode) {
        case 200:
            writePrivateKey();
            writeCertificate();
            writeMnemonic([this] () {
                emit initializationFinished(true);
            });
            break;
        default:
            qCWarning(lcCse) << "Store private key failed, return code:" << retCode;
            failedToInitialize();
        }
    });
    job->start();
}

void ClientSideEncryption::decryptPrivateKey(const QByteArray &key) {
    if (!_account->askUserForMnemonic()) {
        qCDebug(lcCse) << "Not allowed to ask user for mnemonic";
        failedToInitialize();
        return;
    }

    QString msg = tr("Please enter your end-to-end encryption passphrase:<br>"
                     "<br>"
                     "Username: %2<br>"
                     "Account: %3<br>")
                      .arg(Utility::escape(_account->credentials()->user()),
                           Utility::escape(_account->displayName()));

    QInputDialog dialog;
    dialog.setWindowTitle(tr("Enter E2E passphrase"));
    dialog.setLabelText(msg);
    dialog.setTextEchoMode(QLineEdit::Normal);

    QString prev;

    while(true) {
        if (!prev.isEmpty()) {
            dialog.setTextValue(prev);
        }
        bool ok = dialog.exec();
        if (ok) {
            prev = dialog.textValue();

            setMnemonic(prev);
            QString mnemonic = prev.split(" ").join(QString()).toLower();

            // split off salt
            const auto salt = EncryptionHelper::extractPrivateKeySalt(key);

            const auto deprecatedPassword = EncryptionHelper::deprecatedGeneratePassword(mnemonic, salt);
            const auto deprecatedSha1Password = EncryptionHelper::deprecatedSha1GeneratePassword(mnemonic, salt);
            const auto password = EncryptionHelper::generatePassword(mnemonic, salt);

            const auto privateKey = EncryptionHelper::decryptPrivateKey(password, key);
            if (!privateKey.isEmpty()) {
                _encryptionCertificate.setPrivateKeyData(privateKey);
            } else {
                const auto deprecatedSha1PrivateKey = EncryptionHelper::decryptPrivateKey(deprecatedSha1Password, key);
                if (!deprecatedSha1PrivateKey.isEmpty()) {
                    _encryptionCertificate.setPrivateKeyData(deprecatedSha1PrivateKey);
                } else {
                    _encryptionCertificate.setPrivateKeyData(EncryptionHelper::decryptPrivateKey(deprecatedPassword, key));
                }
            }

            if (!getPrivateKey().isNull() && checkEncryptionIsWorking(_encryptionCertificate)) {
                writePrivateKey();
                writeCertificate();
                writeMnemonic([] () {});
                break;
            }
        } else {
            qCDebug(lcCse()) << "Cancelled";
            failedToInitialize();
            return;
        }
    }

    emit initializationFinished();
}

void ClientSideEncryption::getPrivateKeyFromServer()
{
    auto job = new JsonApiJob(_account, e2eeBaseUrl(_account) + "private-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
        if (retCode == 200) {
            QString key = doc.object()["ocs"].toObject()["data"].toObject()["private-key"].toString();
            decryptPrivateKey(key.toLocal8Bit());
        } else if (retCode == 404) {
            qCWarning(lcCse) << "No private key on the server: setup is incomplete.";
            emit initializationFinished();
            return;
        } else {
            qCWarning(lcCse) << "Error while requesting public key: " << retCode;
            emit initializationFinished();
            return;
        }
    });
    job->start();
}

void ClientSideEncryption::getPublicKeyFromServer()
{
    auto job = new JsonApiJob(_account, e2eeBaseUrl(_account) + "public-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
        if (retCode == 200) {
            QString publicKey = doc.object()["ocs"].toObject()["data"].toObject()["public-keys"].toObject()[_account->davUser()].toString();
            _encryptionCertificate = CertificateInformation{useTokenBasedEncryption() ? CertificateInformation::CertificateType::HardwareCertificate : CertificateInformation::CertificateType::SoftwareNextcloudCertificate,
                                                            _encryptionCertificate.getPrivateKeyData(),
                                                            QSslCertificate{publicKey.toLocal8Bit(), QSsl::Pem}};
            fetchAndValidatePublicKeyFromServer();
        } else if (retCode == 404) {
            qCDebug(lcCse()) << "No public key on the server";
            if (!_account->e2eEncryptionKeysGenerationAllowed()) {
                qCDebug(lcCse()) << "User did not allow E2E keys generation.";
                failedToInitialize();
                return;
            }
            generateKeyPair();
        } else {
            qCWarning(lcCse) << "Error while requesting public key: " << retCode;
            failedToInitialize();
        }
    });
    job->start();
}

void ClientSideEncryption::fetchAndValidatePublicKeyFromServer()
{
    auto job = new JsonApiJob(_account, e2eeBaseUrl(_account) + "server-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
        if (retCode == 200) {
            const auto serverPublicKey = doc.object()["ocs"].toObject()["data"].toObject()["public-key"].toString().toLatin1();
            if (checkServerPublicKeyValidity(serverPublicKey)) {
                if (getPrivateKey().isEmpty()) {
                    getPrivateKeyFromServer();
                } else {
                    encryptPrivateKey();
                }
            } else {
                qCWarning(lcCse) << "Error invalid server public key";
                forgetSensitiveData();
                getPublicKeyFromServer();
                return;
            }
        } else {
            qCWarning(lcCse) << "Error while requesting server public key: " << retCode;
            failedToInitialize();
            return;
        }
    });
    job->start();
}

bool EncryptionHelper::fileEncryption(const QByteArray &key, const QByteArray &iv, QFile *input, QFile *output, QByteArray& returnTag)
{
    if (!input->open(QIODevice::ReadOnly)) {
        qCWarning(lcCse) << "Could not open input file for reading" << input->errorString();
    }
    if (!output->open(QIODevice::WriteOnly)) {
        qCWarning(lcCse) << "Could not oppen output file for writing" << output->errorString();
    }

    // Init
    CipherCtx ctx;

    /* Create and initialise the context */
    if(!ctx) {
        qCInfo(lcCse()) << "Could not create context";
        return false;
    }

    /* Initialise the decryption operation. */
    if(!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr)) {
        qCInfo(lcCse()) << "Could not init cipher";
        return false;
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
        qCInfo(lcCse()) << "Could not set iv length";
        return false;
    }

    /* Initialise key and IV */
    if(!EVP_EncryptInit_ex(ctx, nullptr, nullptr, (const unsigned char *)key.constData(), (const unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Could not set key and iv";
        return false;
    }

    QByteArray out(blockSize + OCC::Constants::e2EeTagSize - 1, '\0');
    int len = 0;

    qCDebug(lcCse) << "Starting to encrypt the file" << input->fileName() << input->atEnd();
    while(!input->atEnd()) {
        const auto data = input->read(blockSize);

        if (data.size() == 0) {
            qCInfo(lcCse()) << "Could not read data from file";
            return false;
        }

        if(!EVP_EncryptUpdate(ctx, unsignedData(out), &len, (unsigned char *)data.constData(), data.size())) {
            qCInfo(lcCse()) << "Could not encrypt";
            return false;
        }

        output->write(out, len);
    }

    if(1 != EVP_EncryptFinal_ex(ctx, unsignedData(out), &len)) {
        qCInfo(lcCse()) << "Could finalize encryption";
        return false;
    }
    output->write(out, len);

    /* Get the e2EeTag */
    QByteArray e2EeTag(OCC::Constants::e2EeTagSize, '\0');
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, OCC::Constants::e2EeTagSize, unsignedData(e2EeTag))) {
        qCInfo(lcCse()) << "Could not get e2EeTag";
        return false;
    }

    returnTag = e2EeTag;
    output->write(e2EeTag, OCC::Constants::e2EeTagSize);

    input->close();
    output->close();
    qCDebug(lcCse) << "File Encrypted Successfully";
    return true;
}

bool EncryptionHelper::fileDecryption(const QByteArray &key, const QByteArray& iv,
                                      QFile *input, QFile *output)
{
    input->open(QIODevice::ReadOnly);
    output->open(QIODevice::WriteOnly);

    // Init
    CipherCtx ctx;

    /* Create and initialise the context */
    if(!ctx) {
        qCInfo(lcCse()) << "Could not create context";
        return false;
    }

    /* Initialise the decryption operation. */
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr)) {
        qCInfo(lcCse()) << "Could not init cipher";
        return false;
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,  iv.size(), nullptr)) {
        qCInfo(lcCse()) << "Could not set iv length";
        return false;
    }

    /* Initialise key and IV */
    if(!EVP_DecryptInit_ex(ctx, nullptr, nullptr, (const unsigned char *) key.constData(), (const unsigned char *) iv.constData())) {
        qCInfo(lcCse()) << "Could not set key and iv";
        return false;
    }

    qint64 size = input->size() - OCC::Constants::e2EeTagSize;

    QByteArray out(blockSize + OCC::Constants::e2EeTagSize - 1, '\0');
    int len = 0;

    while(input->pos() < size) {

        auto toRead = size - input->pos();
        if (toRead > blockSize) {
            toRead = blockSize;
        }

        QByteArray data = input->read(toRead);

        if (data.size() == 0) {
            qCInfo(lcCse()) << "Could not read data from file";
            return false;
        }

        if(!EVP_DecryptUpdate(ctx, unsignedData(out), &len, (unsigned char *)data.constData(), data.size())) {
            qCInfo(lcCse()) << "Could not decrypt";
            return false;
        }

        output->write(out, len);
    }

    const QByteArray e2EeTag = input->read(OCC::Constants::e2EeTagSize);

    /* Set expected e2EeTag value. Works in OpenSSL 1.0.1d and later */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, e2EeTag.size(), (unsigned char *)e2EeTag.constData())) {
        qCInfo(lcCse()) << "Could not set expected e2EeTag";
        return false;
    }

    if(1 != EVP_DecryptFinal_ex(ctx, unsignedData(out), &len)) {
        qCInfo(lcCse()) << "Could finalize decryption";
        return false;
    }
    output->write(out, len);

    input->close();
    output->close();
    return true;
}

bool EncryptionHelper::dataEncryption(const QByteArray &key, const QByteArray &iv, const QByteArray &input, QByteArray &output, QByteArray &returnTag)
{
    if (input.isEmpty()) {
        qCWarning(lcCse) << "Could not use empty input data";
    }

    QByteArray inputCopy = input;

    QBuffer inputBuffer(&inputCopy);
    if (!inputBuffer.open(QIODevice::ReadOnly)) {
        qCWarning(lcCse) << "Could not open input buffer for reading" << inputBuffer.errorString();
    }

    QBuffer outputBuffer(&output);
    if (!outputBuffer.open(QIODevice::WriteOnly)) {
        qCWarning(lcCse) << "Could not oppen output buffer for writing" << outputBuffer.errorString();
    }

    // Init
    CipherCtx ctx;

    /* Create and initialise the context */
    if (!ctx) {
        qCInfo(lcCse()) << "Could not create context";
        return false;
    }

    /* Initialise the decryption operation. */
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr)) {
        qCInfo(lcCse()) << "Could not init cipher";
        return false;
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
        qCInfo(lcCse()) << "Could not set iv length";
        return false;
    }

    /* Initialise key and IV */
    if (!EVP_EncryptInit_ex(ctx, nullptr, nullptr, (const unsigned char *)key.constData(), (const unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Could not set key and iv";
        return false;
    }

    QByteArray out(blockSize + OCC::Constants::e2EeTagSize - 1, '\0');
    int len = 0;

    qCDebug(lcCse) << "Starting to encrypt a buffer";

    while (!inputBuffer.atEnd()) {
        const auto data = inputBuffer.read(blockSize);

        if (data.size() == 0) {
            qCInfo(lcCse()) << "Could not read data from file";
            return false;
        }

        if (!EVP_EncryptUpdate(ctx, unsignedData(out), &len, (unsigned char *)data.constData(), data.size())) {
            qCInfo(lcCse()) << "Could not encrypt";
            return false;
        }

        outputBuffer.write(out, len);
    }

    if (1 != EVP_EncryptFinal_ex(ctx, unsignedData(out), &len)) {
        qCInfo(lcCse()) << "Could finalize encryption";
        return false;
    }
    outputBuffer.write(out, len);

    /* Get the e2EeTag */
    QByteArray e2EeTag(OCC::Constants::e2EeTagSize, '\0');
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, OCC::Constants::e2EeTagSize, unsignedData(e2EeTag))) {
        qCInfo(lcCse()) << "Could not get e2EeTag";
        return false;
    }

    returnTag = e2EeTag;
    outputBuffer.write(e2EeTag, OCC::Constants::e2EeTagSize);

    inputBuffer.close();
    outputBuffer.close();
    qCDebug(lcCse) << "Buffer Encrypted Successfully";
    return true;
}

bool EncryptionHelper::dataDecryption(const QByteArray &key, const QByteArray &iv, const QByteArray &input, QByteArray &output)
{
    if (input.isEmpty()) {
        qCWarning(lcCse) << "Could not use empty input data";
    }

    QByteArray inputCopy = input;

    QBuffer inputBuffer(&inputCopy);
    if (!inputBuffer.open(QIODevice::ReadOnly)) {
        qCWarning(lcCse) << "Could not open input buffer for reading" << inputBuffer.errorString();
    }

    QBuffer outputBuffer(&output);
    if (!outputBuffer.open(QIODevice::WriteOnly)) {
        qCWarning(lcCse) << "Could not oppen output buffer for writing" << outputBuffer.errorString();
    }

    // Init
    CipherCtx ctx;

    /* Create and initialise the context */
    if (!ctx) {
        qCWarning(lcCse()) << "Could not create context";
        return false;
    }

    /* Initialise the decryption operation. */
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr)) {
        qCWarning(lcCse()) << "Could not init cipher";
        return false;
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
        qCWarning(lcCse()) << "Could not set iv length";
        return false;
    }

    /* Initialise key and IV */
    if (!EVP_DecryptInit_ex(ctx, nullptr, nullptr, (const unsigned char *)key.constData(), (const unsigned char *)iv.constData())) {
        qCWarning(lcCse()) << "Could not set key and iv";
        return false;
    }

    qint64 size = inputBuffer.size() - OCC::Constants::e2EeTagSize;

    QByteArray out(blockSize + OCC::Constants::e2EeTagSize - 1, '\0');
    int len = 0;

    while (inputBuffer.pos() < size) {
        auto toRead = size - inputBuffer.pos();
        if (toRead > blockSize) {
            toRead = blockSize;
        }

        QByteArray data = inputBuffer.read(toRead);

        if (data.size() == 0) {
            qCWarning(lcCse()) << "Could not read data from file";
            return false;
        }

        if (!EVP_DecryptUpdate(ctx, unsignedData(out), &len, (unsigned char *)data.constData(), data.size())) {
            qCWarning(lcCse()) << "Could not decrypt";
            return false;
        }

        outputBuffer.write(out, len);
    }

    const QByteArray e2EeTag = inputBuffer.read(OCC::Constants::e2EeTagSize);

    /* Set expected e2EeTag value. Works in OpenSSL 1.0.1d and later */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, e2EeTag.size(), (unsigned char *)e2EeTag.constData())) {
        qCWarning(lcCse()) << "Could not set expected e2EeTag";
        return false;
    }

    if (1 != EVP_DecryptFinal_ex(ctx, unsignedData(out), &len)) {
        qCWarning(lcCse()) << "Could not finalize decryption";
        return false;
    }
    outputBuffer.write(out, len);

    inputBuffer.close();
    outputBuffer.close();
    return true;
}

QByteArray EncryptionHelper::gzipThenEncryptData(const QByteArray &key, const QByteArray &inputData, const QByteArray &iv, QByteArray &returnTag)
{
    QBuffer gZipBuffer;
    auto gZipCompressionDevice = KCompressionDevice(&gZipBuffer, false, KCompressionDevice::GZip);
    if (!gZipCompressionDevice.open(QIODevice::WriteOnly)) {
        return {};
    }
    const auto bytesWritten = gZipCompressionDevice.write(inputData);
    gZipCompressionDevice.close();
    if (bytesWritten < 0) {
        return {};
    }

    if (!gZipBuffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    QByteArray outputData;
    returnTag.clear();
    const auto gZippedAndNotEncrypted = gZipBuffer.readAll();
    EncryptionHelper::dataEncryption(key, iv, gZippedAndNotEncrypted, outputData, returnTag);
    gZipBuffer.close();
    return outputData;
}

QByteArray EncryptionHelper::decryptThenUnGzipData(const QByteArray &key, const QByteArray &inputData, const QByteArray &iv)
{
    QByteArray decryptedAndUnGzipped;
    if (!EncryptionHelper::dataDecryption(key, iv, inputData, decryptedAndUnGzipped)) {
        qCWarning(lcCse()) << "Could not decrypt";
        return {};
    }

    QBuffer gZipBuffer;
    if (!gZipBuffer.open(QIODevice::WriteOnly)) {
        return {};
    }
    const auto bytesWritten = gZipBuffer.write(decryptedAndUnGzipped);
    gZipBuffer.close();
    if (bytesWritten < 0) {
        return {};
    }

    auto gZipUnCompressionDevice = KCompressionDevice(&gZipBuffer, false, KCompressionDevice::GZip);
    if (!gZipUnCompressionDevice.open(QIODevice::ReadOnly)) {
        return {};
    }

    decryptedAndUnGzipped = gZipUnCompressionDevice.readAll();
    gZipUnCompressionDevice.close();

    return decryptedAndUnGzipped;
}

EncryptionHelper::StreamingDecryptor::StreamingDecryptor(const QByteArray &key, const QByteArray &iv, quint64 totalSize) : _totalSize(totalSize)
{
    if (_ctx && !key.isEmpty() && !iv.isEmpty() && totalSize > 0) {
        _isInitialized = true;

        /* Initialize the decryption operation. */
        if(!EVP_DecryptInit_ex(_ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr)) {
            qCritical(lcCse()) << "Could not init cipher";
            _isInitialized = false;
        }

        EVP_CIPHER_CTX_set_padding(_ctx, 0);

        /* Set IV length. */
        if(!EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
            qCritical(lcCse()) << "Could not set iv length";
            _isInitialized = false;
        }

        /* Initialize key and IV */
        if(!EVP_DecryptInit_ex(_ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.constData()), reinterpret_cast<const unsigned char*>(iv.constData()))) {
            qCritical(lcCse()) << "Could not set key and iv";
            _isInitialized = false;
        }
    }
}

QByteArray EncryptionHelper::StreamingDecryptor::chunkDecryption(const char *input, quint64 chunkSize)
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);

    Q_ASSERT(isInitialized());
    if (!isInitialized()) {
        qCritical(lcCse()) << "Decryption failed. Decryptor is not initialized!";
        return QByteArray();
    }

    Q_ASSERT(buffer.isOpen() && buffer.isWritable());
    if (!buffer.isOpen() || !buffer.isWritable()) {
        qCritical(lcCse()) << "Decryption failed. Incorrect output device!";
        return QByteArray();
    }

    Q_ASSERT(input);
    if (!input) {
        qCritical(lcCse()) << "Decryption failed. Incorrect input!";
        return QByteArray();
    }

    Q_ASSERT(chunkSize > 0);
    if (chunkSize <= 0) {
        qCritical(lcCse()) << "Decryption failed. Incorrect chunkSize!";
        return QByteArray();
    }

    if (_decryptedSoFar == 0) {
        qCDebug(lcCse()) << "Decryption started";
    }

    Q_ASSERT(_decryptedSoFar + chunkSize <= _totalSize);
    if (_decryptedSoFar + chunkSize > _totalSize) {
        qCritical(lcCse()) << "Decryption failed. Chunk is out of range!";
        return QByteArray();
    }

    Q_ASSERT(_decryptedSoFar + chunkSize < OCC::Constants::e2EeTagSize || _totalSize - OCC::Constants::e2EeTagSize >= _decryptedSoFar + chunkSize - OCC::Constants::e2EeTagSize);
    if (_decryptedSoFar + chunkSize > OCC::Constants::e2EeTagSize && _totalSize - OCC::Constants::e2EeTagSize < _decryptedSoFar + chunkSize - OCC::Constants::e2EeTagSize) {
        qCritical(lcCse()) << "Decryption failed. Incorrect chunk!";
        return QByteArray();
    }

    const bool isLastChunk = _decryptedSoFar + chunkSize == _totalSize;

    // last OCC::Constants::e2EeTagSize bytes is ALWAYS a e2EeTag!!!
    const qint64 size = isLastChunk ? chunkSize - OCC::Constants::e2EeTagSize : chunkSize;

    // either the size is more than 0 and an e2EeTag is at the end of chunk, or, chunk is the e2EeTag itself
    Q_ASSERT(size > 0 || chunkSize == OCC::Constants::e2EeTagSize);
    if (size <= 0 && chunkSize != OCC::Constants::e2EeTagSize) {
        qCritical(lcCse()) << "Decryption failed. Invalid input size: " << size << " !";
        return QByteArray();
    }

    qint64 inputPos = 0;

    QByteArray decryptedBlock(blockSize + OCC::Constants::e2EeTagSize - 1, '\0');

    while(inputPos < size) {
        // read blockSize or less bytes
        const QByteArray encryptedBlock(input + inputPos, qMin(size - inputPos, blockSize));

        if (encryptedBlock.size() == 0) {
            qCritical(lcCse()) << "Could not read data from the input buffer.";
            return QByteArray();
        }

        int outLen = 0;

        if(!EVP_DecryptUpdate(_ctx, unsignedData(decryptedBlock), &outLen, reinterpret_cast<const unsigned char*>(encryptedBlock.data()), encryptedBlock.size())) {
            qCritical(lcCse()) << "Could not decrypt";
            return QByteArray();
        }

        const auto writtenToOutput = buffer.write(decryptedBlock, outLen);

        Q_ASSERT(writtenToOutput == outLen);
        if (writtenToOutput != outLen) {
            qCritical(lcCse()) << "Failed to write decrypted data to device.";
            return QByteArray();
        }

        // advance input position for further read
        inputPos += encryptedBlock.size();

        _decryptedSoFar += encryptedBlock.size();
    }

    if (isLastChunk) {
        // if it's a last chunk, we'd need to read a e2EeTag at the end and finalize the decryption

        Q_ASSERT(chunkSize - inputPos == OCC::Constants::e2EeTagSize);
        if (chunkSize - inputPos != OCC::Constants::e2EeTagSize) {
            qCritical(lcCse()) << "Decryption failed. e2EeTag is missing!";
            return QByteArray();
        }

        int outLen = 0;

        QByteArray e2EeTag = QByteArray(input + inputPos, OCC::Constants::e2EeTagSize);

        /* Set expected e2EeTag value. Works in OpenSSL 1.0.1d and later */
        if(!EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_TAG, e2EeTag.size(), reinterpret_cast<unsigned char*>(e2EeTag.data()))) {
            qCritical(lcCse()) << "Could not set expected e2EeTag";
            return QByteArray();
        }

        if(1 != EVP_DecryptFinal_ex(_ctx, unsignedData(decryptedBlock), &outLen)) {
            qCritical(lcCse()) << "Could finalize decryption";
            return QByteArray();
        }

        const auto writtenToOutput = buffer.write(decryptedBlock, outLen);

        Q_ASSERT(writtenToOutput == outLen);
        if (writtenToOutput != outLen) {
            qCritical(lcCse()) << "Failed to write decrypted data to device.";
            return QByteArray();
        }

        _decryptedSoFar += OCC::Constants::e2EeTagSize;

        _isFinished = true;
    }

    if (isFinished()) {
        qCDebug(lcCse()) << "Decryption complete";
    }

    return byteArray;
}

bool EncryptionHelper::StreamingDecryptor::isInitialized() const
{
    return _isInitialized;
}

bool EncryptionHelper::StreamingDecryptor::isFinished() const
{
    return _isFinished;
}

NextcloudSslCertificate::NextcloudSslCertificate() = default;

NextcloudSslCertificate::NextcloudSslCertificate(const NextcloudSslCertificate &other) = default;

NextcloudSslCertificate::NextcloudSslCertificate(const QSslCertificate &certificate)
    : _certificate(certificate)
{
}

NextcloudSslCertificate::NextcloudSslCertificate(QSslCertificate &&certificate)
    : _certificate(std::move(certificate))
{
}

OCC::NextcloudSslCertificate::operator QSslCertificate()
{
    return _certificate;
}

QSslCertificate &NextcloudSslCertificate::get()
{
    return _certificate;
}

const QSslCertificate &NextcloudSslCertificate::get() const
{
    return _certificate;
}

NextcloudSslCertificate &NextcloudSslCertificate::operator=(const NextcloudSslCertificate &other)
{
    if (this != &other) {
        _certificate = other._certificate;
    }

    return *this;
}

NextcloudSslCertificate &NextcloudSslCertificate::operator=(NextcloudSslCertificate &&other)
{
    if (this != &other) {
        _certificate = std::move(other._certificate);
    }

    return *this;
}

OCC::NextcloudSslCertificate::operator QSslCertificate() const
{
    return _certificate;
}

CertificateInformation::CertificateInformation()
{
    checkEncryptionCertificate();
}

CertificateInformation::CertificateInformation(PKCS11_CERT *hardwareCertificate,
                                               QSslCertificate &&certificate)
    : _hardwareCertificate{hardwareCertificate}
    , _certificate{std::move(certificate)}
    , _certificateType{CertificateType::HardwareCertificate}
{
    checkEncryptionCertificate();
}

CertificateInformation::CertificateInformation(CertificateType certificateType,
                                               const QByteArray &privateKey,
                                               QSslCertificate &&certificate)
    : _hardwareCertificate()
    , _privateKeyData()
    , _certificate(std::move(certificate))
    , _certificateType{certificateType}
{
    if (!privateKey.isEmpty()) {
        setPrivateKeyData(privateKey);
    }

    switch (_certificateType)
    {
    case CertificateType::HardwareCertificate:
        checkEncryptionCertificate();
        break;
    case CertificateType::SoftwareNextcloudCertificate:
        doNotCheckEncryptionCertificate();
        break;
    }
}

bool CertificateInformation::operator==(const CertificateInformation &other) const
{
    return _certificate.digest(QCryptographicHash::Sha256) == other._certificate.digest(QCryptographicHash::Sha256);
}

void CertificateInformation::clear()
{
    _hardwareCertificate = nullptr;
    _privateKeyData.clear();
    _certificate.clear();
    _certificateExpired = true;
    _certificateNotYetValid = true;
    _certificateRevoked = true;
    _certificateInvalid = true;
}

const QByteArray& CertificateInformation::getPrivateKeyData() const
{
    return _privateKeyData;
}

void CertificateInformation::setPrivateKeyData(const QByteArray &privateKey)
{
    _privateKeyData = privateKey;
}

QList<QSslError> CertificateInformation::verify() const
{
    auto result = QSslCertificate::verify({_certificate});

    auto hasNeededExtendedKeyUsageExtension = false;
    const auto &allExtensions = _certificate.extensions();
    for (const auto &oneExtension : allExtensions) {
        if (oneExtension.oid() == QStringLiteral("2.5.29.37")) {
            const auto extendedKeyUsageList = oneExtension.value().toList();
            for (const auto &oneExtendedKeyUsageValue : extendedKeyUsageList) {
                if (oneExtendedKeyUsageValue == QStringLiteral("E-mail Protection")) {
                    hasNeededExtendedKeyUsageExtension = true;
                    break;
                }
            }
        }
    }
    if (!hasNeededExtendedKeyUsageExtension) {
        result.append(QSslError{QSslError::InvalidPurpose});
    }

    return result;
}

bool CertificateInformation::isSelfSigned() const
{
    return _certificate.isSelfSigned();
}

QSslKey CertificateInformation::getSslPublicKey() const
{
    return _certificate.publicKey();
}

PKey CertificateInformation::getEvpPublicKey() const
{
    const auto publicKey = _certificate.publicKey();
    Q_ASSERT(!publicKey.isNull());
    if (publicKey.isNull()) {
        qCWarning(lcCse) << "Public key is null. Could not encrypt.";
    }
    Bio publicKeyBio;
    const auto publicKeyPem = publicKey.toPem();
    BIO_write(publicKeyBio, publicKeyPem.constData(), publicKeyPem.size());
    return PKey::readPublicKey(publicKeyBio);
}

PKCS11_KEY *CertificateInformation::getPkcs11PrivateKey() const
{
    return canDecrypt() && _hardwareCertificate ? PKCS11_find_key(_hardwareCertificate) : nullptr;
}

PKey CertificateInformation::getEvpPrivateKey() const
{
    if (_hardwareCertificate) {
        return PKey::readHardwarePrivateKey(PKCS11_find_key(_hardwareCertificate));
    } else {
        const auto privateKeyPem = _privateKeyData;
        Q_ASSERT(!privateKeyPem.isEmpty());
        if (privateKeyPem.isEmpty()) {
            qCWarning(lcCse) << "Private key is empty. Could not encrypt.";
        }

        Bio privateKeyBio;
        BIO_write(privateKeyBio, privateKeyPem.constData(), privateKeyPem.size());
        return PKey::readPrivateKey(privateKeyBio);
    }
}

const QSslCertificate &CertificateInformation::getCertificate() const
{
    return _certificate;
}

bool CertificateInformation::canEncrypt() const
{
    return (_hardwareCertificate || !_certificate.isNull()) && !_certificateExpired && !_certificateNotYetValid && !_certificateRevoked && !_certificateInvalid;
}

bool CertificateInformation::canDecrypt() const
{
    return _hardwareCertificate || !_privateKeyData.isEmpty();
}

bool CertificateInformation::userCertificateNeedsMigration() const
{
    return _hardwareCertificate &&
        (_certificateExpired || _certificateNotYetValid || _certificateRevoked || _certificateInvalid);
}

bool CertificateInformation::sensitiveDataRemaining() const
{
    return _hardwareCertificate && !_privateKeyData.isEmpty() && !_certificate.isNull();
}

QByteArray CertificateInformation::sha256Fingerprint() const
{
    return _certificate.digest(QCryptographicHash::Sha256).toBase64();
}

void CertificateInformation::checkEncryptionCertificate()
{
    _certificateExpired = false;
    _certificateNotYetValid = false;
    _certificateRevoked = false;
    _certificateInvalid = false;

    const auto sslErrors = QSslCertificate::verify({_certificate});
    for (const auto &sslError : sslErrors) {
        qCWarning(lcCse()) << "certificate validation error" << sslError;
        switch (sslError.error())
        {
        case QSslError::CertificateExpired:
            _certificateExpired = true;
            break;
        case QSslError::CertificateNotYetValid:
            _certificateNotYetValid = true;
            break;
        case QSslError::CertificateRevoked:
            _certificateRevoked = true;
            break;
        case QSslError::UnableToGetIssuerCertificate:
        case QSslError::UnableToDecryptCertificateSignature:
        case QSslError::UnableToDecodeIssuerPublicKey:
        case QSslError::CertificateSignatureFailed:
        case QSslError::InvalidNotBeforeField:
        case QSslError::InvalidNotAfterField:
        case QSslError::SelfSignedCertificate:
        case QSslError::SelfSignedCertificateInChain:
        case QSslError::UnableToGetLocalIssuerCertificate:
        case QSslError::UnableToVerifyFirstCertificate:
        case QSslError::InvalidCaCertificate:
        case QSslError::PathLengthExceeded:
        case QSslError::InvalidPurpose:
        case QSslError::CertificateUntrusted:
        case QSslError::CertificateRejected:
        case QSslError::SubjectIssuerMismatch:
        case QSslError::AuthorityIssuerSerialNumberMismatch:
        case QSslError::NoPeerCertificate:
        case QSslError::HostNameMismatch:
        case QSslError::NoSslSupport:
        case QSslError::CertificateBlacklisted:
        case QSslError::CertificateStatusUnknown:
        case QSslError::OcspNoResponseFound:
        case QSslError::OcspMalformedRequest:
        case QSslError::OcspMalformedResponse:
        case QSslError::OcspInternalError:
        case QSslError::OcspTryLater:
        case QSslError::OcspSigRequred:
        case QSslError::OcspUnauthorized:
        case QSslError::OcspResponseCannotBeTrusted:
        case QSslError::OcspResponseCertIdUnknown:
        case QSslError::OcspResponseExpired:
        case QSslError::OcspStatusUnknown:
        case QSslError::UnspecifiedError:
            _certificateInvalid = true;
            break;
        case QSslError::NoError:
            break;
        }
    }
}

void CertificateInformation::doNotCheckEncryptionCertificate()
{
    _certificateExpired = false;
    _certificateNotYetValid = false;
    _certificateRevoked = false;
    _certificateInvalid = false;
}

}
