#ifndef CLIENTSIDEENCRYPTION_H
#define CLIENTSIDEENCRYPTION_H

#include <QString>
#include <QObject>
#include <QJsonDocument>

#include <openssl/rsa.h>
#include <openssl/evp.h>

#include "accountfwd.h"

namespace OCC {
QString baseUrl();
QString baseDirectory();

class ClientSideEncryption : public QObject {
    Q_OBJECT
public:
    ClientSideEncryption();
    void initialize();
    void setAccount(AccountPtr account);
    bool hasPrivateKey() const;
    bool hasPublicKey() const;
    void generateKeyPair();
    QString generateCSR(EVP_PKEY *keyPair);
    void getPrivateKeyFromServer();
    void getPublicKeyFromServer();
    void encryptPrivateKey(EVP_PKEY *keyPair);
    QString privateKeyPath() const;
    QString publicKeyPath() const;

signals:
    void initializationFinished();

private:
    OCC::AccountPtr _account;
    bool isInitialized = false;
};

}

#endif
