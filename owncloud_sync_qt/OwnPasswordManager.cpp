#include "OwnPasswordManager.h"

#include <QProcess>

OwnPasswordManager::OwnPasswordManager(QObject *parent, WId winID )
    : QObject(parent)
{
    mIsReady = false;
#ifdef Q_OS_LINUX
    QProcess p;
    QStringList args;
    args.push_back("ksmserver");
    p.start("pidof",args);
    p.waitForFinished(-1);
    if(p.readAllStandardOutput()!="") {
        mPasswordManagerLinux = KWALLET;
    } else {
        args.clear();
        args.push_back("gnome-session");
        p.start("pidof", args);
        p.waitForFinished(-1);
        if(p.readAllStandardOutput()!="") {
            mPasswordManagerLinux = GNOME_KEYRING;
        }
    }
#if defined(OCS_USE_KWALLET)
    if(mPasswordManagerLinux == KWALLET ) {
        mKWallet= KWallet::Wallet::openWallet(
                    KWallet::Wallet::NetworkWallet(),
                    winID,KWallet::Wallet::Asynchronous);
        connect(mKWallet, SIGNAL(walletOpened(bool)),
                this,SLOT(slotKWalletOpened(bool)));
    }
#endif // OCS_USE_KWALLET
    if(mPasswordManagerLinux != KWALLET ) {
        mIsReady = true;
        emit managerReady();
    }
#endif // Q_OS_LINUX

#ifdef Q_OS_MAC_OS_X
    mIsReady = true;
#endif // Q_OS_MAC_OS_X

#ifdef Q_OS_WIN
    mIsReady = true;
#endif // Q_OS_WIN
}

bool OwnPasswordManager::savePassword(QString name, QString pass)
{
#ifdef Q_OS_LINUX

#if defined(OCS_USE_KWALLET)
    if(mPasswordManagerLinux == KWALLET) {
        QMap<QString,QString> map;
        map[name] = pass;
        if( mKWallet ) {
            mKWallet->writeMap(name,map);
            return true;
        }
    }
#endif

#if defined(OCS_USE_GNOME_KEYRING)
#endif

#endif // Q_OS_LINUX

#ifdef Q_OS_MAC_OS_X
#endif // Q_OS_MAC_OS_X

#ifdef Q_OS_WIN
#endif // Q_OS_WIN

    return false; // Which means password could not be stored, store in DB
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
    return QString(""); // If empty, means read from DB
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
        mIsReady = true;
        emit managerReady();
            //emit toLog("Wallet opened!");
            syncDebug() << "Wallet opened!";
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
