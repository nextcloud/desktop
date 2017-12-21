#include "clientsideencryption.h"
#include "account.h"
#include "capabilities.h"
#include "networkjobs.h"
#include "clientsideencryptionjobs.h"
#include "theme.h"
#include "creds/abstractcredentials.h"

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/engine.h>

#include <map>

#include <cstdio>

#include <QDebug>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QXmlStreamNamespaceDeclaration>
#include <QStack>
#include <QInputDialog>
#include <QLineEdit>
#include <QIODevice>

#include <keychain.h>

#include "wordlist.h"

QDebug operator<<(QDebug out, const std::string& str)
{
    out << QString::fromStdString(str);
    return out;
}

using namespace QKeychain;

namespace OCC
{

Q_LOGGING_CATEGORY(lcCse, "sync.clientsideencryption", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCseDecryption, "e2e", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCseMetadata, "metadata", QtInfoMsg)

QString baseUrl(){
    return QStringLiteral("ocs/v2.php/apps/end_to_end_encryption/api/v1/");
}

namespace {
    const char e2e_cert[] = "_e2e-certificate";
    const char e2e_private[] = "_e2e-private";
    const char e2e_mnemonic[] = "_e2e-mnemonic";
} // ns

namespace {
    void handleErrors(void)
    {
        ERR_print_errors_fp(stdout); // This line is not printing anything.
        fflush(stdout);
    }
}

QByteArray EncryptionHelper::generateRandomString(int size)
{
   const QByteArray possibleCharacters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

   QByteArray randomString(size, '\0');
   for(int i=0; i < size; ++i)
   {
       int index = qrand() % possibleCharacters.length();
       randomString[i] = possibleCharacters.at(index);
   }
   return randomString;
}

QByteArray EncryptionHelper::generateRandom(int size)
{
    unsigned char *tmp = (unsigned char *)malloc(sizeof(unsigned char) * size);

    int ret = RAND_bytes(tmp, size);
    if (ret != 1) {
        qCInfo(lcCse()) << "Random byte generation failed!";
        // Error out?
    }

    QByteArray result((const char *)tmp, size);
    free(tmp);

    return result;
}

QByteArray EncryptionHelper::generatePassword(const QString& wordlist, const QByteArray& salt) {
    qCInfo(lcCse()) << "Start encryption key generation!";

    const int iterationCount = 1024;
    const int keyStrength = 256;
    const int keyLength = keyStrength/8;

    unsigned char secretKey[keyLength];

    int ret = PKCS5_PBKDF2_HMAC_SHA1(
        wordlist.toLocal8Bit().constData(),     // const char *password,
        wordlist.size(),                        // int password length,
        (const unsigned char *)salt.constData(),                       // const unsigned char *salt,
        salt.size(),                            // int saltlen,
        iterationCount,                         // int iterations,
        keyLength,                              // int keylen,
        secretKey                               // unsigned char *out
    );

    if (ret != 1) {
        qCInfo(lcCse()) << "Failed to generate encryption key";
        // Error out?
    }

    qCInfo(lcCse()) << "Encryption key generated!";

    QByteArray password((const char *)secretKey, keyLength);
    return password;
}

QByteArray EncryptionHelper::encryptPrivateKey(
        const QByteArray& key,
        const QByteArray& privateKey,
        const QByteArray& salt
        ) {

    QByteArray iv = generateRandom(12);

    EVP_CIPHER_CTX *ctx;
    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        qCInfo(lcCse()) << "Error creating cipher";
        handleErrors();
    }

    /* Initialise the decryption operation. */
    if(!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        qCInfo(lcCse()) << "Error initializing context with aes_256";
        handleErrors();
    }

    // No padding
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), NULL)) {
        qCInfo(lcCse()) << "Error setting iv length";
        handleErrors();
    }

    /* Initialise key and IV */
    if(!EVP_EncryptInit_ex(ctx, NULL, NULL, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv";
        handleErrors();
    }

    // We write the base64 encoded private key
    QByteArray privateKeyB64 = privateKey.toBase64();

    // Make sure we have enough room in the cipher text
    unsigned char *ctext = (unsigned char *)malloc(sizeof(unsigned char) * (privateKeyB64.size() + 32));

    // Do the actual encryption
    int len = 0;
    if(!EVP_EncryptUpdate(ctx, ctext, &len, (unsigned char *)privateKeyB64.constData(), privateKeyB64.size())) {
        qCInfo(lcCse()) << "Error encrypting";
        handleErrors();
    }

    int clen = len;

    /* Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if(1 != EVP_EncryptFinal_ex(ctx, ctext + len, &len)) {
        qCInfo(lcCse()) << "Error finalizing encryption";
        handleErrors();
    }
    clen += len;

    /* Get the tag */
    unsigned char *tag = (unsigned char *)calloc(sizeof(unsigned char), 16);
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
        qCInfo(lcCse()) << "Error getting the tag";
        handleErrors();
    }

    QByteArray cipherTXT((char *)ctext, clen);
    cipherTXT.append((char *)tag, 16);

    QByteArray result = cipherTXT.toBase64();
    result += "fA==";
    result += iv.toBase64();
    result += "fA==";
    result += salt.toBase64();

    return result;
}

QByteArray EncryptionHelper::decryptPrivateKey(const QByteArray& key, const QByteArray& data) {
    qCInfo(lcCse()) << "decryptStringSymmetric key: " << key;
    qCInfo(lcCse()) << "decryptStringSymmetric data: " << data;

    int sep = data.indexOf("fA==");
    qCInfo(lcCse()) << "sep at" << sep;

    QByteArray cipherTXT64 = data.left(sep);
    QByteArray ivB64 = data.right(data.size() - sep - 4);

    qCInfo(lcCse()) << "decryptStringSymmetric cipherTXT: " << cipherTXT64;
    qCInfo(lcCse()) << "decryptStringSymmetric IV: " << ivB64;

    QByteArray cipherTXT = QByteArray::fromBase64(cipherTXT64);
    QByteArray iv = QByteArray::fromBase64(ivB64);

    QByteArray tag = cipherTXT.right(16);
    cipherTXT.chop(16);

    // Init
    EVP_CIPHER_CTX *ctx;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        qCInfo(lcCse()) << "Error creating cipher";
        return QByteArray();
    }

    /* Initialise the decryption operation. */
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        qCInfo(lcCse()) << "Error initialising context with aes 256";
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), NULL)) {
        qCInfo(lcCse()) << "Error setting IV size";
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    /* Initialise key and IV */
    if(!EVP_DecryptInit_ex(ctx, NULL, NULL, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv";
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    unsigned char *ptext = (unsigned char *)calloc(cipherTXT.size() + 16, sizeof(unsigned char));
    int plen;

    /* Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if(!EVP_DecryptUpdate(ctx, ptext, &plen, (unsigned char *)cipherTXT.constData(), cipherTXT.size())) {
        qCInfo(lcCse()) << "Could not decrypt";
        EVP_CIPHER_CTX_free(ctx);
        free(ptext);
        return QByteArray();
    }

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(), (unsigned char *)tag.constData())) {
        qCInfo(lcCse()) << "Could not set tag";
        EVP_CIPHER_CTX_free(ctx);
        free(ptext);
        return QByteArray();
    }

    /* Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    int len = plen;
    if (EVP_DecryptFinal_ex(ctx, ptext + plen, &len) == 0) {
        qCInfo(lcCse()) << "Tag did not match!";
        EVP_CIPHER_CTX_free(ctx);
        free(ptext);
        return QByteArray();
    }

    QByteArray result((char *)ptext, plen);

    free(ptext);
    EVP_CIPHER_CTX_free(ctx);

    return QByteArray::fromBase64(result);
}

QByteArray EncryptionHelper::decryptStringSymmetric(const QByteArray& key, const QByteArray& data) {
    qCInfo(lcCse()) << "decryptStringSymmetric key: " << key;
    qCInfo(lcCse()) << "decryptStringSymmetric data: " << data;

    int sep = data.indexOf("fA==");
    qCInfo(lcCse()) << "sep at" << sep;

    QByteArray cipherTXT64 = data.left(sep);
    QByteArray ivB64 = data.right(data.size() - sep - 4);

    qCInfo(lcCse()) << "decryptStringSymmetric cipherTXT: " << cipherTXT64;
    qCInfo(lcCse()) << "decryptStringSymmetric IV: " << ivB64;

    QByteArray cipherTXT = QByteArray::fromBase64(cipherTXT64);
    QByteArray iv = QByteArray::fromBase64(ivB64);

    QByteArray tag = cipherTXT.right(16);
    cipherTXT.chop(16);

    // Init
    EVP_CIPHER_CTX *ctx;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        qCInfo(lcCse()) << "Error creating cipher";
        return QByteArray();
    }

    /* Initialise the decryption operation. */
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
        qCInfo(lcCse()) << "Error initialising context with aes 128";
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), NULL)) {
        qCInfo(lcCse()) << "Error setting IV size";
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    /* Initialise key and IV */
    if(!EVP_DecryptInit_ex(ctx, NULL, NULL, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv";
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    unsigned char *ptext = (unsigned char *)calloc(cipherTXT.size() + 16, sizeof(unsigned char));
    int plen;

    /* Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if(!EVP_DecryptUpdate(ctx, ptext, &plen, (unsigned char *)cipherTXT.constData(), cipherTXT.size())) {
        qCInfo(lcCse()) << "Could not decrypt";
        EVP_CIPHER_CTX_free(ctx);
        free(ptext);
        return QByteArray();
    }

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(), (unsigned char *)tag.constData())) {
        qCInfo(lcCse()) << "Could not set tag";
        EVP_CIPHER_CTX_free(ctx);
        free(ptext);
        return QByteArray();
    }

    /* Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    int len = plen;
    if (EVP_DecryptFinal_ex(ctx, ptext + plen, &len) == 0) {
        qCInfo(lcCse()) << "Tag did not match!";
        EVP_CIPHER_CTX_free(ctx);
        free(ptext);
        return QByteArray();
    }

    QByteArray result((char *)ptext, plen);

    free(ptext);
    EVP_CIPHER_CTX_free(ctx);

    return result;
}

QByteArray EncryptionHelper::encryptStringSymmetric(const QByteArray& key, const QByteArray& data) {
    QByteArray iv = generateRandom(16);

    EVP_CIPHER_CTX *ctx;
    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        qCInfo(lcCse()) << "Error creating cipher";
        handleErrors();
    }

    /* Initialise the decryption operation. */
    if(!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
        qCInfo(lcCse()) << "Error initializing context with aes_128";
        handleErrors();
    }

    // No padding
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), NULL)) {
        qCInfo(lcCse()) << "Error setting iv length";
        handleErrors();
    }

    /* Initialise key and IV */
    if(!EVP_EncryptInit_ex(ctx, NULL, NULL, (unsigned char *)key.constData(), (unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Error initialising key and iv";
        handleErrors();
    }

    // We write the data base64 encoded
    QByteArray dataB64 = data.toBase64();

    // Make sure we have enough room in the cipher text
    unsigned char *ctext = (unsigned char *)malloc(sizeof(unsigned char) * (dataB64.size() + 16));

    // Do the actual encryption
    int len = 0;
    if(!EVP_EncryptUpdate(ctx, ctext, &len, (unsigned char *)dataB64.constData(), dataB64.size())) {
        qCInfo(lcCse()) << "Error encrypting";
        handleErrors();
    }

    int clen = len;

    /* Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if(1 != EVP_EncryptFinal_ex(ctx, ctext + len, &len)) {
        qCInfo(lcCse()) << "Error finalizing encryption";
        handleErrors();
    }
    clen += len;

    /* Get the tag */
    unsigned char *tag = (unsigned char *)calloc(sizeof(unsigned char), 16);
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
        qCInfo(lcCse()) << "Error getting the tag";
        handleErrors();
    }

    QByteArray cipherTXT((char *)ctext, clen);
    cipherTXT.append((char *)tag, 16);

    QByteArray result = cipherTXT.toBase64();
    result += "fA==";
    result += iv.toBase64();

    return result;
}

QByteArray EncryptionHelper::decryptStringAsymmetric(EVP_PKEY *privateKey, const QByteArray& data) {
    int err = -1;

    qCInfo(lcCseDecryption()) << "Start to work the decryption.";
    auto ctx = EVP_PKEY_CTX_new(privateKey, ENGINE_get_default_RSA());
    if (!ctx) {
        qCInfo(lcCseDecryption()) << "Could not create the PKEY context.";
        handleErrors();
        exit(1);
    }

    err = EVP_PKEY_decrypt_init(ctx);
    if (err <= 0) {
        qCInfo(lcCseDecryption()) << "Could not init the decryption of the metadata";
        handleErrors();
        exit(1);
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        qCInfo(lcCseDecryption()) << "Error setting the encryption padding.";
        handleErrors();
        exit(1);
    }

    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCseDecryption()) << "Error setting OAEP SHA 256";
        handleErrors();
        exit(1);
    }

    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        qCInfo(lcCseDecryption()) << "Error setting MGF1 padding";
        handleErrors();
        exit(1);
    }

    size_t outlen = 0;
    err = EVP_PKEY_decrypt(ctx, NULL, &outlen,  (unsigned char *)data.constData(), data.size());
    if (err <= 0) {
        qCInfo(lcCseDecryption()) << "Could not determine the buffer length";
        handleErrors();
        exit(1);
    } else {
        qCInfo(lcCseDecryption()) << "Size of output is: " << outlen;
        qCInfo(lcCseDecryption()) << "Size of data is: " << data.size();
    }

    unsigned char *out = (unsigned char *) OPENSSL_malloc(outlen);
    if (!out) {
        qCInfo(lcCseDecryption()) << "Could not alloc space for the decrypted metadata";
        handleErrors();
        exit(1);
    }

    if (EVP_PKEY_decrypt(ctx, out, &outlen, (unsigned char *)data.constData(), data.size()) <= 0) {
        qCInfo(lcCseDecryption()) << "Could not decrypt the data.";
        ERR_print_errors_fp(stdout); // This line is not printing anything.
        exit(1);
    } else {
        qCInfo(lcCseDecryption()) << "data decrypted successfully";
    }

    const auto ret = std::string((char*) out, outlen);
    QByteArray raw((const char*) out, outlen);
    qCInfo(lcCse()) << raw;
    return raw;
}

QByteArray EncryptionHelper::encryptStringAsymmetric(EVP_PKEY *publicKey, const QByteArray& data) {
    int err = -1;

    auto ctx = EVP_PKEY_CTX_new(publicKey, ENGINE_get_default_RSA());
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
    if (EVP_PKEY_encrypt(ctx, NULL, &outLen, (unsigned char *)data.constData(), data.size()) != 1) {
        qCInfo(lcCse()) << "Error retrieving the size of the encrypted data";
        exit(1);
    } else {
        qCInfo(lcCse()) << "Encrption Length:" << outLen;
    }

    unsigned char *out = (uchar*) OPENSSL_malloc(outLen);
    if (!out) {
        qCInfo(lcCse()) << "Error requesting memory for the encrypted contents";
        exit(1);
    }

    if (EVP_PKEY_encrypt(ctx, out, &outLen, (unsigned char *)data.constData(), data.size()) != 1) {
        qCInfo(lcCse()) << "Could not encrypt key." << err;
        exit(1);
    }

    // Transform the encrypted data into base64.
    QByteArray raw((const char*) out, outLen);
    qCInfo(lcCse()) << raw.toBase64();
    return raw.toBase64();
}

QByteArray EncryptionHelper::BIO2ByteArray(BIO *b) {
    int pending = BIO_ctrl_pending(b);
    char *tmp = (char *)calloc(pending+1, sizeof(char));
    BIO_read(b, tmp, pending);

    QByteArray res(tmp, pending);
    free(tmp);

    return res;
}

ClientSideEncryption::ClientSideEncryption()
{
}

void ClientSideEncryption::setAccount(AccountPtr account)
{
    _account = account;
}

void ClientSideEncryption::initialize()
{
    qCInfo(lcCse()) << "Initializing";
    if (!_account->capabilities().clientSideEncryptionAvaliable()) {
        qCInfo(lcCse()) << "No Client side encryption avaliable on server.";
        emit initializationFinished();
    }

    fetchFromKeyChain();
}

void ClientSideEncryption::fetchFromKeyChain() {
    const QString kck = AbstractCredentials::keychainKey(
                _account->url().toString(),
                _account->credentials()->user() + e2e_cert,
                _account->id()
    );

    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::publicKeyFetched);
    job->start();
}

void ClientSideEncryption::publicKeyFetched(Job *incoming) {
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob *>(incoming);

    // Error or no valid public key error out
    if (readJob->error() != NoError || readJob->binaryData().length() == 0) {
        getPublicKeyFromServer();
        return;
    }

    _certificate = QSslCertificate(readJob->binaryData(), QSsl::Pem);

    if (_certificate.isNull()) {
        getPublicKeyFromServer();
        return;
    }

    _publicKey = _certificate.publicKey();

    qCInfo(lcCse()) << "Public key fetched from keychain";

    const QString kck = AbstractCredentials::keychainKey(
                _account->url().toString(),
                _account->credentials()->user() + e2e_private,
                _account->id()
    );

    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::privateKeyFetched);
    job->start();
}

void ClientSideEncryption::setFolderEncryptedStatus(const QString& folder, bool status)
{
    qDebug() << "Setting folder" << folder << "as encrypted" << status;
    _folder2encryptedStatus[folder] = status;
}

void ClientSideEncryption::privateKeyFetched(Job *incoming) {
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob *>(incoming);

    // Error or no valid public key error out
    if (readJob->error() != NoError || readJob->binaryData().length() == 0) {
        _certificate = QSslCertificate();
        _publicKey = QSslKey();
        getPublicKeyFromServer();
        return;
    }

    _privateKey = QSslKey(readJob->binaryData(), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);

    if (_privateKey.isNull()) {
        getPrivateKeyFromServer();
        return;
    }

    qCInfo(lcCse()) << "Private key fetched from keychain";

    const QString kck = AbstractCredentials::keychainKey(
                _account->url().toString(),
                _account->credentials()->user() + e2e_mnemonic,
                _account->id()
    );

    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, this, &ClientSideEncryption::mnemonicKeyFetched);
    job->start();
}

void ClientSideEncryption::mnemonicKeyFetched(QKeychain::Job *incoming) {
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob *>(incoming);

    // Error or no valid public key error out
    if (readJob->error() != NoError || readJob->textData().length() == 0) {
        _certificate = QSslCertificate();
        _publicKey = QSslKey();
        _privateKey = QSslKey();
        getPublicKeyFromServer();
        return;
    }

    _mnemonic = readJob->textData();

    qCInfo(lcCse()) << "Mnemonic key fetched from keychain";

    emit initializationFinished();
}

void ClientSideEncryption::writePrivateKey() {
    const QString kck = AbstractCredentials::keychainKey(
                _account->url().toString(),
                _account->credentials()->user() + e2e_private,
                _account->id()
    );

    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setBinaryData(_privateKey.toPem());
    connect(job, &WritePasswordJob::finished, [this](Job *incoming) {
        Q_UNUSED(incoming);
        qCInfo(lcCse()) << "Private key stored in keychain";
    });
    job->start();
}

void ClientSideEncryption::writeCertificate() {
    const QString kck = AbstractCredentials::keychainKey(
                _account->url().toString(),
                _account->credentials()->user() + e2e_cert,
                _account->id()
    );

    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setBinaryData(_certificate.toPem());
    connect(job, &WritePasswordJob::finished, [this](Job *incoming) {
        Q_UNUSED(incoming);
        qCInfo(lcCse()) << "Certificate stored in keychain";
    });
    job->start();
}

void ClientSideEncryption::writeMnemonic() {
    const QString kck = AbstractCredentials::keychainKey(
                _account->url().toString(),
                _account->credentials()->user() + e2e_mnemonic,
                _account->id()
    );

    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setTextData(_mnemonic);
    connect(job, &WritePasswordJob::finished, [this](Job *incoming) {
        Q_UNUSED(incoming);
        qCInfo(lcCse()) << "Mnemonic stored in keychain";
    });
    job->start();
}

void ClientSideEncryption::forgetSensitiveData()
{
    _privateKey = QSslKey();
    _certificate = QSslCertificate();
    _publicKey = QSslKey();
    _mnemonic = QString();

    auto startDeleteJob = [this](QString user) {
        DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
        job->setInsecureFallback(false);
        job->setKey(AbstractCredentials::keychainKey(_account->url().toString(), user, _account->id()));
        job->start();
    };

    auto user = _account->credentials()->user();
    startDeleteJob(user + e2e_private);
    startDeleteJob(user + e2e_cert);
    startDeleteJob(user + e2e_mnemonic);
}

bool ClientSideEncryption::hasPrivateKey() const
{
    return !_privateKey.isNull();
}

bool ClientSideEncryption::hasPublicKey() const
{
    return !_publicKey.isNull();
}

void ClientSideEncryption::generateKeyPair()
{
    // AES/GCM/NoPadding,
    // metadataKeys with RSA/ECB/OAEPWithSHA-256AndMGF1Padding
    qCInfo(lcCse()) << "No public key, generating a pair.";
    const int rsaKeyLen = 2048;

    EVP_PKEY *localKeyPair = nullptr;

    // Init RSA
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);

    if(EVP_PKEY_keygen_init(ctx) <= 0) {
        qCInfo(lcCse()) << "Couldn't initialize the key generator";
        return;
    }

    if(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, rsaKeyLen) <= 0) {
        qCInfo(lcCse()) << "Couldn't initialize the key generator bits";
        return;
    }

    if(EVP_PKEY_keygen(ctx, &localKeyPair) <= 0) {
        qCInfo(lcCse()) << "Could not generate the key";
        return;
    }
    EVP_PKEY_CTX_free(ctx);
    qCInfo(lcCse()) << "Key correctly generated";
    qCInfo(lcCse()) << "Storing keys locally";

    BIO *privKey = BIO_new(BIO_s_mem());
    if (PEM_write_bio_PrivateKey(privKey, localKeyPair, NULL, NULL, 0, NULL, NULL) <= 0) {
        qCInfo(lcCse()) << "Could not read private key from bio.";
        return;
    }
    QByteArray key = EncryptionHelper::BIO2ByteArray(privKey);
    _privateKey = QSslKey(key, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);

    qCInfo(lcCse()) << "Keys generated correctly, sending to server.";
    generateCSR(localKeyPair);
}

void ClientSideEncryption::generateCSR(EVP_PKEY *keyPair)
{
    // OpenSSL expects const char.
    auto cnArray = _account->davUser().toLocal8Bit();
    qCInfo(lcCse()) << "Getting the following array for the account Id" << cnArray;

    auto certParams = std::map<const char *, const char*>{
      {"C", "DE"},
      {"ST", "Baden-Wuerttemberg"},
      {"L", "Stuttgart"},
      {"O","Nextcloud"},
      {"CN", cnArray.constData()}
    };

    int             ret = 0;
    int             nVersion = 1;

    X509_REQ *x509_req = nullptr;
    SignPublicKeyApiJob *job = nullptr;

    // 2. set version of x509 req
    x509_req = X509_REQ_new();
    ret = X509_REQ_set_version(x509_req, nVersion);

    // 3. set subject of x509 req
    auto x509_name = X509_REQ_get_subject_name(x509_req);

    using ucharp = const unsigned char *;
    for(const auto& v : certParams) {
        ret = X509_NAME_add_entry_by_txt(x509_name, v.first,  MBSTRING_ASC, (ucharp) v.second, -1, -1, 0);
        if (ret != 1) {
            qCInfo(lcCse()) << "Error Generating the Certificate while adding" << v.first << v.second;
            X509_REQ_free(x509_req);
            return;
        }
    }

    ret = X509_REQ_set_pubkey(x509_req, keyPair);
    if (ret != 1){
        qCInfo(lcCse()) << "Error setting the public key on the csr";
        X509_REQ_free(x509_req);
        return;
    }

    ret = X509_REQ_sign(x509_req, keyPair, EVP_sha1());    // return x509_req->signature->length
    if (ret <= 0){
        qCInfo(lcCse()) << "Error setting the public key on the csr";
        X509_REQ_free(x509_req);
        return;
    }

    BIO *out = BIO_new(BIO_s_mem());
    ret = PEM_write_bio_X509_REQ(out, x509_req);
    QByteArray output = EncryptionHelper::BIO2ByteArray(out);
    BIO_free(out);
    EVP_PKEY_free(keyPair);

    qCInfo(lcCse()) << "Returning the certificate";
    qCInfo(lcCse()) << output;

    job = new SignPublicKeyApiJob(_account, baseUrl() + "public-key", this);
    job->setCsr(output);

    connect(job, &SignPublicKeyApiJob::jsonReceived, [this](const QJsonDocument& json, int retCode) {
        if (retCode == 200) {
            QString cert = json.object().value("ocs").toObject().value("data").toObject().value("public-key").toString();
            _certificate = QSslCertificate(cert.toLocal8Bit(), QSsl::Pem);
            qCInfo(lcCse()) << "Certificate saved, Encrypting Private Key.";
            encryptPrivateKey();
        }
        qCInfo(lcCse()) << retCode;
    });
    job->start();
}

void ClientSideEncryption::setTokenForFolder(const QByteArray& folderId, const QByteArray& token)
{
	_folder2token[folderId] = token;
}

QByteArray ClientSideEncryption::tokenForFolder(const QByteArray& folderId) const
{
	Q_ASSERT(_folder2token.contains(folderId));
	return _folder2token[folderId];
}

void ClientSideEncryption::encryptPrivateKey()
{
    QStringList list = WordList::getRandomWords(12);
    _mnemonic = list.join(' ');
    qCInfo(lcCse()) << "mnemonic Generated:" << _mnemonic;

    QString passPhrase = list.join(QString()).toLower();
    qCInfo(lcCse()) << "Passphrase Generated:" << passPhrase;

    auto salt = EncryptionHelper::generateRandom(40);
    auto secretKey = EncryptionHelper::generatePassword(passPhrase, salt);
    auto cryptedText = EncryptionHelper::encryptPrivateKey(secretKey, _privateKey.toPem(), salt);

    // Send private key to the server
	auto job = new StorePrivateKeyApiJob(_account, baseUrl() + "private-key", this);
    job->setPrivateKey(cryptedText);
	connect(job, &StorePrivateKeyApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
		Q_UNUSED(doc);
		switch(retCode) {
			case 200:
                qCInfo(lcCse()) << "Private key stored encrypted on server.";
                writePrivateKey();
                writeCertificate();
                writeMnemonic();
				emit initializationFinished();
				break;
			default:
				qCInfo(lcCse()) << "Store private key failed, return code:" << retCode;
		}
	});
	job->start();
}

void ClientSideEncryption::decryptPrivateKey(const QByteArray &key) {
    QString msg = tr("Please enter your end to end encryption passphrase:<br>"
                     "<br>"
                     "User: %2<br>"
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
            qCInfo(lcCse()) << "Got mnemonic:" << dialog.textValue();
            prev = dialog.textValue();

            _mnemonic = prev;
            QString mnemonic = prev.split(" ").join(QString()).toLower();
            qCInfo(lcCse()) << "mnemonic:" << mnemonic;

            // split off salt
            // Todo better place?
            auto pos = key.lastIndexOf("fA==");
            QByteArray salt = QByteArray::fromBase64(key.mid(pos + 4));
            auto key2 = key.left(pos);

            auto pass = EncryptionHelper::generatePassword(mnemonic, salt);
            qCInfo(lcCse()) << "Generated key:" << pass;

            QByteArray privateKey = EncryptionHelper::decryptPrivateKey(pass, key2);
            _privateKey = QSslKey(privateKey, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);

            qCInfo(lcCse()) << "Private key: " << _privateKey.toPem();

            if (!_privateKey.isNull()) {
                writePrivateKey();
                writeCertificate();
                writeMnemonic();
                break;
            }
        } else {
            _mnemonic = QString();
            _privateKey = QSslKey();
            qCInfo(lcCse()) << "Cancelled";
            break;
        }
    }

    emit initializationFinished();
}

void ClientSideEncryption::getPrivateKeyFromServer()
{
    qCInfo(lcCse()) << "Retrieving private key from server";
    auto job = new JsonApiJob(_account, baseUrl() + "private-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
            if (retCode == 200) {
                QString key = doc.object()["ocs"].toObject()["data"].toObject()["private-key"].toString();
                qCInfo(lcCse()) << key;
                qCInfo(lcCse()) << "Found private key, lets decrypt it!";
                decryptPrivateKey(key.toLocal8Bit());
            } else if (retCode == 404) {
                qCInfo(lcCse()) << "No private key on the server: setup is incomplete.";
            } else {
                qCInfo(lcCse()) << "Error while requesting public key: " << retCode;
            }
    });
    job->start();
}

void ClientSideEncryption::getPublicKeyFromServer()
{
    qCInfo(lcCse()) << "Retrieving public key from server";
    auto job = new JsonApiJob(_account, baseUrl() + "public-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
            if (retCode == 200) {
                QString publicKey = doc.object()["ocs"].toObject()["data"].toObject()["public-keys"].toObject()[_account->davUser()].toString();
                _certificate = QSslCertificate(publicKey.toLocal8Bit(), QSsl::Pem);
                _publicKey = _certificate.publicKey();
                qCInfo(lcCse()) << publicKey;
                qCInfo(lcCse()) << "Found Public key, requesting Private Key.";
                getPrivateKeyFromServer();
            } else if (retCode == 404) {
                qCInfo(lcCse()) << "No public key on the server";
                generateKeyPair();
            } else {
                qCInfo(lcCse()) << "Error while requesting public key: " << retCode;
            }
    });
    job->start();
}

void ClientSideEncryption::fetchFolderEncryptedStatus() {
	_refreshingEncryptionStatus = true;
	auto getEncryptedStatus = new GetFolderEncryptStatusJob(_account, QString());
	connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusReceived,
					this, &ClientSideEncryption::folderEncryptedStatusFetched);
	connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusError,
					this, &ClientSideEncryption::folderEncryptedStatusError);
	getEncryptedStatus->start();
}

void ClientSideEncryption::folderEncryptedStatusFetched(const QMap<QString, bool>& result)
{
	_refreshingEncryptionStatus = false;
	_folder2encryptedStatus = result;
	qDebug() << "Retrieved correctly the encrypted status of the folders." << result;
}

void ClientSideEncryption::folderEncryptedStatusError(int error)
{
	_refreshingEncryptionStatus = false;
	qDebug() << "Failed to retrieve the status of the folders." << error;
}

FolderMetadata::FolderMetadata(AccountPtr account, const QByteArray& metadata) : _account(account)
{
    if (metadata.isEmpty()) {
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
  // This *seems* to be broken but the RFC doesn't explicits how it wants.
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
  QByteArray sharing = metadataObj["sharing"].toString().toLocal8Bit();
  QJsonObject files = metaDataDoc.object()["files"].toObject();

  QJsonDocument debugHelper;
  debugHelper.setObject(metadataKeys);
  qDebug() << "Keys: " << debugHelper.toJson(QJsonDocument::Compact);

  // Iterate over the document to store the keys. I'm unsure that the keys are in order,
  // perhaps it's better to store a map instead of a vector, perhaps this just doesn't matter.
  for(auto it = metadataKeys.constBegin(), end = metadataKeys.constEnd(); it != end; it++) {
    QByteArray currB64Pass = it.value().toString().toLocal8Bit();
    QByteArray decryptedKey = decryptMetadataKey(currB64Pass);
    _metadataKeys.insert(it.key().toInt(), decryptedKey);
  }

  // Cool, We actually have the key, we can decrypt the rest of the metadata.
  qDebug() << "Sharing: " << sharing;
  auto sharingDecrypted = QByteArray::fromBase64(decryptJsonObject(sharing, _metadataKeys.last()));
  qDebug() << "Sharing Decrypted" << sharingDecrypted;

  //Sharing is also a JSON object, so extract it and populate.
  auto sharingDoc = QJsonDocument::fromJson(sharingDecrypted);
  auto sharingObj = sharingDoc.object();
  for (auto it = sharingObj.constBegin(), end = sharingObj.constEnd(); it != end; it++) {
    _sharing.push_back({it.key(), it.value().toString()});
  }

    for (auto it = files.constBegin(), end = files.constEnd(); it != end; it++) {
        EncryptedFile file;
        file.encryptedFilename = it.key();

        auto fileObj = it.value().toObject();
        file.metadataKey = fileObj["metadataKey"].toInt();
        file.authenticationTag = QByteArray::fromBase64(fileObj["authenticationTag"].toString().toLocal8Bit());
        file.initializationVector = QByteArray::fromBase64(fileObj["initializationVector"].toString().toLocal8Bit());

        //Decrypt encrypted part
        QByteArray key = _metadataKeys[file.metadataKey];
        auto encryptedFile = fileObj["encrypted"].toString().toLocal8Bit();
        auto decryptedFile = QByteArray::fromBase64(decryptJsonObject(encryptedFile, key));
        auto decryptedFileDoc = QJsonDocument::fromJson(decryptedFile);
        auto decryptedFileObj = decryptedFileDoc.object();

        file.originalFilename = decryptedFileObj["filename"].toString();
        file.encryptionKey = QByteArray::fromBase64(decryptedFileObj["key"].toString().toLocal8Bit());
        file.mimetype = decryptedFileObj["mimetype"].toString().toLocal8Bit();
        file.fileVersion = decryptedFileObj["version"].toInt();

        _files.push_back(file);
    }
}

// RSA/ECB/OAEPWithSHA-256AndMGF1Padding using private / public key.
QByteArray FolderMetadata::encryptMetadataKey(const QByteArray& data) const {

    BIO *publicKeyBio = BIO_new(BIO_s_mem());
    QByteArray publicKeyPem = _account->e2e()->_publicKey.toPem();
    BIO_write(publicKeyBio, publicKeyPem.constData(), publicKeyPem.size());
    EVP_PKEY *publicKey = PEM_read_bio_PUBKEY(publicKeyBio, NULL, NULL, NULL);

    // The metadata key is binary so base64 encode it first
    auto ret = EncryptionHelper::encryptStringAsymmetric(publicKey, data.toBase64());
    EVP_PKEY_free(publicKey);
    return ret; // ret is already b64
}

QByteArray FolderMetadata::decryptMetadataKey(const QByteArray& encryptedMetadata) const
{
    BIO *privateKeyBio = BIO_new(BIO_s_mem());
    QByteArray privateKeyPem = _account->e2e()->_privateKey.toPem();
    BIO_write(privateKeyBio, privateKeyPem.constData(), privateKeyPem.size());
    EVP_PKEY *key = PEM_read_bio_PrivateKey(privateKeyBio, NULL, NULL, NULL);

    // Also base64 decode the result
    return QByteArray::fromBase64(
                EncryptionHelper::decryptStringAsymmetric(
                    key,
                    QByteArray::fromBase64(encryptedMetadata)
                    )
                );
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

void FolderMetadata::setupEmptyMetadata() {
    qDebug() << "Settint up empty metadata";
    QByteArray newMetadataPass = EncryptionHelper::generateRandom(16);
    _metadataKeys.insert(0, newMetadataPass);

    QString publicKey = _account->e2e()->_publicKey.toPem().toBase64();
    QString displayName = _account->displayName();

    _sharing.append({displayName, publicKey});
}

QByteArray FolderMetadata::encryptedMetadata() {
    qDebug() << "Generating metadata";

    QJsonObject metadataKeys;
    for (auto it = _metadataKeys.constBegin(), end = _metadataKeys.constEnd(); it != end; it++) {
        const QByteArray encryptedKey = encryptMetadataKey(it.value());
        metadataKeys.insert(QString::number(it.key()), QString(encryptedKey));
    }

    QJsonObject recepients;
    for (auto it = _sharing.constBegin(), end = _sharing.constEnd(); it != end; it++) {
        recepients.insert(it->first, it->second);
    }
    QJsonDocument recepientDoc;
    recepientDoc.setObject(recepients);
    QString sharingEncrypted = encryptJsonObject(recepientDoc.toJson(QJsonDocument::Compact), _metadataKeys.last());

    QJsonObject metadata = {
      {"metadataKeys", metadataKeys},
      {"sharing", sharingEncrypted},
      {"version", 1}
    };

    QJsonObject files;
    for (auto it = _files.constBegin(), end = _files.constEnd(); it != end; it++) {
        QJsonObject encrypted;
        encrypted.insert("key", QString(it->encryptionKey.toBase64()));
        encrypted.insert("filename", it->originalFilename);
        encrypted.insert("mimetype", QString(it->mimetype));
        encrypted.insert("version", it->fileVersion);
        QJsonDocument encryptedDoc;
        encryptedDoc.setObject(encrypted);

        QString encryptedEncrypted = encryptJsonObject(encryptedDoc.toJson(QJsonDocument::Compact), _metadataKeys.last());

        QJsonObject file;
        file.insert("encrypted", encryptedEncrypted);
        file.insert("initializationVector", QString(it->initializationVector.toBase64()));
        file.insert("authenticationTag", QString(it->authenticationTag.toBase64()));
        file.insert("metadataKey", _metadataKeys.lastKey());

        files.insert(it->encryptedFilename, file);
    }

    QJsonObject metaObject = {
      {"metadata", metadata},
      {"files", files}
    };

    QJsonDocument internalMetadata;
    internalMetadata.setObject(metaObject);
    return internalMetadata.toJson();
}

void FolderMetadata::addEncryptedFile(const EncryptedFile &f) {
    // TODO: check for duplicates
    _files.append(f);
}

QVector<EncryptedFile> FolderMetadata::files() const {
    return _files;
}

bool ClientSideEncryption::isFolderEncrypted(const QString& path) {
  auto it = _folder2encryptedStatus.find(path);
  if (it == _folder2encryptedStatus.end())
    return false;
  return (*it);
}

void EncryptionHelper::fileEncryption(const QByteArray &key, const QByteArray &iv, QFile *input, QFile *output)
{
    if (!input->open(QIODevice::ReadOnly)) {
      qDebug() << "Could not open input file for reading" << input->errorString();
    }
    if (!output->open(QIODevice::WriteOnly)) {
      qDebug() << "Could not oppen output file for writting" << output->errorString();
    }

    // Init
    EVP_CIPHER_CTX *ctx;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        qCInfo(lcCse()) << "Could not create context";
        exit(-1);
    }

    /* Initialise the decryption operation. */
    if(!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
        qCInfo(lcCse()) << "Could not init cipher";
        exit(-1);
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), NULL)) {
        qCInfo(lcCse()) << "Could not set iv length";
        exit(-1);
    }

    /* Initialise key and IV */
    if(!EVP_EncryptInit_ex(ctx, NULL, NULL, (const unsigned char *)key.constData(), (const unsigned char *)iv.constData())) {
        qCInfo(lcCse()) << "Could not set key and iv";
        exit(-1);
    }

    unsigned char *out = (unsigned char *)malloc(sizeof(unsigned char) * (1024 + 16 -1));
    int len = 0;
    int total_len = 0;

    qDebug() << "Starting to encrypt the file" << input->fileName() << input->atEnd();
    while(!input->atEnd()) {
        QByteArray data = input->read(1024);

        if (data.size() == 0) {
            qCInfo(lcCse()) << "Could not read data from file";
            exit(-1);
        }

        qDebug() << "Encrypting " << data;
        if(!EVP_EncryptUpdate(ctx, out, &len, (unsigned char *)data.constData(), data.size())) {
            qCInfo(lcCse()) << "Could not encrypt";
            exit(-1);
        }

        output->write((char *)out, len);
        total_len += len;
    }

    if(1 != EVP_EncryptFinal_ex(ctx, out, &len)) {
        qCInfo(lcCse()) << "Could finalize encryption";
        exit(-1);
    }
    output->write((char *)out, len);
    total_len += len;

    /* Get the tag */
    unsigned char *tag = (unsigned char *)malloc(sizeof(unsigned char) * 16);
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
        qCInfo(lcCse()) << "Could not get tag";
        exit(-1);
    }

    output->write((char *)tag, 16);

    free(out);
    free(tag);
    EVP_CIPHER_CTX_free(ctx);

    input->close();
    output->close();
    qDebug() << "File Encrypted Successfully";
}

FileDecryptionJob::FileDecryptionJob(QByteArray &key, QByteArray &iv, QFile *input, QFile *output, QObject *parent)
    : QObject(parent),
      _key(key),
      _iv(iv),
      _input(input),
      _output(output)
{
}

void FileDecryptionJob::start()
{
    _input->open(QIODevice::ReadOnly);
    _output->open(QIODevice::WriteOnly);

    // Init
    EVP_CIPHER_CTX *ctx;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        qCInfo(lcCse()) << "Could not create context";
        exit(-1);
    }

    /* Initialise the decryption operation. */
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
        qCInfo(lcCse()) << "Could not init cipher";
        exit(-1);
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /* Set IV length. */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, _iv.size(), NULL)) {
        qCInfo(lcCse()) << "Could not set iv length";
        exit(-1);
    }

    /* Initialise key and IV */
    if(!EVP_DecryptInit_ex(ctx, NULL, NULL, (const unsigned char *)_key.constData(), (const unsigned char *)_iv.constData())) {
        qCInfo(lcCse()) << "Could not set key and iv";
        exit(-1);
    }

    qint64 size = _input->size() - 16;

    unsigned char *out = (unsigned char *)malloc(sizeof(unsigned char) * (1024 + 16 -1));
    int len = 0;

    while(_input->pos() < size) {

        int toRead = size - _input->pos();
        if (toRead > 1024) {
            toRead = 1024;
        }

        QByteArray data = _input->read(toRead);

        if (data.size() == 0) {
            qCInfo(lcCse()) << "Could not read data from file";
            exit(-1);
        }

        if(!EVP_DecryptUpdate(ctx, out, &len, (unsigned char *)data.constData(), data.size())) {
            qCInfo(lcCse()) << "Could not decrypt";
            exit(-1);
        }

        _output->write((char *)out, len);
    }

    QByteArray tag = _input->read(16);

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(), (unsigned char *)tag.constData())) {
        qCInfo(lcCse()) << "Could not set expected tag";
        exit(-1);
    }

    if(1 != EVP_DecryptFinal_ex(ctx, out, &len)) {
        qCInfo(lcCse()) << "Could finalize decryption";
        exit(-1);
    }
    _output->write((char *)out, len);

    free(out);
    EVP_CIPHER_CTX_free(ctx);

    _input->close();
    _output->close();

    emit finished(_output);
}

}
