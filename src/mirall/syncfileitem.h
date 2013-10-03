/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef SYNCFILEITEM_H
#define SYNCFILEITEM_H

#include <QVector>
#include <QString>
#include <QMetaType>

#include <csync.h>

namespace Mirall {

// FIXME: Unhack this.
class SyncFileItem {
public:
    typedef enum {
      None = 0,
      Up,
      Down } Direction;

    typedef enum {
      UnknownType,
      File = CSYNC_FTW_TYPE_FILE,
      Directory = CSYNC_FTW_TYPE_DIR,
      SoftLink = CSYNC_FTW_TYPE_SLINK
    } Type;

    SyncFileItem() {}

    friend bool operator==(const SyncFileItem& item1, const SyncFileItem& item2) {
        return item1._file == item2._file;
    }

    friend bool operator<(const SyncFileItem& item1, const SyncFileItem& item2) {
        // Delete at the end:
        if (item1._instruction == CSYNC_INSTRUCTION_REMOVE && item2._instruction != CSYNC_INSTRUCTION_REMOVE)
            return false;
        if (item1._instruction != CSYNC_INSTRUCTION_REMOVE && item2._instruction == CSYNC_INSTRUCTION_REMOVE)
            return true;

        // Sort by destination
        return item1.destination() < item2.destination();
    }

    QString destination() const {
        return _instruction == CSYNC_INSTRUCTION_RENAME ? _renameTarget : _file;
    }



    bool isEmpty() const {
        return _file.isEmpty();
    }

    // variables
    QString _file;
    QString _renameTarget;
    QByteArray _originalFile; // as it is in the csync tree
    csync_instructions_e _instruction;
    Direction _dir;
    bool _isDirectory;
    time_t _modtime;
    QByteArray _etag;
    quint64  _size;

    QString _errorString;
    int     _httpCode;

    Type      _type;
};



typedef QVector<SyncFileItem> SyncFileItemVector;

}

Q_DECLARE_METATYPE(Mirall::SyncFileItem)

#endif // SYNCFILEITEM_H
