// stub to prevent linker error
#include "accountmanager.h"
OCC::AccountManager *OCC::AccountManager::instance() { static QObject dummy; return reinterpret_cast<AccountManager *>(&dummy); }
void OCC::AccountManager::saveAccountState(AccountState *) { }
void OCC::AccountManager::save(bool saveCredentials) { Q_UNUSED(saveCredentials); }
void OCC::AccountManager::accountRemoved(OCC::AccountState*) { }
const QMetaObject OCC::AccountManager::staticMetaObject = QObject::staticMetaObject;
