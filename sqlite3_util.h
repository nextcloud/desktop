/******************************************************************************
  * This header is placed in the public domain by Juan Carlos Cornejo Nov 10,
  * 2011
*******************************************************************************/
#ifndef SQLITE3_UTIL_H
#define SQLITE3_UTIL_H

#include <QSqlDatabase>

namespace sqlite3_util {
    bool sqliteDBMemFile( QSqlDatabase memdb, QString filename, bool save );
}
#endif
