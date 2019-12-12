#include "syncwrapper.h"
#include "socketapi.h"
#include <qdir.h>
#include "networkjobs.h"

#include <qtextcodec.h>

namespace OCC {

//enum csync_instructions_e {
//    CSYNC_INSTRUCTION_NONE = 0x00000000, /* Nothing to do (UPDATE|RECONCILE) */
//    CSYNC_INSTRUCTION_EVAL = 0x00000001, /* There was changed compared to the DB (UPDATE) */
//    CSYNC_INSTRUCTION_EVAL_RENAME = 0x00000800, /* The file is new, it is the destination of a rename (UPDATE) */
//    CSYNC_INSTRUCTION_NEW = 0x00000008, /* The file is new compared to the db (UPDATE) */
//    CSYNC_INSTRUCTION_IGNORE = 0x00000020, /* The file is ignored (UPDATE|RECONCILE) */

//    CSYNC_INSTRUCTION_UPDATE_METADATA = 0x00000400, /* If the etag has been updated and need to be writen to the db,
//                                                      but without any propagation (UPDATE|RECONCILE) */

//    CSYNC_INSTRUCTION_SYNC = 0x00000040, /* The file need to be pushed to the other remote (RECONCILE) */
//    CSYNC_INSTRUCTION_STAT_ERROR = 0x00000080,
//    CSYNC_INSTRUCTION_ERROR = 0x00000100,
//    CSYNC_INSTRUCTION_TYPE_CHANGE = 0x00000200, /* Like NEW, but deletes the old entity first (RECONCILE)
//                                                      Used when the type of something changes from directory to file
//                                                      or back. */
//    CSYNC_INSTRUCTION_CONFLICT = 0x00000010, /* The file need to be downloaded because it is a conflict (RECONCILE) */
//    CSYNC_INSTRUCTION_REMOVE = 0x00000002, /* The file need to be removed (RECONCILE) */
//    CSYNC_INSTRUCTION_RENAME = 0x00000004, /* The file need to be renamed (RECONCILE) */
//};

Q_LOGGING_CATEGORY(lcSyncWrapper, "nextcloud.gui.wrapper", QtInfoMsg);

SyncWrapper *SyncWrapper::instance()
{
    static SyncWrapper instance;
    return &instance;
}

}
