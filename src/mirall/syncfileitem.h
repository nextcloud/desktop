#ifndef SYNCFILEITEM_H
#define SYNCFILEITEM_H

#include <QVector>

#include <csync.h>

namespace Mirall {

// FIXME: Unhack this.
class SyncFileItem {
public:
    typedef enum {
        None = 0,
        Up,
        Down } Direction;

    SyncFileItem() {}

    bool operator==(const SyncFileItem& item) const {
        return item._file == this->_file;
    }

    bool isEmpty() const {
        return _file.isEmpty();
    }

    // variables
    QString _file;
    QString _renameTarget;
    csync_instructions_e _instruction;
    Direction _dir;
};

typedef QVector<SyncFileItem> SyncFileItemVector;

}

#endif // SYNCFILEITEM_H
