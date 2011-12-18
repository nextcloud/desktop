#ifndef OWNPASSWORDMANAGER_H
#define OWNPASSWORDMANAGER_H

#include "SyncGlobal.h"

#include <QMainWindow>
#include <QObject>

#ifdef Q_OS_LINUX
// We have two choices for linux (kwallet,gnome keyring or I suppose, none)

#if defined(OCS_USE_KWALLET)
#include <kwallet.h>
#endif

#if defined(OCS_USE_GNOME_KEYRING)
#endif

#endif // Q_OS_LINUX

#ifdef Q_OS_MAC_OS_X
#endif // Q_OS_MAC_OS_X

#ifdef Q_OS_WIN
#endif // Q_OS_WIN

class OwnPasswordManager : public QObject
{
    Q_OBJECT
public:
    OwnPasswordManager(QObject *parent = 0, WId winId = 0);
    bool savePassword(QString name, QString pass);
    QString getPassword(QString name);

private:
#ifdef Q_OS_LINUX
    enum PasswordManagerLinux {
        NONE,
        KWALLET,
        GNOME_KEYRING
    };
    PasswordManagerLinux mPasswordManagerLinux;

#if defined(OCS_USE_KWALLET)
    KWallet::Wallet *mKWallet;
    void kwalletOpened(bool);
#endif

#if defined(OCS_USE_GNOME_KEYRING)
#endif

#endif // Q_OS_LINUX

#ifdef Q_OS_MAC_OS_X
#endif // Q_OS_MAC_OS_X

#ifdef Q_OS_WIN
#endif // Q_OS_WIN

public slots:
    void slotKWalletOpened(bool);

signals:
    void managerReady();

};

#endif // OWNPASSWORDMANAGER_H
