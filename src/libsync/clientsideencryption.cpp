#include "clientsideencryption.h"
#include "account.h"
#include "capabilities.h"
#include "networkjobs.h"

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include <map>
#include <string>

#include <cstdio>

#include <QDebug>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>

#include <nlohmann/json.hpp>
#include "wordlist.h"

namespace OCC
{

Q_LOGGING_CATEGORY(lcCse, "sync.clientsideencryption", QtInfoMsg)
Q_LOGGING_CATEGORY(lcSignPublicKeyApiJob, "sync.networkjob.sendcsr", QtInfoMsg);
Q_LOGGING_CATEGORY(lcStorePrivateKeyApiJob, "sync.networkjob.storeprivatekey", QtInfoMsg);
Q_LOGGING_CATEGORY(lcCseJob, "sync.networkjob.clientsideencrypt", QtInfoMsg);

QString baseUrl(){
    return QStringLiteral("ocs/v2.php/apps/end_to_end_encryption/api/v1/");
}

QString baseDirectory() {
    return QDir::homePath() + QStringLiteral("/.nextcloud-keys/");
}

namespace {
    void handleErrors(void)
    {
        ERR_print_errors_fp(stdout); // This line is not printing anything.
        fflush(stdout);
    }

    int encrypt(unsigned char *plaintext,
                int plaintext_len,
                unsigned char *key,
                unsigned char *iv,
                unsigned char *ciphertext,
                unsigned char *tag)
    {
        EVP_CIPHER_CTX *ctx;
        int len;
        int ciphertext_len;

        /* Create and initialise the context */
        if(!(ctx = EVP_CIPHER_CTX_new())) {
			qCInfo(lcCse()) << "Error creating the Cipher.";
            handleErrors();
        }

        /* Initialise the encryption operation. */
        if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
			qCInfo(lcCse()) << "Error initializing the context with aes_256";
            handleErrors();
        }

        // We don't do padding
        EVP_CIPHER_CTX_set_padding(ctx, 0);

        /* Set IV length to 16 bytes */
        if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL)) {
            qCInfo(lcCse()) << "Error setting the iv length to 16 bytes. ";
            handleErrors();
        }

        /* Initialise key and IV */
        if(1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) {
			qCInfo(lcCse()) << "Error initializing encryption";
            handleErrors();
        }

        /* Provide the message to be encrypted, and obtain the encrypted output.
        * EVP_EncryptUpdate can be called multiple times if necessary
        */
        if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
			qCInfo(lcCse()) << "Error encrypting the cipher ext"; // Current error is here.
            handleErrors();
        }
        ciphertext_len = len;

        /* Finalise the encryption. Normally ciphertext bytes may be written at
        * this stage, but this does not occur in GCM mode
        */
        if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
			qCInfo(lcCse()) << "Error finalizing the encryption";
            handleErrors();
        }
        ciphertext_len += len;

        /* Get the tag */
        if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
			qCInfo(lcCse()) << "Error Retrieving the tag";
            handleErrors();
        }

        /* Add tag to cypher text to be compatible with the Android implementation */
        memcpy(ciphertext + ciphertext_len, tag, 16);
        ciphertext_len += 16;

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        return ciphertext_len;
    }

    int decrypt(unsigned char *ciphertext,
                int ciphertext_len,
                unsigned char *tag,
                unsigned char *key,
                unsigned char *iv,
                unsigned char *plaintext)
    {
        EVP_CIPHER_CTX *ctx;
        int len;
        int plaintext_len;
        int ret;

        /* Create and initialise the context */
        if(!(ctx = EVP_CIPHER_CTX_new())) {
			qCInfo(lcCse()) << "Error Initializing the decrypt context";
            handleErrors();
        }

        /* Initialise the decryption operation. */
        if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
			qCInfo(lcCse()) << "Error initializing the decryption context";
            handleErrors();
        }

        /* Set IV length to 16 bytes */
        if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL)) {
			qCInfo(lcCse()) << "Error seting th iv length for the decrypt context";
            handleErrors();
        }

        /* Initialise key and IV */
        if(!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) {
			qCInfo(lcCse()) << "Error setting the key and iv for decryption";
            handleErrors();
        }

       /* Provide the message to be decrypted, and obtain the plaintext output.
        * EVP_DecryptUpdate can be called multiple times if necessary
        *
        * Do not try to decrypt the last 16 bytes. The tag is appended by Android.
        * So we ignore the last 16 bytes.
        */
        if(!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len - 16)) {
			qCInfo(lcCse()) << "Error decrypting the text";
            handleErrors();
        }
        plaintext_len = len;

        /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
        if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag)) {
			qCInfo(lcCse()) << "Error setting the tag on the decrupt context";
            handleErrors();
        }

        /* Finalise the decryption. A positive return value indicates success,
        * anything else is a failure - the plaintext is not trustworthy.
        */
        ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        if(ret > 0)
        {
            /* Success */
            plaintext_len += len;
            return plaintext_len;
        }
        else
        {
			qCInfo(lcCse()) << "Error finalizing the decrypt";
            /* Verify failed */
            return -1;
        }
    }
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

    if (hasPrivateKey() && hasPublicKey()) {
        qCInfo(lcCse()) << "Public and private keys already downloaded";
        emit initializationFinished();
    }

    getPublicKeyFromServer();
}

QString ClientSideEncryption::publicKeyPath() const
{
    return baseDirectory() + _account->displayName() + ".pub";
}

QString ClientSideEncryption::privateKeyPath() const
{
    return baseDirectory() + _account->displayName() + ".rsa";
}

bool ClientSideEncryption::hasPrivateKey() const
{
    return QFileInfo(privateKeyPath()).exists();
}

bool ClientSideEncryption::hasPublicKey() const
{
    return QFileInfo(publicKeyPath()).exists();
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

    QDir dir;
    if (!dir.mkpath(baseDirectory())) {
        qCInfo(lcCse()) << "Could not create the folder for the keys.";
        return;
    }

    auto privKeyPath = privateKeyPath().toLocal8Bit();
    auto pubKeyPath = publicKeyPath().toLocal8Bit();
    FILE *privKeyFile = fopen(privKeyPath.constData(), "w");
    FILE *pubKeyFile = fopen(pubKeyPath.constData(), "w");

    qCInfo(lcCse()) << "Private key filename" << privKeyPath;
    qCInfo(lcCse()) << "Public key filename" << pubKeyPath;

    //TODO: Verify if the key needs to be stored with a Cipher and Pass.
    if (!PEM_write_PrivateKey(privKeyFile, localKeyPair, NULL, NULL, 0, 0, NULL)) {
        qCInfo(lcCse()) << "Could not write the private key to a file.";
        return;
    }

    if (!PEM_write_PUBKEY(pubKeyFile, localKeyPair)) {
        qCInfo(lcCse()) << "Could not write the public key to a file.";
        return;
    }

    fclose(privKeyFile);
    fclose(pubKeyFile);

    generateCSR(localKeyPair);

    //TODO: Send to server.
    qCInfo(lcCse()) << "Keys generated correctly, sending to server.";
}

QString ClientSideEncryption::generateCSR(EVP_PKEY *keyPair)
{
    // OpenSSL expects const char.
    auto certParams = std::map<const char *, const char*>{
      {"C", "DE"},
      {"ST", "Baden-Wuerttemberg"},
      {"L", "Stuttgart"},
      {"O","Nextcloud"},
      {"CN", "www.nextcloud.com"}
    };

    int             ret = 0;
    int             nVersion = 1;

    X509_REQ *x509_req = nullptr;
    auto out = BIO_new(BIO_s_mem());
    QByteArray output;
    char data[80];
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
            goto free_all;
        }
    }

    ret = X509_REQ_set_pubkey(x509_req, keyPair);
    if (ret != 1){
        qCInfo(lcCse()) << "Error setting the public key on the csr";
        goto free_all;
    }

    ret = X509_REQ_sign(x509_req, keyPair, EVP_sha1());    // return x509_req->signature->length
    if (ret <= 0){
        qCInfo(lcCse()) << "Error setting the public key on the csr";
        goto free_all;
    }

    ret = PEM_write_bio_X509_REQ(out, x509_req);
    do {
        ret = BIO_gets(out, data, 80);
        output += data;
        if (output.endsWith("-----END CERTIFICATE REQUEST-----")) {
            output = output.trimmed();
            break;
        }
    } while (ret > 0 );

    qCInfo(lcCse()) << "Returning the certificate";
    qCInfo(lcCse()) << output;

    job = new SignPublicKeyApiJob(_account, baseUrl() + "public-key", this);
    job->setCsr(output);

    connect(job, &SignPublicKeyApiJob::jsonReceived, [this, keyPair](const QJsonDocument& json, int retCode) {
        if (retCode == 200) {
            auto caps = json.object().value("ocs").toObject().value("data").toObject().value("public-key").toString();
            qCInfo(lcCse()) << "Public Key Returned" << caps;
            QFile file(publicKeyPath() + ".sign");
            if (file.open(QIODevice::WriteOnly)) {
                QTextStream s(&file);
                s << caps;
            }
            file.close();
            qCInfo(lcCse()) << "public key saved, Encrypting Private Key.";
            encryptPrivateKey(keyPair);
        }
        qCInfo(lcCse()) << retCode;
    });
    job->start();

free_all:
    X509_REQ_free(x509_req);
    BIO_free_all(out);
    return "";
}

void ClientSideEncryption::encryptPrivateKey(EVP_PKEY *keyPair)
{
    // Write the Private File to a BIO
    // Retrieve the BIO contents, and encrypt it.
    // Send the encrypted key to the server.
    // I have no idea what I'm doing.

    using ucharp = unsigned char *;
    const char *salt = "$4$YmBjm3hk$Qb74D5IUYwghUmzsMqeNFx5z0/8$";
    const int saltLen = 40;
    const int iterationCount = 1024;
    const int keyStrength = 256;
    BIO* bio = BIO_new(BIO_s_mem());

    QString passPhrase = WordList::getUnifiedString(WordList::getRandomWords(12));
    const char* passPhrasePtr = qPrintable(passPhrase);
    qCInfo(lcCse()) << "Passphrase Generated:";
    qCInfo(lcCse()) << passPhrase;

    // Extract the Private key from the key pair.
    PEM_write_bio_PrivateKey(bio, keyPair, NULL, NULL, 0, 0, NULL);
    char data[80];
    QString output;
    int ret = 0;
    do {
        ret = BIO_gets(bio, data, 80);
        output += data;
        if (output.endsWith("-----END PRIVATE KEY-----")) {
            output = output.trimmed();
            break;
        }
    } while (ret > 0 );

    qCInfo(lcCse()) << "Private Key Extracted";
    qCInfo(lcCse()) << output;

    /* Jesus. the OpenSSL docs do not help at all.
     * This PKCS5_PBKDF2_HMAC_SHA1 call will generate
     * a new password from the password that was submited.
     */
    unsigned char secretKey[keyStrength];

    ret = PKCS5_PBKDF2_HMAC_SHA1(
        passPhrasePtr,     // const char *password,
        passPhrase.size(), // int password length,
        (ucharp) salt,     // const unsigned char *salt,
        saltLen,      // int saltlen,
        iterationCount,    // int iterations,
        keyStrength,       // int keylen,
        secretKey          // unsigned char *out
    );
    qCInfo(lcCse()) << "Return of the PKCS5" << ret;
    qCInfo(lcCse()) << "Result String" << secretKey;

    //: FIRST TRY A SILLY PHRASE.
    //: Hardcoed IV, really bad.
    unsigned char *fakepass = (unsigned char*) "qwertyuiasdfghjkzxcvbnm,qwertyui";
    unsigned char *iv = (unsigned char *)"0123456789012345";
    unsigned char encryptTag[16];


    const char *encryptTest = "a quick brown fox jumps over the lazy dog";
    // TODO: Find a way to
    unsigned char cryptedText[128];
    unsigned char decryptedText[128];
    unsigned char tag[16];
    int cryptedText_len = encrypt(
        (unsigned char*) encryptTest,   //unsigned char *plaintext,
        strlen(encryptTest),            //        int plaintext_len,
        fakepass,                       //        unsigned char *key,
        iv,                             //        unsigned char *iv,
        cryptedText,                    //        unsigned char *ciphertext,
        tag                             //        unsigned char *tag
    );
/*
    qCInfo(lcCse()) << "Encrypted Text" << QByteArray( (const char*) cryptedText, cryptedText_len);
    int decryptedText_len = decrypt(
        cryptedText,     //unsigned char *ciphertext,
        cryptedText_len, //int ciphertext_len,
        NULL,            //unsigned char *aad,
        0,               //int aad_len,
        tag,             //unsigned char *tag,
        fakepass,       //unsigned char *key,
        iv,              //unsigned char *iv,
        decryptedText    //unsigned char *plaintext
    );
    qCInfo(lcCse()) << "Decrypted Text" << QByteArray( (const char*) decryptedText, decryptedText_len);
*/
// Pretend that the private key is actually encrypted and send it to the server.
	auto job = new StorePrivateKeyApiJob(_account, baseUrl() + "private-key", this);
	job->setPrivateKey(QByteArray((const char*) cryptedText, 128));
	connect(job, &StorePrivateKeyApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
		switch(retCode) {
			case 200:
				qCInfo(lcCse()) << "Store private key working as expected.";
				emit initializationFinished();
				break;
			default:
				qCInfo(lcCse()) << "Store private key failed, return code:" << retCode;
		}
	});
	job->start();
}

void ClientSideEncryption::getPrivateKeyFromServer()
{
	qCInfo(lcCse()) << "Trying to store the private key on the server.";
}

void ClientSideEncryption::getPublicKeyFromServer()
{
    qCInfo(lcCse()) << "Retrieving public key from server";
    auto job = new JsonApiJob(_account, baseUrl() + "public-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
        switch(retCode) {
            case 404: // no public key
                qCInfo(lcCse()) << "No public key on the server";
                generateKeyPair();
                break;
            case 400: // internal error
                qCInfo(lcCse()) << "Internal server error while requesting the public key, encryption aborted.";
                break;
            case 200: // ok
                qCInfo(lcCse()) << "Found Public key, requesting Private Key.";
                break;
        }
    });
    job->start();
}


SignPublicKeyApiJob::SignPublicKeyApiJob(const AccountPtr& account, const QString& path, QObject* parent)
: AbstractNetworkJob(account, path, parent)
{
}

void SignPublicKeyApiJob::setCsr(const QByteArray& csr)
{
    QByteArray data = "csr=";
    data += QUrl::toPercentEncoding(csr);
    _csr.setData(data);
}

void SignPublicKeyApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QList<QPair<QString, QString>> params = {
        qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json"))
    };
    url.setQueryItems(params);

    qCInfo(lcSignPublicKeyApiJob) << "Sending the CSR" << _csr.data();
    sendRequest("POST", url, req, &_csr);
    AbstractNetworkJob::start();
}

bool SignPublicKeyApiJob::finished()
{
    qCInfo(lcStorePrivateKeyApiJob()) << "Sending CSR ended with"  << path() << errorString() << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
}


StorePrivateKeyApiJob::StorePrivateKeyApiJob(const AccountPtr& account, const QString& path, QObject* parent)
: AbstractNetworkJob(account, path, parent)
{
}

void StorePrivateKeyApiJob::setPrivateKey(const QByteArray& privKey)
{
    QByteArray data = "privateKey=";
    data += QUrl::toPercentEncoding(privKey);
    _privKey.setData(data);
}

void StorePrivateKeyApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QList<QPair<QString, QString>> params = {
        qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json"))
    };
    url.setQueryItems(params);

    qCInfo(lcStorePrivateKeyApiJob) << "Sending the private key" << _privKey.data();
    sendRequest("POST", url, req, &_privKey);
    AbstractNetworkJob::start();
}

bool StorePrivateKeyApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200)
        qCInfo(lcStorePrivateKeyApiJob()) << "Sending private key ended with"  << path() << errorString() << retCode;

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
}

SetEncryptionFlagApiJob::SetEncryptionFlagApiJob(const AccountPtr& account, const QString& fileId, QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("encrypted/") + fileId, parent), _fileId(fileId)
{
}

void SetEncryptionFlagApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QList<QPair<QString, QString>> params = {
        qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json"))
    };
    url.setQueryItems(params);

    qCInfo(lcCseJob()) << "marking the file with id" << _fileId << "as encrypted";
    sendRequest("PUT", url, req);
    AbstractNetworkJob::start();
}

bool SetEncryptionFlagApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200)
        qCInfo(lcCseJob()) << "Setting the encrypted flag failed with" << path() << errorString() << retCode;

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
}

/* Test metdata:
{
        // Metadata about the share
        "metadata": {
                // Encryption algorithm: RSA/ECB/OAEPWithSHA-256AndMGF1Padding, encrypted via private/public key (asymmetric)
                "metadataKeys": {
                        "0": "OLDESTMETADATAKEY",
                        "2": "…",
                        "3": "NEWESTMETADATAKEY"
                },
                // Encryption algorithm: AES/GCM/NoPadding (128 bit key size)  with metadata key from above (symmetric)
                "sharing": {
                        // Name of recipients as well as public keys of the recipients
                        "recipient": {
                                "recipient1@example.com": "PUBLIC KEY",
                                "recipient2@example.com": "PUBLIC KEY"
                        },
                },
                "version": 1
        },
        // A JSON blob referencing all files
        "files": {
                "ia7OEEEyXMoRa1QWQk8r": {
                        // Encryption algorithm: AES/GCM/NoPadding (128 bit key size)  with metadata key from above (symmetric)
                        "encrypted": {
                                "key": "jtboLmgGR1OQf2uneqCVHpklQLlIwWL5TXAQ0keK",
                                "filename": "/foo/test.txt",
                                "mimetype": "plain/text",
                                "version": 1
                        },
                        "initializationVector": "+mHu52HyZq+pAAIN",
                        "authenticationTag": "GCM authentication tag",
                        "metadataKey": 1
                }
        }
}

*/
FolderMetadata::FolderMetadata(const QByteArray& metadata)
{
    // This is a new folder
    /*
    if (metadata.isEmpty()) {

    }
    QJsonParseError err;
    _doc = QJsonDocument::fromJson(metadata, err);
    */

}

}
