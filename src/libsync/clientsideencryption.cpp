#include "clientsideencryption.h"
#include "account.h"
#include "capabilities.h"

#include <QDebug>
#include <QLoggingCategory>

namespace OCC
{

Q_LOGGING_CATEGORY(lcCse, "sync.connectionvalidator", QtInfoMsg)

QString baseUrl = QStringLiteral("ocs/v2.php/apps/client_side_encryption/api/v1/");

ClientSideEncryption::ClientSideEncryption(Account *parent) : _account(parent)
{
}

void OCC::ClientSideEncryption::initialize()
{
    if (!_account->capabilities().clientSideEncryptionAvaliable()) {
        qCInfo(lcCse()) << "No client side encryption, do not initialize anything.";
        emit initializationFinished();
    }

    fetchPrivateKey();
}

void ClientSideEncryption::fetchPrivateKey()
{
    qCInfo(lcCse()) << "Client side encryption enabled, trying to retrieve the key.";
}

}
