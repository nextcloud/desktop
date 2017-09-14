#include "clientsideencryption.h"
#include "account.h"
#include "capabilities.h"
#include "networkjobs.h"

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <map>
#include <string>

#include <cstdio>

#include <QDebug>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>

#include "wordlist.h"

namespace OCC
{

Q_LOGGING_CATEGORY(lcCse, "sync.clientsideencryption", QtInfoMsg)

QString baseUrl = QStringLiteral("ocs/v2.php/apps/client_side_encryption/api/v1/");
QString baseDirectory = QDir::homePath() + QStringLiteral("/.nextcloud-keys/");

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
    return baseDirectory + _account->displayName() + ".pub";
}

QString ClientSideEncryption::privateKeyPath() const
{
    return baseDirectory + _account->displayName() + ".rsa";
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
    if (!dir.mkpath(baseDirectory)) {
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

    job = new SignPublicKeyApiJob(_account, baseUrl + "public-key", this);
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

    static const char* salt = "$4$YmBjm3hk$Qb74D5IUYwghUmzsMqeNFx5z0/8$";
    static const int iterationCount = 1024;
    static const int keyStrength = 256;
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
}

void ClientSideEncryption::getPrivateKeyFromServer()
{

}

void ClientSideEncryption::getPublicKeyFromServer()
{
    qCInfo(lcCse()) << "Retrieving public key from server";
    auto job = new JsonApiJob(_account, baseUrl + "public-key", this);
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

void ClientSideEncryption::signPublicKey()
{

}

}
