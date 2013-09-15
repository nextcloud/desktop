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
      File,
      Directory,
      SoftLink
    } Type;

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
    QString _errorString;
    csync_instructions_e _instruction;
    Direction _dir;
    Type      _type;
};

typedef QVector<SyncFileItem> SyncFileItemVector;

}

#endif // SYNCFILEITEM_H
