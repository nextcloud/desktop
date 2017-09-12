#include "clientsideencryption.h"
#include "account.h"
#include "capabilities.h"
#include "networkjobs.h"

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <cstdio>

#include <QDebug>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>

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
    EVP_PKEY *serverPub = nullptr;

    EVP_CIPHER_CTX *rsaEncryptCtx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX *aesEncryptCtx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX *rsaDecryptCtx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX *aesDecryptCtx = EVP_CIPHER_CTX_new();

    unsigned char *aesKey = nullptr;
    unsigned char *aesIv = nullptr;

    for(auto ctx : {rsaEncryptCtx, aesEncryptCtx, rsaDecryptCtx, aesDecryptCtx}) {
        if (!ctx) {
            qCInfo(lcCse()) << "Failed to generate the cripto context";
            return;
        }
    }

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

    //TODO: Send to server.
    qCInfo(lcCse()) << "Keys generated correctly, sending to server.";
}

QString ClientSideEncryption::generateCSR()
{
    return {};
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
