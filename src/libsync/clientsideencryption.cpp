#include "clientsideencryption.h"
#include "account.h"
#include "capabilities.h"
#include "networkjobs.h"

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

}

QString ClientSideEncryption::generateSCR()
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
                qCInfo(lcCse()) << "No public key, generating a pair.";
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
