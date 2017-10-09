// stub to prevent linker error
#include "accountmanager.h"
OCC::AccountManager *OCC::AccountManager::instance() { return static_cast<AccountManager *>(new QObject); }
void OCC::AccountManager::saveAccountState(AccountState *) { }
void OCC::AccountManager::save(bool saveCredentials) { Q_UNUSED(saveCredentials); }
void OCC::AccountManager::accountRemoved(OCC::AccountState*) { }
const QMetaObject OCC::AccountManager::staticMetaObject = QObject::staticMetaObject;
