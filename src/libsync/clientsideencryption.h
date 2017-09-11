#ifndef CLIENTSIDEENCRYPTION_H
#define CLIENTSIDEENCRYPTION_H

#include <QString>
#include <QObject>

namespace OCC {

class Account;

class ClientSideEncryption : public QObject {
    Q_OBJECT
public:
    ClientSideEncryption(OCC::Account *parent);
    void initialize();

    void fetchPrivateKey();
signals:
    void initializationFinished();

private:
    OCC::Account *_account;
    bool isInitialized = false;
};

}

#endif
