#ifndef CLIENTSIDEENCRYPTION_H
#define CLIENTSIDEENCRYPTION_H

#include <QString>
#include <QObject>
#include <QJsonDocument>


#include "accountfwd.h"

namespace OCC {


class ClientSideEncryption : public QObject {
    Q_OBJECT
public:
    ClientSideEncryption();
    void initialize();
    void setAccount(AccountPtr account);
    bool hasPrivateKey() const;
    bool hasPublicKey() const;
    void generateKeyPair();
    QString generateSCR();
    void getPrivateKeyFromServer();
    void getPublicKeyFromServer();
    void signPublicKey();
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
