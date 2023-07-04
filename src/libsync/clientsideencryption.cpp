#include "clientsideencryption.h"

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/rand.h>

#include "account.h"
#include "capabilities.h"
#include "networkjobs.h"
#include "clientsideencryptionjobs.h"
#include "theme.h"
#include "creds/abstractcredentials.h"
#include "common/utility.h"
#include "common/constants.h"
#include "wordlist.h"

#include <qt5keychain/keychain.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>
#include <QXmlStreamReader>
#include <QXmlStreamNamespaceDeclaration>
#include <QStack>
#include <QInputDialog>
#include <QLineEdit>
#include <QIODevice>
#include <QUuid>
#include <QScopeGuard>
#include <QRandomGenerator>
#include <QCryptographicHash>

#include <map>
#include <string>
#include <algorithm>

#include <cstdio>

QDebug operator<<(QDebug out, const std::string& str)
{
    out << QString::fromStdString(str);
    return out;
}

using namespace QKeychain;

namespace OCC
{

Q_LOGGING_CATEGORY(lcCse, "nextcloud.sync.clientsideencryption", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCseDecryption, "nextcloud.e2e", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCseMetadata, "nextcloud.metadata", QtInfoMsg)

QString e2eeBaseUrl()
{
    return QStringLiteral("ocs/v2.php/apps/end_to_end_encryption/api/v1/");
}

namespace {
constexpr char accountProperty[] = "account";

constexpr char e2e_cert[] = "_e2e-certificate";
constexpr char e2e_private[] = "_e2e-private";
constexpr char e2e_public[] = "_e2e-public";
constexpr char e2e_mnemonic[] = "_e2e-mnemonic";

constexpr auto metadataKeyJsonKey = "metadataKey";

constexpr qint64 blockSize = 1024;

constexpr auto metadataKeySize = 16;

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

class Bio {
public:
    Bio()
        : _bio(BIO_new(BIO_s_mem()))
    {
    }

    ~Bio()
    {
        BIO_free_all(_bio);
    }

    operator const BIO*() const
    {
        return _bio;
    }

    operator BIO*()
    {
        return _bio;
    }

private:
    Q_DISABLE_COPY(Bio)

    BIO* _bio;
};

class PKeyCtx {
public:
    explicit PKeyCtx(int id, ENGINE *e = nullptr)
        : _ctx(EVP_PKEY_CTX_new_id(id, e))
    {
    }

    ~PKeyCtx()
    {
        EVP_PKEY_CTX_free(_ctx);
    }

    // The move constructor is needed for pre-C++17 where
    // return-value optimization (RVO) is not obligatory
    // and we have a `forKey` static function that returns
    // an instance of this class
    PKeyCtx(PKeyCtx&& other)
    {
        std::swap(_ctx, other._ctx);
    }

    PKeyCtx& operator=(PKeyCtx&& other) = delete;

    static PKeyCtx forKey(EVP_PKEY *pkey, ENGINE *e = nullptr)
    {
        PKeyCtx ctx;
        ctx._ctx = EVP_PKEY_CTX_new(pkey, e);
        return ctx;
    }

    operator EVP_PKEY_CTX*()
    {
        return _ctx;
    }

private:
    Q_DISABLE_COPY(PKeyCtx)

    PKeyCtx() = default;

    EVP_PKEY_CTX* _ctx = nullptr;
};

}

class ClientSideEncryption::PKey {
public:
    ~PKey()
    {
        EVP_PKEY_free(_pkey);
    }

    // The move constructor is needed for pre-C++17 where
    // return-value optimization (RVO) is not obligatory
    // and we have a static functions that return
    // an instance of this class
    PKey(PKey&& other)
    {
        std::swap(_pkey, other._pkey);
    }

    PKey& operator=(PKey&& other) = delete;

    static PKey readPublicKey(Bio &bio)
    {
        PKey result;
        result._pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
        return result;
    }

    static PKey readPrivateKey(Bio &bio)
    {
        PKey result;
        result._pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        return result;
    }

    static PKey generate(PKeyCtx& ctx)
    {
        PKey result;
        if (EVP_PKEY_keygen(ctx, &result._pkey) <= 0) {
            result._pkey = nullptr;
        }
        return result;
    }

    operator EVP_PKEY*()
    {
        return _pkey;
    }

    operator EVP_PKEY*() const
    {
        return _pkey;
    }

private:
    Q_DISABLE_COPY(PKey)

    PKey() = default;

    EVP_PKEY* _pkey = nullptr;
};

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

QByteArray BIO2ByteArray(Bio &b) {
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

QByteArray generatePassword(const QString& wordlist, const QByteArray& salt) {
    qCInfo(lcCse()) << "Start encryption key generation!";

    const int iterationCount = 1024;
    const int keyStrength = 256;
    const int keyLength = keyStrength/8;

    QByteArray secretKey(keyLength, '\0');

    int ret = PKCS5_PBKDF2_HMAC_SHA1(
        wordlist.toLocal8Bit().constData(),     // const char *password,
        wordlist.size(),                        // int password length,
        (const unsigned char *)salt.constData(),// const unsigned char *salt,
        salt.size(),                            // int saltlen,
        iterationCount,                         // int iterations,
        keyLength,                              // int keylen,
        unsignedData(secretKey)                 // unsigned char *out
        );

    if (ret != 1) {
        qCInfo(lcCse()) << "Failed to generate encryption key";
        // Error out?
    }

    qCInfo(lcCse()) << "Encryption key generated!";

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
        qCInfo(lcCse()) << "Error creating cipher";
        handleErrors();
    }

    /* Initialise the decryption operation. */
    if(!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)) {
        qCInfo(lcCse()) << "Error initializing context with aes_256";
        handleErrors();
    }

    // No padding
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
        qCInfo(lcCse()) << "Error setting iv length";
        handleErrors();
    }

    /* Initialise key and IV */
    if(!EVP_EncryptInit_ex(ctx, nullptr, nullptr, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv";
        handleErrors();
    }

    // We write the base64 encoded private key
    QByteArray privateKeyB64 = privateKey.toBase64();

    // Make sure we have enough room in the cipher text
    QByteArray ctext(privateKeyB64.size() + 32, '\0');

    // Do the actual encryption
    int len = 0;
    if(!EVP_EncryptUpdate(ctx, unsignedData(ctext), &len, (unsigned char *)privateKeyB64.constData(), privateKeyB64.size())) {
        qCInfo(lcCse()) << "Error encrypting";
        handleErrors();
    }

    int clen = len;

    /* Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if(1 != EVP_EncryptFinal_ex(ctx, unsignedData(ctext) + len, &len)) {
        qCInfo(lcCse()) << "Error finalizing encryption";
        handleErrors();
    }
    clen += len;

    /* Get the e2EeTag */
    QByteArray e2EeTag(OCC::Constants::e2EeTagSize, '\0');
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, OCC::Constants::e2EeTagSize, unsignedData(e2EeTag))) {
        qCInfo(lcCse()) << "Error getting the e2EeTag";
        handleErrors();
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
    auto pkey = ClientSideEncryption::PKey::readPrivateKey(privateKeyBio);

    Bio pemBio;
    PEM_write_bio_PKCS8PrivateKey(pemBio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    QByteArray pem = BIO2ByteArray(pemBio);

    return pem;
}

QByteArray encryptStringAsymmetric(const QSslKey key, const QByteArray &data)
{
    Q_ASSERT(!key.isNull());
    if (key.isNull()) {
        qCDebug(lcCse) << "Public key is null. Could not encrypt.";
        return {};
    }
    Bio publicKeyBio;
    const auto publicKeyPem = key.toPem();
    BIO_write(publicKeyBio, publicKeyPem.constData(), publicKeyPem.size());
    const auto publicKey = ClientSideEncryption::PKey::readPublicKey(publicKeyBio);
    return EncryptionHelper::encryptStringAsymmetric(publicKey, data.toBase64());
}

QByteArray decryptStringAsymmetric(const QByteArray &privateKeyPem, const QByteArray &data)
{
    Q_ASSERT(!privateKeyPem.isEmpty());
    if (privateKeyPem.isEmpty()) {
        qCDebug(lcCse) << "Private key is empty. Could not encrypt.";
        return {};
    }

    Bio privateKeyBio;
    BIO_write(privateKeyBio, privateKeyPem.constData(), privateKeyPem.size());
    const auto key = ClientSideEncryption::PKey::readPrivateKey(privateKeyBio);

    // Also base64 decode the result
    const auto decryptResult = EncryptionHelper::decryptStringAsymmetric(key, QByteArray::fromBase64(data));

    if (decryptResult.isEmpty()) {
        qCDebug(lcCse()) << "ERROR. Could not decrypt data";
        return {};
    }
    return QByteArray::fromBase64(decryptResult);
}

QByteArray encryptStringSymmetric(const QByteArray& key, const QByteArray& data) {
    QByteArray iv = generateRandom(16);

    CipherCtx ctx;

    /* Create and initialise the context */
    if(!ctx) {
        qCInfo(lcCse()) << "Error creating cipher";
        handleErrors();
        return {};
    }

    /* Initialise the decryption operation. */
    if(!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr)) {
        qCInfo(lcCse()) << "Error initializing context with aes_128";
        handleErrors();
        return {};
    }

    // No padding
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr)) {
        qCInfo(lcCse()) << "Error setting iv length";
        handleErrors();
        return {};
    }

    /* Initialise key and IV */
    if(!EVP_EncryptInit_ex(ctx, nullptr, nullptr, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv";
        handleErrors();
        return {};
    }

    // We write the data base64 encoded
    QByteArray dataB64 = data.toBase64();

    // Make sure we have enough room in the cipher text
    QByteArray ctext(dataB64.size() + 16, '\0');

    // Do the actual encryption
    int len = 0;
    if(!EVP_EncryptUpdate(ctx, unsignedData(ctext), &len, (unsigned char *)dataB64.constData(), dataB64.size())) {
        qCInfo(lcCse()) << "Error encrypting";
        handleErrors();
        return {};
    }

    int clen = len;

    /* Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if(1 != EVP_EncryptFinal_ex(ctx, unsignedData(ctext) + len, &len)) {
        qCInfo(lcCse()) << "Error finalizing encryption";
        handleErrors();
        return {};
    }
    clen += len;

    /* Get the e2EeTag */
    QByteArray e2EeTag(OCC::Constants::e2EeTagSize, '\0');
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, OCC::Constants::e2EeTagSize, unsignedData(e2EeTag))) {
        qCInfo(lcCse()) << "Error getting the e2EeTag";
        handleErrors();
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

QByteArray decryptStringAsymmetric(EVP_PKEY *privateKey, const QByteArray& data) {
    int err = -1;

    qCInfo(lcCseDecryption()) << "Start to work the decryption.";
    auto ctx = PKeyCtx::forKey(privateKey, ENGINE_get_default_RSA());
    if (!ctx) {
        qCInfo(lcCseDecryption()) << "Could not create the PKEY context.";
        handleErrors();
        return {};
    }

    err = EVP_PKEY_decrypt_init(ctx);
    if (err <= 0) {
        qCInfo(lcCseDecryption()) << "Could not init the decryption of the metadata";
        handleErrors();
        return {};
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        qCInfo(lcCseDecryption()) << "Error setting the encryption padding.";
        handleErrors();
        return {};
    }

    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCseDecryption()) << "Error setting OAEP SHA 256";
        handleErrors();
        return {};
    }

    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCseDecryption()) << "Error setting MGF1 padding";
        handleErrors();
        return {};
    }

    size_t outlen = 0;
    err = EVP_PKEY_decrypt(ctx, nullptr, &outlen,  (unsigned char *)data.constData(), data.size());
    if (err <= 0) {
        qCInfo(lcCseDecryption()) << "Could not determine the buffer length";
        handleErrors();
        return {};
    } else {
        qCInfo(lcCseDecryption()) << "Size of output is: " << outlen;
        qCInfo(lcCseDecryption()) << "Size of data is: " << data.size();
    }

    QByteArray out(static_cast<int>(outlen), '\0');

    if (EVP_PKEY_decrypt(ctx, unsignedData(out), &outlen, (unsigned char *)data.constData(), data.size()) <= 0) {
        const auto error = handleErrors();
        qCCritical(lcCseDecryption()) << "Could not decrypt the data." << error;
        return {};
    } else {
        qCInfo(lcCseDecryption()) << "data decrypted successfully";
    }

    // we don't need extra zeroes in out, so let's only return meaningful data
    out = QByteArray(out.constData(), outlen);

    qCInfo(lcCse()) << out;
    return out;
}

QByteArray encryptStringAsymmetric(EVP_PKEY *publicKey, const QByteArray& data) {
    int err = -1;

    auto ctx = PKeyCtx::forKey(publicKey, ENGINE_get_default_RSA());
    if (!ctx) {
        qCInfo(lcCse()) << "Could not initialize the pkey context.";
        exit(1);
    }

    if (EVP_PKEY_encrypt_init(ctx) != 1) {
        qCInfo(lcCse()) << "Error initilaizing the encryption.";
        exit(1);
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        qCInfo(lcCse()) << "Error setting the encryption padding.";
        exit(1);
    }

    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCse()) << "Error setting OAEP SHA 256";
        exit(1);
    }

    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCse()) << "Error setting MGF1 padding";
        exit(1);
    }

    size_t outLen = 0;
    if (EVP_PKEY_encrypt(ctx, nullptr, &outLen, (unsigned char *)data.constData(), data.size()) != 1) {
        qCInfo(lcCse()) << "Error retrieving the size of the encrypted data";
        exit(1);
    } else {
        qCInfo(lcCse()) << "Encryption Length:" << outLen;
    }

    QByteArray out(static_cast<int>(outLen), '\0');
    if (EVP_PKEY_encrypt(ctx, unsignedData(out), &outLen, (unsigned char *)data.constData(), data.size()) != 1) {
        qCInfo(lcCse()) << "Could not encrypt key." << err;
        exit(1);
    }

    // Transform the encrypted data into base64.
    qCInfo(lcCse()) << out.toBase64();
    return out.toBase64();
}

}


ClientSideEncryption::ClientSideEncryption() = default;

void ClientSideEncryption::initialize(const AccountPtr &account)
{
    Q_ASSERT(account);

    qCInfo(lcCse()) << "Initializing";
    if (!account->capabilities().clientSideEncryptionAvailable()) {
        qCInfo(lcCse()) << "No Client side encryption available on server.";
        emit initializationFinished();
        return;
    }

    fetchCertificateFromKeyChain(account);
}

void ClientSideEncryption::fetchCertificateFromKeyChain(const AccountPtr &account)
{
    const QString kck = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_cert,
        account->id()
        );

    const auto job = new ReadPasswordJob(Theme::instance()->appName());
    job->setProperty(accountProperty, QVariant::fromValue(account));
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::publicCertificateFetched);
    job->start();
}

void ClientSideEncryption::fetchPublicKeyFromKeyChain(const AccountPtr &account)
{
    const QString kck = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_public,
        account->id()
        );

    auto *job = new ReadPasswordJob(Theme::instance()->appName());
    job->setProperty(accountProperty, QVariant::fromValue(account));
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::publicKeyFetched);
    job->start();
}

bool ClientSideEncryption::checkPublicKeyValidity(const AccountPtr &account) const
{
    QByteArray data = EncryptionHelper::generateRandom(64);

    Bio publicKeyBio;
    QByteArray publicKeyPem = account->e2e()->_publicKey.toPem();
    BIO_write(publicKeyBio, publicKeyPem.constData(), publicKeyPem.size());
    auto publicKey = PKey::readPublicKey(publicKeyBio);

    auto encryptedData = EncryptionHelper::encryptStringAsymmetric(publicKey, data.toBase64());

    Bio privateKeyBio;
    QByteArray privateKeyPem = account->e2e()->_privateKey;
    BIO_write(privateKeyBio, privateKeyPem.constData(), privateKeyPem.size());
    auto key = PKey::readPrivateKey(privateKeyBio);

    QByteArray decryptResult = QByteArray::fromBase64(EncryptionHelper::decryptStringAsymmetric( key, QByteArray::fromBase64(encryptedData)));

    if (data != decryptResult) {
        qCInfo(lcCse()) << "invalid private key";
        return false;
    }

    return true;
}

bool ClientSideEncryption::checkServerPublicKeyValidity(const QByteArray &serverPublicKeyString) const
{
    Bio serverPublicKeyBio;
    BIO_write(serverPublicKeyBio, serverPublicKeyString.constData(), serverPublicKeyString.size());
    const auto serverPublicKey = PKey::readPrivateKey(serverPublicKeyBio);

    Bio certificateBio;
    const auto certificatePem = _certificate.toPem();
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
        fetchPublicKeyFromKeyChain(account);
        return;
    }

    _certificate = QSslCertificate(readJob->binaryData(), QSsl::Pem);

    if (_certificate.isNull()) {
        fetchPublicKeyFromKeyChain(account);
        return;
    }

    _publicKey = _certificate.publicKey();

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

void ClientSideEncryption::publicKeyFetched(QKeychain::Job *incoming)
{
    const auto readJob = dynamic_cast<ReadPasswordJob *>(incoming);
    const auto account = readJob->property(accountProperty).value<AccountPtr>();
    Q_ASSERT(account);

           // Error or no valid public key error out
    if (readJob->error() != NoError || readJob->binaryData().length() == 0) {
        getPublicKeyFromServer(account);
        return;
    }

    const auto publicKey =  QSslKey(readJob->binaryData(), QSsl::Rsa, QSsl::Pem, QSsl::PublicKey);

    if (publicKey.isNull()) {
        getPublicKeyFromServer(account);
        return;
    }

    _publicKey = publicKey;

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

void ClientSideEncryption::privateKeyFetched(Job *incoming)
{
    auto *readJob = dynamic_cast<ReadPasswordJob *>(incoming);
    auto account = readJob->property(accountProperty).value<AccountPtr>();
    Q_ASSERT(account);

    // Error or no valid public key error out
    if (readJob->error() != NoError || readJob->binaryData().length() == 0) {
        forgetSensitiveData(account);
        getPublicKeyFromServer(account);
        return;
    }

    //_privateKey = QSslKey(readJob->binaryData(), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    _privateKey = readJob->binaryData();

    if (_privateKey.isNull()) {
        getPrivateKeyFromServer(account);
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
        forgetSensitiveData(account);
        getPublicKeyFromServer(account);
        return;
    }

    _mnemonic = readJob->textData();

    qCInfo(lcCse()) << "Mnemonic key fetched from keychain: " << _mnemonic;

    checkServerHasSavedKeys(account);
}

void ClientSideEncryption::writePrivateKey(const AccountPtr &account)
{
    const QString kck = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_private,
        account->id()
        );

    auto *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setBinaryData(_privateKey);
    connect(job, &WritePasswordJob::finished, [](Job *incoming) {
        Q_UNUSED(incoming);
        qCInfo(lcCse()) << "Private key stored in keychain";
    });
    job->start();
}

void ClientSideEncryption::writeCertificate(const AccountPtr &account)
{
    const QString kck = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_cert,
        account->id()
        );

    auto *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setBinaryData(_certificate.toPem());
    connect(job, &WritePasswordJob::finished, [](Job *incoming) {
        Q_UNUSED(incoming);
        qCInfo(lcCse()) << "Certificate stored in keychain";
    });
    job->start();
}

void ClientSideEncryption::generateMnemonic()
{
    const auto list = WordList::getRandomWords(12);
    _mnemonic = list.join(' ');
}

template <typename L>
void ClientSideEncryption::writeMnemonic(OCC::AccountPtr account,
                                         L nextCall)
{
    const QString kck = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_mnemonic,
        account->id()
        );

    auto *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setTextData(_mnemonic);
    connect(job, &WritePasswordJob::finished, [this, account, nextCall = std::move(nextCall)](Job *incoming) mutable {
        if (incoming->error() != Error::NoError) {
            failedToInitialize(account);
            return;
        }

        nextCall();
    });
    job->start();
}

void ClientSideEncryption::forgetSensitiveData(const AccountPtr &account)
{
    if (!sensitiveDataRemaining()) {
        checkAllSensitiveDataDeleted();
        return;
    }

    const auto createDeleteJob = [account](const QString user) {
        auto *job = new DeletePasswordJob(Theme::instance()->appName());
        job->setInsecureFallback(false);
        job->setKey(AbstractCredentials::keychainKey(account->url().toString(), user, account->id()));
        return job;
    };

    const auto user = account->credentials()->user();
    const auto deletePrivateKeyJob = createDeleteJob(user + e2e_private);
    const auto deleteCertJob = createDeleteJob(user + e2e_cert);
    const auto deleteMnemonicJob = createDeleteJob(user + e2e_mnemonic);

    connect(deletePrivateKeyJob, &DeletePasswordJob::finished, this, &ClientSideEncryption::handlePrivateKeyDeleted);
    connect(deleteCertJob, &DeletePasswordJob::finished, this, &ClientSideEncryption::handleCertificateDeleted);
    connect(deleteMnemonicJob, &DeletePasswordJob::finished, this, &ClientSideEncryption::handleMnemonicDeleted);
    deletePrivateKeyJob->start();
    deleteCertJob->start();
    deleteMnemonicJob->start();
}

void ClientSideEncryption::handlePrivateKeyDeleted(const QKeychain::Job* const incoming)
{
    const auto error = incoming->error();
    if (error != QKeychain::NoError && error != QKeychain::EntryNotFound) {
        qCWarning(lcCse) << "Private key could not be deleted:" << incoming->errorString();
        return;
    }

    qCDebug(lcCse) << "Private key successfully deleted from keychain. Clearing.";
    _privateKey = QByteArray();
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
    _certificate = QSslCertificate();
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
    _mnemonic = QString();
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

    _publicKey.clear();
    Q_EMIT publicKeyDeleted();
    checkAllSensitiveDataDeleted();
}

bool ClientSideEncryption::sensitiveDataRemaining() const
{
    return !_privateKey.isEmpty() || !_certificate.isNull() || !_mnemonic.isEmpty();
}

void ClientSideEncryption::failedToInitialize(const AccountPtr &account)
{
    forgetSensitiveData(account);
    Q_EMIT initializationFinished();
}

void ClientSideEncryption::checkAllSensitiveDataDeleted()
{
    if (sensitiveDataRemaining()) {
        qCWarning(lcCse) << "Some sensitive data emaining:"
                       << "Private key:" << (_privateKey.isEmpty() ? "is empty" : "is not empty")
                         << "Certificate is null:" << (_certificate.isNull() ? "true" : "false")
                         << "Mnemonic:" << (_mnemonic.isEmpty() ? "is empty" : "is not empty");
        return;
    }

    Q_EMIT sensitiveDataForgotten();
}

void ClientSideEncryption::generateKeyPair(const AccountPtr &account)
{
    // AES/GCM/NoPadding,
    // metadataKeys with RSA/ECB/OAEPWithSHA-256AndMGF1Padding
    qCInfo(lcCse()) << "No public key, generating a pair.";
    const int rsaKeyLen = 2048;


    // Init RSA
    PKeyCtx ctx(EVP_PKEY_RSA);

    if(EVP_PKEY_keygen_init(ctx) <= 0) {
        qCInfo(lcCse()) << "Couldn't initialize the key generator";
        failedToInitialize(account);
        return;
    }

    if(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, rsaKeyLen) <= 0) {
        qCInfo(lcCse()) << "Couldn't initialize the key generator bits";
        failedToInitialize(account);
        return;
    }

    auto localKeyPair = PKey::generate(ctx);
    if(!localKeyPair) {
        qCInfo(lcCse()) << "Could not generate the key";
        failedToInitialize(account);
        return;
    }

    {
        Bio privKey;
        if (PEM_write_bio_PrivateKey(privKey, localKeyPair, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
            qCWarning(lcCse()) << "Could not read private key from bio.";
            failedToInitialize(account);
            return;
        }

        _privateKey = BIO2ByteArray(privKey);
    }

    Bio privKey;
    if (PEM_write_bio_PrivateKey(privKey, localKeyPair, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
        qCInfo(lcCse()) << "Could not read private key from bio.";
        failedToInitialize(account);
        return;
    }

    qCDebug(lcCse()) << "Key correctly generated";

    auto csrContent = generateCSR(account, std::move(localKeyPair), PKey::readPrivateKey(privKey));
    writeMnemonic(account, [account, keyPair = std::move(csrContent.second), csrContent = std::move(csrContent.first), this]() mutable -> void {
        writeKeyPair(account, std::move(keyPair), csrContent);
    });
}

std::pair<QByteArray, ClientSideEncryption::PKey> ClientSideEncryption::generateCSR(const AccountPtr &account,
                                                                                    PKey keyPair,
                                                                                    PKey privateKey)
{
    auto result = QByteArray{};

    // OpenSSL expects const char.
    auto cnArray = account->davUser().toLocal8Bit();

    auto certParams = std::map<const char *, const char*>{
        {"C", "DE"},
        {"ST", "Baden-Wuerttemberg"},
        {"L", "Stuttgart"},
        {"O","Nextcloud"},
        {"CN", cnArray.constData()}
    };

    int ret = 0;
    int nVersion = 1;

    // 2. set version of x509 req
    auto x509_req = X509_REQ_new();
    auto release_on_exit_x509_req = qScopeGuard([&] {
        X509_REQ_free(x509_req);
    });

    ret = X509_REQ_set_version(x509_req, nVersion);

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

    ret = X509_REQ_sign(x509_req, privateKey, EVP_sha1());    // return x509_req->signature->length
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

void ClientSideEncryption::sendSignRequestCSR(const AccountPtr &account,
                                              PKey keyPair,
                                              const QByteArray &csrContent)
{
    auto job = new SignPublicKeyApiJob(account, e2eeBaseUrl() + "public-key", this);
    job->setCsr(csrContent);

    connect(job, &SignPublicKeyApiJob::jsonReceived, [this, account, keyPair = std::move(keyPair)](const QJsonDocument& json, const int retCode) {
        if (retCode == 200) {
            const auto cert = json.object().value("ocs").toObject().value("data").toObject().value("public-key").toString();
            _certificate = QSslCertificate(cert.toLocal8Bit(), QSsl::Pem);
            _publicKey = _certificate.publicKey();
            Bio certificateBio;
            const auto certificatePem = _certificate.toPem();
            BIO_write(certificateBio, certificatePem.constData(), certificatePem.size());
            const auto x509Certificate = X509Certificate::readCertificate(certificateBio);
            if (!X509_check_private_key(x509Certificate, keyPair)) {
                auto lastError = ERR_get_error();
                while (lastError) {
                    qCWarning(lcCse()) << ERR_lib_error_string(lastError);
                    lastError = ERR_get_error();
                }
                failedToInitialize(account);
                return;
            }
            fetchAndValidatePublicKeyFromServer(account);
        } else {
            qCWarning(lcCse()) << retCode;
            failedToInitialize(account);
            return;
        }
    });
    job->start();
}

void ClientSideEncryption::writeKeyPair(const AccountPtr &account,
                                        PKey keyPair,
                                        const QByteArray &csrContent)
{
    const auto privateKeyKeychainId = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_private,
        account->id()
        );

    const auto publicKeyKeychainId = AbstractCredentials::keychainKey(
        account->url().toString(),
        account->credentials()->user() + e2e_public,
        account->id()
        );

    Bio privateKey;
    if (PEM_write_bio_PrivateKey(privateKey, keyPair, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
        qCWarning(lcCse()) << "Could not read private key from bio.";
        failedToInitialize(account);
        return;
    }
    const auto bytearrayPrivateKey = BIO2ByteArray(privateKey);

    const auto privateKeyJob = new WritePasswordJob(Theme::instance()->appName());
    privateKeyJob->setInsecureFallback(false);
    privateKeyJob->setKey(privateKeyKeychainId);
    privateKeyJob->setBinaryData(bytearrayPrivateKey);
    connect(privateKeyJob, &WritePasswordJob::finished, [keyPair = std::move(keyPair), publicKeyKeychainId, account, csrContent, this] (Job *incoming) mutable {
        if (incoming->error() != Error::NoError) {
            failedToInitialize(account);
            return;
        }

        Bio publicKey;
        if (PEM_write_bio_PUBKEY(publicKey, keyPair) <= 0) {
            qCWarning(lcCse()) << "Could not read public key from bio.";
            failedToInitialize(account);
            return;
        }

        const auto bytearrayPublicKey = BIO2ByteArray(publicKey);

        const auto publicKeyJob = new WritePasswordJob(Theme::instance()->appName());
        publicKeyJob->setInsecureFallback(false);
        publicKeyJob->setKey(publicKeyKeychainId);
        publicKeyJob->setBinaryData(bytearrayPublicKey);
        connect(publicKeyJob, &WritePasswordJob::finished, [account, keyPair = std::move(keyPair), csrContent, this](Job *incoming) mutable {
            if (incoming->error() != Error::NoError) {
                failedToInitialize(account);
                return;
            }

            sendSignRequestCSR(account, std::move(keyPair), csrContent);
        });
        publicKeyJob->start();
    });
    privateKeyJob->start();
}

void ClientSideEncryption::checkServerHasSavedKeys(const AccountPtr &account)
{
    const auto keyIsNotOnServer = [account, this] () {
        qCInfo(lcCse) << "server is missing keys. deleting local keys";

        failedToInitialize(account);
    };

    const auto privateKeyOnServerIsValid = [this] () {
        Q_EMIT initializationFinished();
    };

    const auto publicKeyOnServerIsValid = [this, account, privateKeyOnServerIsValid, keyIsNotOnServer] () {
        checkUserPrivateKeyOnServer(account, privateKeyOnServerIsValid, keyIsNotOnServer);
    };

    checkUserPublicKeyOnServer(account, publicKeyOnServerIsValid, keyIsNotOnServer);
}

template <typename SUCCESS_CALLBACK, typename ERROR_CALLBACK>
void ClientSideEncryption::checkUserKeyOnServer(const QString &keyType,
                                                const AccountPtr &account,
                                                SUCCESS_CALLBACK nextCheck,
                                                ERROR_CALLBACK onError)
{
    auto job = new JsonApiJob(account, e2eeBaseUrl() + keyType, this);
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
void ClientSideEncryption::checkUserPublicKeyOnServer(const AccountPtr &account,
                                                      SUCCESS_CALLBACK nextCheck,
                                                      ERROR_CALLBACK onError)
{
    checkUserKeyOnServer("public-key", account, nextCheck, onError);
}

template <typename SUCCESS_CALLBACK, typename ERROR_CALLBACK>
void ClientSideEncryption::checkUserPrivateKeyOnServer(const AccountPtr &account, SUCCESS_CALLBACK nextCheck, ERROR_CALLBACK onError)
{
    checkUserKeyOnServer("private-key", account, nextCheck, onError);
}

void ClientSideEncryption::encryptPrivateKey(const AccountPtr &account)
{
    if (_mnemonic.isEmpty()) {
        generateMnemonic();
    }

    auto passPhrase = _mnemonic;
    passPhrase = passPhrase.remove(' ').toLower();
    qCDebug(lcCse) << "Passphrase Generated";

    auto salt = EncryptionHelper::generateRandom(40);
    auto secretKey = EncryptionHelper::generatePassword(passPhrase, salt);
    auto cryptedText = EncryptionHelper::encryptPrivateKey(secretKey, EncryptionHelper::privateKeyToPem(_privateKey), salt);

    // Send private key to the server
    auto job = new StorePrivateKeyApiJob(account, e2eeBaseUrl() + "private-key", this);
    job->setPrivateKey(cryptedText);
    connect(job, &StorePrivateKeyApiJob::jsonReceived, [this, account](const QJsonDocument& doc, int retCode) {
        Q_UNUSED(doc);
        switch(retCode) {
        case 200:
            writePrivateKey(account);
            writeCertificate(account);
            writeMnemonic(account, [this] () {
                emit initializationFinished(true);
            });
            break;
        default:
            qCWarning(lcCse) << "Store private key failed, return code:" << retCode;
            failedToInitialize(account);
        }
    });
    job->start();
}

void ClientSideEncryption::decryptPrivateKey(const AccountPtr &account, const QByteArray &key) {
    if (!account->askUserForMnemonic()) {
        qCDebug(lcCse) << "Not allowed to ask user for mnemonic";
        failedToInitialize(account);
        return;
    }

    QString msg = tr("Please enter your end-to-end encryption passphrase:<br>"
                     "<br>"
                     "Username: %2<br>"
                     "Account: %3<br>")
                      .arg(Utility::escape(account->credentials()->user()),
                           Utility::escape(account->displayName()));

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

            _mnemonic = prev;
            QString mnemonic = prev.split(" ").join(QString()).toLower();

            // split off salt
            const auto salt = EncryptionHelper::extractPrivateKeySalt(key);

            auto pass = EncryptionHelper::generatePassword(mnemonic, salt);

            QByteArray privateKey = EncryptionHelper::decryptPrivateKey(pass, key);
            //_privateKey = QSslKey(privateKey, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
            _privateKey = privateKey;

            if (!_privateKey.isNull() && checkPublicKeyValidity(account)) {
                writePrivateKey(account);
                writeCertificate(account);
                writeMnemonic(account, [] () {});
                break;
            }
        } else {
            qCDebug(lcCse()) << "Cancelled";
            failedToInitialize(account);
            return;
        }
    }

    emit initializationFinished();
}

void ClientSideEncryption::getPrivateKeyFromServer(const AccountPtr &account)
{
    auto job = new JsonApiJob(account, e2eeBaseUrl() + "private-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this, account](const QJsonDocument& doc, int retCode) {
        if (retCode == 200) {
            QString key = doc.object()["ocs"].toObject()["data"].toObject()["private-key"].toString();
            decryptPrivateKey(account, key.toLocal8Bit());
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

void ClientSideEncryption::getPublicKeyFromServer(const AccountPtr &account)
{
    auto job = new JsonApiJob(account, e2eeBaseUrl() + "public-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this, account](const QJsonDocument& doc, int retCode) {
        if (retCode == 200) {
            QString publicKey = doc.object()["ocs"].toObject()["data"].toObject()["public-keys"].toObject()[account->davUser()].toString();
            _certificate = QSslCertificate(publicKey.toLocal8Bit(), QSsl::Pem);
            _publicKey = _certificate.publicKey();
            fetchAndValidatePublicKeyFromServer(account);
        } else if (retCode == 404) {
            qCDebug(lcCse()) << "No public key on the server";
            if (!account->e2eEncryptionKeysGenerationAllowed()) {
                qCDebug(lcCse()) << "User did not allow E2E keys generation.";
                failedToInitialize(account);
                return;
            }
            generateKeyPair(account);
        } else {
            qCWarning(lcCse) << "Error while requesting public key: " << retCode;
            failedToInitialize(account);
        }
    });
    job->start();
}

void ClientSideEncryption::fetchAndValidatePublicKeyFromServer(const AccountPtr &account)
{
    auto job = new JsonApiJob(account, e2eeBaseUrl() + "server-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this, account](const QJsonDocument& doc, int retCode) {
        if (retCode == 200) {
            const auto serverPublicKey = doc.object()["ocs"].toObject()["data"].toObject()["public-key"].toString().toLatin1();
            if (checkServerPublicKeyValidity(serverPublicKey)) {
                if (_privateKey.isEmpty()) {
                    getPrivateKeyFromServer(account);
                } else {
                    encryptPrivateKey(account);
                }
            } else {
                qCWarning(lcCse) << "Error invalid server public key";
                forgetSensitiveData(account);
                getPublicKeyFromServer(account);
                return;
            }
        } else {
            qCWarning(lcCse) << "Error while requesting server public key: " << retCode;
            failedToInitialize(account);
            return;
        }
    });
    job->start();
}

FolderMetadata::FolderMetadata(AccountPtr account)
    : _account(account)
{
    qCInfo(lcCseMetadata()) << "Setupping Empty Metadata";
    setupEmptyMetadata();
}

FolderMetadata::FolderMetadata(AccountPtr account,
                               RequiredMetadataVersion requiredMetadataVersion,
                               const QByteArray& metadata,
                               int statusCode)
    : _account(std::move(account))
    , _requiredMetadataVersion(requiredMetadataVersion)
{
    if (metadata.isEmpty() || statusCode == 404) {
        qCInfo(lcCseMetadata()) << "Setupping Empty Metadata";
        setupEmptyMetadata();
    } else {
        qCInfo(lcCseMetadata()) << "Setting up existing metadata";
        setupExistingMetadata(metadata);
    }
}

void FolderMetadata::setupExistingMetadata(const QByteArray& metadata)
{
    /* This is the json response from the server, it contains two extra objects that we are *not* interested.
     * ocs and data.
     */
    QJsonDocument doc = QJsonDocument::fromJson(metadata);
    qCInfo(lcCseMetadata()) << doc.toJson(QJsonDocument::Compact);

    // The metadata is being retrieved as a string stored in a json.
    // This *seems* to be broken but the RFC doesn't explicit how it wants.
    // I'm currently unsure if this is error on my side or in the server implementation.
    // And because inside of the meta-data there's an object called metadata, without '-'
    // make it really different.

    QString metaDataStr = doc.object()["ocs"]
                              .toObject()["data"]
                              .toObject()["meta-data"]
                              .toString();

    QJsonDocument metaDataDoc = QJsonDocument::fromJson(metaDataStr.toLocal8Bit());
    QJsonObject metadataObj = metaDataDoc.object()["metadata"].toObject();
    QJsonObject metadataKeys = metadataObj["metadataKeys"].toObject();

    const auto metadataKeyFromJson = metadataObj[metadataKeyJsonKey].toString().toLocal8Bit();
    if (!metadataKeyFromJson.isEmpty()) {
        const auto decryptedMetadataKeyBase64 = decryptData(metadataKeyFromJson);
        if (!decryptedMetadataKeyBase64.isEmpty()) {
            _metadataKey = QByteArray::fromBase64(decryptedMetadataKeyBase64);
        }
    }

    auto migratedMetadata = false;
    if (_metadataKey.isEmpty() && _requiredMetadataVersion != RequiredMetadataVersion::Version1_2) {
        qCDebug(lcCse()) << "Migrating from v1.1 to v1.2";
        migratedMetadata = true;

        if (metadataKeys.isEmpty()) {
            qCDebug(lcCse()) << "Could not migrate. No metadata keys found!";
            return;
        }

        const auto lastMetadataKey = metadataKeys.keys().last();
        const auto decryptedMetadataKeyBase64 = decryptData(metadataKeys.value(lastMetadataKey).toString().toLocal8Bit());
        if (!decryptedMetadataKeyBase64.isEmpty()) {
            _metadataKey = QByteArray::fromBase64(decryptedMetadataKeyBase64);
        }
    }

    if (_metadataKey.isEmpty()) {
        qCDebug(lcCse()) << "Could not setup existing metadata with missing metadataKeys!";
        return;
    }

    const auto sharing = metadataObj["sharing"].toString().toLocal8Bit();
    const auto files = metaDataDoc.object()["files"].toObject();
    const auto metadataKey = metaDataDoc.object()["metadata"].toObject()["metadataKey"].toString().toUtf8();
    const auto metadataKeyChecksum = metaDataDoc.object()["metadata"].toObject()["checksum"].toString().toUtf8();

    _fileDrop = metaDataDoc.object().value("filedrop").toObject();
    // for unit tests
    _fileDropFromServer = metaDataDoc.object().value("filedrop").toObject();

    // Iterate over the document to store the keys. I'm unsure that the keys are in order,
    // perhaps it's better to store a map instead of a vector, perhaps this just doesn't matter.

    // Cool, We actually have the key, we can decrypt the rest of the metadata.
    qCDebug(lcCse) << "Sharing: " << sharing;
    if (sharing.size()) {
        auto sharingDecrypted = decryptJsonObject(sharing, _metadataKey);
        qCDebug(lcCse) << "Sharing Decrypted" << sharingDecrypted;

        // Sharing is also a JSON object, so extract it and populate.
        auto sharingDoc = QJsonDocument::fromJson(sharingDecrypted);
        auto sharingObj = sharingDoc.object();
        for (auto it = sharingObj.constBegin(), end = sharingObj.constEnd(); it != end; it++) {
            _sharing.push_back({it.key(), it.value().toString()});
        }
    } else {
        qCDebug(lcCse) << "Skipping sharing section since it is empty";
    }

    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        EncryptedFile file;
        file.encryptedFilename = it.key();

        const auto fileObj = it.value().toObject();
        file.authenticationTag = QByteArray::fromBase64(fileObj["authenticationTag"].toString().toLocal8Bit());
        file.initializationVector = QByteArray::fromBase64(fileObj["initializationVector"].toString().toLocal8Bit());

        // Decrypt encrypted part
        const auto encryptedFile = fileObj["encrypted"].toString().toLocal8Bit();
        const auto decryptedFile = decryptJsonObject(encryptedFile, _metadataKey);
        const auto decryptedFileDoc = QJsonDocument::fromJson(decryptedFile);

        const auto decryptedFileObj = decryptedFileDoc.object();

        if (decryptedFileObj["filename"].toString().isEmpty()) {
            qCDebug(lcCse) << "decrypted metadata" << decryptedFileDoc.toJson(QJsonDocument::Indented);
            qCWarning(lcCse) << "skipping encrypted file" << file.encryptedFilename << "metadata has an empty file name";
            continue;
        }

        file.originalFilename = decryptedFileObj["filename"].toString();
        file.encryptionKey = QByteArray::fromBase64(decryptedFileObj["key"].toString().toLocal8Bit());
        file.mimetype = decryptedFileObj["mimetype"].toString().toLocal8Bit();

        // In case we wrongly stored "inode/directory" we try to recover from it
        if (file.mimetype == QByteArrayLiteral("inode/directory")) {
            file.mimetype = QByteArrayLiteral("httpd/unix-directory");
        }

        qCDebug(lcCseMetadata) << "encrypted file" << decryptedFileObj["filename"].toString() << decryptedFileObj["key"].toString() << it.key();

        _files.push_back(file);
    }

    if (!migratedMetadata && !checkMetadataKeyChecksum(metadataKey, metadataKeyChecksum)) {
        qCInfo(lcCseMetadata) << "checksum comparison failed" << "server value" << metadataKeyChecksum << "client value" << computeMetadataKeyChecksum(metadataKey);
        if (_account->shouldSkipE2eeMetadataChecksumValidation()) {
            qCDebug(lcCseMetadata) << "shouldSkipE2eeMetadataChecksumValidation is set. Allowing invalid checksum until next sync.";
            _encryptedMetadataNeedUpdate = true;
        } else {
            _metadataKey.clear();
            _files.clear();
            return;
        }
    }

    // decryption finished, create new metadata key to be used for encryption
    _metadataKey = EncryptionHelper::generateRandom(metadataKeySize);
    _isMetadataSetup = true;

    if (migratedMetadata) {
        _encryptedMetadataNeedUpdate = true;
    }
}

// RSA/ECB/OAEPWithSHA-256AndMGF1Padding using private / public key.
QByteArray FolderMetadata::encryptData(const QByteArray& data) const
{
    Bio publicKeyBio;
    QByteArray publicKeyPem = _account->e2e()->_publicKey.toPem();
    BIO_write(publicKeyBio, publicKeyPem.constData(), publicKeyPem.size());
    auto publicKey = ClientSideEncryption::PKey::readPublicKey(publicKeyBio);

    // The metadata key is binary so base64 encode it first
    return EncryptionHelper::encryptStringAsymmetric(publicKey, data.toBase64());
}

QByteArray FolderMetadata::decryptData(const QByteArray &data) const
{
    Bio privateKeyBio;
    QByteArray privateKeyPem = _account->e2e()->_privateKey;
    BIO_write(privateKeyBio, privateKeyPem.constData(), privateKeyPem.size());
    auto key = ClientSideEncryption::PKey::readPrivateKey(privateKeyBio);

    // Also base64 decode the result
    QByteArray decryptResult = EncryptionHelper::decryptStringAsymmetric(key, QByteArray::fromBase64(data));

    if (decryptResult.isEmpty())
    {
        qCDebug(lcCse()) << "ERROR. Could not decrypt the metadata key";
        return {};
    }
    return QByteArray::fromBase64(decryptResult);
}

QByteArray FolderMetadata::decryptDataUsingKey(const QByteArray &data,
                                               const QByteArray &key,
                                               const QByteArray &authenticationTag,
                                               const QByteArray &initializationVector) const
{
    // Also base64 decode the result
    QByteArray decryptResult = EncryptionHelper::decryptStringSymmetric(QByteArray::fromBase64(key),
                                                                        data + '|' + initializationVector + '|' + authenticationTag);

    if (decryptResult.isEmpty())
    {
        qCDebug(lcCse()) << "ERROR. Could not decrypt";
        return {};
    }

    return decryptResult;
}

// AES/GCM/NoPadding (128 bit key size)
QByteArray FolderMetadata::encryptJsonObject(const QByteArray& obj, const QByteArray pass) const
{
    return EncryptionHelper::encryptStringSymmetric(pass, obj);
}

QByteArray FolderMetadata::decryptJsonObject(const QByteArray& encryptedMetadata, const QByteArray& pass) const
{
    return EncryptionHelper::decryptStringSymmetric(pass, encryptedMetadata);
}

bool FolderMetadata::checkMetadataKeyChecksum(const QByteArray &metadataKey,
                                              const QByteArray &metadataKeyChecksum) const
{
    const auto referenceMetadataKeyValue = computeMetadataKeyChecksum(metadataKey);

    return referenceMetadataKeyValue == metadataKeyChecksum;
}

QByteArray FolderMetadata::computeMetadataKeyChecksum(const QByteArray &metadataKey) const
{
    auto hashAlgorithm = QCryptographicHash{QCryptographicHash::Sha256};

    hashAlgorithm.addData(_account->e2e()->_mnemonic.remove(' ').toUtf8());
    auto sortedFiles = _files;
    std::sort(sortedFiles.begin(), sortedFiles.end(), [] (const auto &first, const auto &second) {
        return first.encryptedFilename < second.encryptedFilename;
    });
    for (const auto &singleFile : sortedFiles) {
        hashAlgorithm.addData(singleFile.encryptedFilename.toUtf8());
    }
    hashAlgorithm.addData(metadataKey);

    return hashAlgorithm.result().toHex();
}

bool FolderMetadata::isMetadataSetup() const
{
    return _isMetadataSetup;
}

void FolderMetadata::setupEmptyMetadata() {
    qCDebug(lcCse) << "Settint up empty metadata";
    _metadataKey = EncryptionHelper::generateRandom(metadataKeySize);
    QString publicKey = _account->e2e()->_publicKey.toPem().toBase64();
    QString displayName = _account->displayName();

    _sharing.append({displayName, publicKey});

    _isMetadataSetup = true;
}

QByteArray FolderMetadata::encryptedMetadata() const {
    qCDebug(lcCse) << "Generating metadata";

    if (_metadataKey.isEmpty()) {
        qCDebug(lcCse) << "Metadata generation failed! Empty metadata key!";
        return {};
    }
    const auto version = _account->capabilities().clientSideEncryptionVersion();
    const auto encryptedMetadataKey = encryptData(_metadataKey.toBase64());
    QJsonObject metadata{
        {"version", version},
        {metadataKeyJsonKey, QJsonValue::fromVariant(encryptedMetadataKey)},
        {"checksum", QJsonValue::fromVariant(computeMetadataKeyChecksum(encryptedMetadataKey))},
    };

    QJsonObject files;
    for (auto it = _files.constBegin(), end = _files.constEnd(); it != end; it++) {
        QJsonObject encrypted;
        encrypted.insert("key", QString(it->encryptionKey.toBase64()));
        encrypted.insert("filename", it->originalFilename);
        encrypted.insert("mimetype", QString(it->mimetype));
        QJsonDocument encryptedDoc;
        encryptedDoc.setObject(encrypted);

        QString encryptedEncrypted = encryptJsonObject(encryptedDoc.toJson(QJsonDocument::Compact), _metadataKey);
        if (encryptedEncrypted.isEmpty()) {
            qCDebug(lcCse) << "Metadata generation failed!";
        }
        QJsonObject file;
        file.insert("encrypted", encryptedEncrypted);
        file.insert("initializationVector", QString(it->initializationVector.toBase64()));
        file.insert("authenticationTag", QString(it->authenticationTag.toBase64()));

        files.insert(it->encryptedFilename, file);
    }

    QJsonObject filedrop;
    for (auto fileDropIt = _fileDrop.constBegin(), end = _fileDrop.constEnd(); fileDropIt != end; ++fileDropIt) {
        filedrop.insert(fileDropIt.key(), fileDropIt.value());
    }

    auto metaObject = QJsonObject{
                                  {"metadata", metadata},
                                  };

    if (files.count()) {
        metaObject.insert("files", files);
    }

    if (filedrop.count()) {
        metaObject.insert("filedrop", filedrop);
    }

    QJsonDocument internalMetadata;
    internalMetadata.setObject(metaObject);
    return internalMetadata.toJson();
}

void FolderMetadata::addEncryptedFile(const EncryptedFile &f) {

    for (int i = 0; i < _files.size(); i++) {
        if (_files.at(i).originalFilename == f.originalFilename) {
            _files.removeAt(i);
            break;
        }
    }
    _files.append(f);
}

void FolderMetadata::removeEncryptedFile(const EncryptedFile &f)
{
    for (int i = 0; i < _files.size(); i++) {
        if (_files.at(i).originalFilename == f.originalFilename) {
            _files.removeAt(i);
            break;
        }
    }
}

void FolderMetadata::removeAllEncryptedFiles()
{
    _files.clear();
}

QVector<EncryptedFile> FolderMetadata::files() const {
    return _files;
}

bool FolderMetadata::isFileDropPresent() const
{
    return _fileDrop.size() > 0;
}

bool FolderMetadata::encryptedMetadataNeedUpdate() const
{
    return _encryptedMetadataNeedUpdate;
}

bool FolderMetadata::moveFromFileDropToFiles()
{
    if (_fileDrop.isEmpty()) {
        return false;
    }

    for (auto it = _fileDrop.begin(); it != _fileDrop.end(); ) {
        const auto fileObject = it.value().toObject();

        const auto decryptedKey = decryptData(fileObject["encryptedKey"].toString().toLocal8Bit());
        const auto decryptedAuthenticationTag = fileObject["encryptedTag"].toString().toLocal8Bit();
        const auto decryptedInitializationVector = fileObject["encryptedInitializationVector"].toString().toLocal8Bit();

        if (decryptedKey.isEmpty() || decryptedAuthenticationTag.isEmpty() || decryptedInitializationVector.isEmpty()) {
            qCDebug(lcCseMetadata) << "failed to decrypt filedrop entry" << it.key();
            continue;
        }

        const auto encryptedFile = fileObject["encrypted"].toString().toLocal8Bit();
        const auto decryptedFile = decryptDataUsingKey(encryptedFile, decryptedKey, decryptedAuthenticationTag, decryptedInitializationVector);
        const auto decryptedFileDocument = QJsonDocument::fromJson(decryptedFile);
        const auto decryptedFileObject = decryptedFileDocument.object();
        const auto authenticationTag = QByteArray::fromBase64(fileObject["authenticationTag"].toString().toLocal8Bit());
        const auto initializationVector = QByteArray::fromBase64(fileObject["initializationVector"].toString().toLocal8Bit());

        EncryptedFile file;
        file.encryptedFilename = it.key();
        file.authenticationTag = authenticationTag;
        file.initializationVector = initializationVector;

        file.originalFilename = decryptedFileObject["filename"].toString();
        file.encryptionKey = QByteArray::fromBase64(decryptedFileObject["key"].toString().toLocal8Bit());
        file.mimetype = decryptedFileObject["mimetype"].toString().toLocal8Bit();

        // In case we wrongly stored "inode/directory" we try to recover from it
        if (file.mimetype == QByteArrayLiteral("inode/directory")) {
            file.mimetype = QByteArrayLiteral("httpd/unix-directory");
        }

        _files.push_back(file);
        it = _fileDrop.erase(it);
    }

    return true;
}

QJsonObject FolderMetadata::fileDrop() const
{
    return _fileDropFromServer;
}

bool EncryptionHelper::fileEncryption(const QByteArray &key, const QByteArray &iv, QFile *input, QFile *output, QByteArray& returnTag)
{
    if (!input->open(QIODevice::ReadOnly)) {
        qCDebug(lcCse) << "Could not open input file for reading" << input->errorString();
    }
    if (!output->open(QIODevice::WriteOnly)) {
        qCDebug(lcCse) << "Could not oppen output file for writing" << output->errorString();
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
}
