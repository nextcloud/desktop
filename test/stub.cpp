// stub to prevent linker error
#include "accountmanager.h"
OCC::AccountManager *OCC::AccountManager::instance() { return 0; }
void OCC::AccountManager::saveAccountState(AccountState *) { }
void OCC::AccountManager::save(bool saveCredentials) { Q_UNUSED(saveCredentials); }
