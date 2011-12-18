#include "OwnPasswordManager.h"

OwnPasswordManager::OwnPasswordManager(QObject *parent, WId winID )
    : QObject(parent)
{
#ifdef Q_OS_LINUX
#if defined(OCS_USE_KWALLET)
    mKWallet= KWallet::Wallet::openWallet(
                KWallet::Wallet::NetworkWallet(),
                winID,KWallet::Wallet::Asynchronous);
    connect(mKWallet, SIGNAL(walletOpened(bool)),
            this,SLOT(slotKWalletOpened(bool)));
#endif // OCS_USE_KWALLET

#endif // Q_OS_LINUX
}

void OwnPasswordManager::savePassword(QString name, QString pass)
{
#ifdef Q_OS_LINUX

#if defined(OCS_USE_KWALLET)
    QMap<QString,QString> map;
    map[name] = pass;
    if( mKWallet )
        mKWallet->writeMap(name,map);
#endif

#if defined(OCS_USE_GNOME_KEYRING)
#endif

#endif // Q_OS_LINUX

#ifdef Q_OS_MAC_OS_X
#endif // Q_OS_MAC_OS_X

#ifdef Q_OS_WIN
#endif // Q_OS_WIN
}

QString OwnPasswordManager::getPassword(QString name)
{
#ifdef Q_OS_LINUX

#if defined(OCS_USE_KWALLET)
    QMap<QString,QString> map;
    mKWallet->readMap(name,map);
    if(map.size()) {
        return map[name];
    }
#endif

#if defined(OCS_USE_GNOME_KEYRING)
#endif

#endif // Q_OS_LINUX

#ifdef Q_OS_MAC_OS_X
#endif // Q_OS_MAC_OS_X

#ifdef Q_OS_WIN
#endif // Q_OS_WIN
    return QString("");
}

void OwnPasswordManager::slotKWalletOpened(bool ok)
{
#ifdef Q_OS_LINUX
#if defined(OCS_USE_KWALLET)
    kwalletOpened(ok);
#endif
#endif
}


#ifdef Q_OS_LINUX

#if defined(OCS_USE_KWALLET)
void OwnPasswordManager::kwalletOpened(bool ok)
{
    if( ok && (mKWallet->hasFolder("owncloud_sync")
         || mKWallet->createFolder("owncloud_sync"))
         && mKWallet->setFolder("owncloud_sync") ){
        emit managerReady();
            //emit toLog("Wallet opened!");
            //syncDebug() << "Wallet opened!" <<
            //           KWallet::Wallet::FormDataFolder() ;
            //if (mReadPassword ) {
            //    requestPassword();
            //}
    } else {
        syncDebug() << "Error opening wallet";
    }
}

#endif // OCS_USE_KWALLET

#if defined(OCS_USE_GNOME_KEYRING)
#endif // OCS_USE_GNOME_KEYRING

#endif // Q_OS_LINUX

#ifdef Q_OS_MAC_OS_X
#endif // Q_OS_MAC_OS_X

#ifdef Q_OS_WIN
#endif // Q_OS_WIN
